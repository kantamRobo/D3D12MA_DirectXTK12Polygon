// AssimpRay.hlsl
// DirectX Raytracing shader library for Assimp-loaded triangle mesh rendering with Lambert reflection.
// Compile target: lib_6_3 or later
//
// Example:
// dxc.exe -T lib_6_3 -Fo AssimpRay.cso AssimpRay.hlsl

// -----------------------------------------------------------------------------
// Constant buffers
// -----------------------------------------------------------------------------

struct SceneConstantBuffer
{
    // Inverse of ViewProjection matrix for ray generation.
    float4x4 projectionToWorld;

    // xyz = camera world position, w = unused
    float4 cameraPosition;

    // xyz = light world position, w = unused
    float4 lightPosition;

    // rgba = ambient light color
    float4 lightAmbientColor;

    // rgba = diffuse light color
    float4 lightDiffuseColor;
};

// ランバート反射用定数バッファ (register b1)
cbuffer LambertCB : register(b1)
{
    float4 g_lightDir;       // 光の進行方向 (xyz)
    float4 g_lightColor;     // ライトカラー (rgb)
    float4 g_materialColor;  // マテリアルカラー (rgb)
    float4 g_ambientColor;   // 環境光カラー (rgb)
};

// -----------------------------------------------------------------------------
// Vertex layout
// -----------------------------------------------------------------------------
// DirectXTK12 VertexPositionNormalColorTexture と一致 (48 bytes)
struct Vertex
{
    float3 position;
    float3 normal;
    float4 color;
    float2 texCoord;
};

// -----------------------------------------------------------------------------
// Global resources
// -----------------------------------------------------------------------------
// t0 : TLAS
// u0 : output texture
// t1 : index buffer as ByteAddressBuffer
// t2 : vertex buffer as StructuredBuffer<Vertex>
// b0 : scene constant buffer
// b1 : lambert constant buffer

RaytracingAccelerationStructure Scene : register(t0);
RWTexture2D<float4>             RenderTarget : register(u0);

ByteAddressBuffer        Indices : register(t1);
StructuredBuffer<Vertex> Vertices : register(t2);

ConstantBuffer<SceneConstantBuffer> g_sceneCB : register(b0);

// -----------------------------------------------------------------------------
// Helper: load three packed 16-bit indices from a ByteAddressBuffer.
// -----------------------------------------------------------------------------
uint3 Load3x16BitIndices(uint offsetBytes)
{
    uint3 indices;
    const uint dwordAlignedOffset = offsetBytes & ~3u;
    const uint2 four16BitIndices = Indices.Load2(dwordAlignedOffset);

    if (dwordAlignedOffset == offsetBytes)
    {
        indices.x = four16BitIndices.x & 0xffff;
        indices.y = (four16BitIndices.x >> 16) & 0xffff;
        indices.z = four16BitIndices.y & 0xffff;
    }
    else
    {
        indices.x = (four16BitIndices.x >> 16) & 0xffff;
        indices.y = four16BitIndices.y & 0xffff;
        indices.z = (four16BitIndices.y >> 16) & 0xffff;
    }

    return indices;
}

// -----------------------------------------------------------------------------
// DXR payload / attributes
// -----------------------------------------------------------------------------

typedef BuiltInTriangleIntersectionAttributes MyAttributes;

struct RayPayload
{
    float4 color;
};

// -----------------------------------------------------------------------------
// Camera ray generation
// -----------------------------------------------------------------------------

inline void GenerateCameraRay(
    uint2 index,
    out float3 origin,
    out float3 direction
)
{
    float2 xy = index + 0.5f;

    float2 screenPos =
        xy / DispatchRaysDimensions().xy * 2.0f - 1.0f;

    // DirectXの画面座標は左上原点なのでYを反転
    screenPos.y = -screenPos.y;

    // projectionToWorld は ViewProjection の逆行列
    float4 world = mul(
        float4(screenPos, 0.0f, 1.0f),
        g_sceneCB.projectionToWorld
    );

    world.xyz /= world.w;

    origin = g_sceneCB.cameraPosition.xyz;
    direction = normalize(world.xyz - origin);
}

// -----------------------------------------------------------------------------
// Ray Generation Shader
// -----------------------------------------------------------------------------

[shader("raygeneration")]
void MyRaygenShader()
{
    float3 rayDirection;
    float3 rayOrigin;

    GenerateCameraRay(
        DispatchRaysIndex().xy,
        rayOrigin,
        rayDirection
    );

    RayDesc ray;
    ray.Origin = rayOrigin;
    ray.Direction = rayDirection;
    ray.TMin = 0.001f;
    ray.TMax = 10000.0f;

    RayPayload payload;
    payload.color = float4(0.0f, 0.0f, 0.0f, 0.0f);

    TraceRay(
        Scene,
        RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
        ~0u,
        0,
        1,
        0,
        ray,
        payload
    );

    RenderTarget[DispatchRaysIndex().xy] = payload.color;
}

// -----------------------------------------------------------------------------
// Closest Hit Shader (ランバート反射 & 重心座標法線補間)
// -----------------------------------------------------------------------------

[shader("closesthit")]
void MyClosestHitShader(
    inout RayPayload payload,
    in MyAttributes attr
)
{
    uint primIndex = PrimitiveIndex();

    // 16ビットインデックスバッファから3頂点インデックスを取得
    const uint indexSizeInBytes = 2;
    const uint indicesPerTriangle = 3;
    const uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;

    const uint baseIndex = primIndex * triangleIndexStride;
    const uint3 indices = Load3x16BitIndices(baseIndex);

    // 各頂点の法線を取得
    float3 n0 = Vertices[indices.x].normal;
    float3 n1 = Vertices[indices.y].normal;
    float3 n2 = Vertices[indices.z].normal;

    // 重心座標による補間
    float u = attr.barycentrics.x;
    float v = attr.barycentrics.y;
    float w = 1.0f - u - v;
    float3 localNormal = normalize(w * n0 + u * n1 + v * n2);

    // ワールド空間法線への変換
    float3 worldNormal = normalize(mul((float3x3)ObjectToWorld3x4(), localNormal));

    // ランバート拡散反射光の算出
    // 光源の逆ベクトル L = normalize(-g_lightDir.xyz)
    float3 L = normalize(-g_lightDir.xyz);
    float NdotL = saturate(dot(worldNormal, L));
    float3 diffuse = g_lightColor.rgb * g_materialColor.rgb * NdotL;
    float3 ambient = g_ambientColor.rgb * g_materialColor.rgb;

    payload.color = float4(diffuse + ambient, 1.0f);
}

// -----------------------------------------------------------------------------
// Miss Shader
// -----------------------------------------------------------------------------

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
    // 背景色（暗めのダークグレーブルー）
    payload.color = float4(0.08f, 0.08f, 0.12f, 1.0f);
}
