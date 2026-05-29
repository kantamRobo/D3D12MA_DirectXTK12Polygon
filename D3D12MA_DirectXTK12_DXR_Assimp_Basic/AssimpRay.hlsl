// AssimpRay.hlsl
// DirectX Raytracing shader library for Assimp-loaded triangle mesh rendering.
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

    // xyz = camera world position
    // w   = unused
    float4 cameraPosition;

    // xyz = light world position
    // w   = unused
    float4 lightPosition;

    // rgba = ambient light color
    float4 lightAmbientColor;

    // rgba = diffuse light color
    float4 lightDiffuseColor;
};

struct SphereConstantBuffer
{
    // rgba = base diffuse color
    float4 albedo;
};

// -----------------------------------------------------------------------------
// Vertex layout
// -----------------------------------------------------------------------------
//
// C++側の頂点構造体も、このレイアウトに合わせる。
// 例:
//
// struct Vertex
// {
//     DirectX::XMFLOAT3 position;
//     DirectX::XMFLOAT3 normal;
// };
//
// StructuredBuffer<Vertex> として読むため、C++側のStrideは sizeof(Vertex)。

struct Vertex
{
    float3 position;
    float3 normal;
};

// -----------------------------------------------------------------------------
// Global resources
// -----------------------------------------------------------------------------
//
// ルートシグネチャ側のレジスタ番号と一致させること。
//
// t0 : TLAS
// u0 : output texture
// t1 : index buffer as ByteAddressBuffer
// t2 : vertex buffer as StructuredBuffer<Vertex>
// b0 : scene constant buffer
// b1 : material/object constant buffer

RaytracingAccelerationStructure Scene : register(t0);
RWTexture2D<float4>             RenderTarget : register(u0);

ByteAddressBuffer        Indices : register(t1);
StructuredBuffer<Vertex> Vertices : register(t2);

ConstantBuffer<SceneConstantBuffer>  g_sceneCB  : register(b0);
ConstantBuffer<SphereConstantBuffer> g_sphereCB : register(b1);

// -----------------------------------------------------------------------------
// Helper: load three packed 16-bit indices from a ByteAddressBuffer.
// -----------------------------------------------------------------------------
//
// IndexBufferが DXGI_FORMAT_R16_UINT の前提。
// 3つの16bit indexを、ByteAddressBufferから取り出す。

uint3 Load3x16BitIndices(uint offsetBytes)
{
    uint3 indices;

    // ByteAddressBuffer::Load / Load2 は4byte alignmentが必要。
    const uint dwordAlignedOffset = offsetBytes & ~3u;

    // 4つ分の16bit indexを2つのuintとして読む。
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
// Hit helpers
// -----------------------------------------------------------------------------

float3 HitWorldPosition()
{
    return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
}

float3 HitAttribute(
    float3 vertexAttribute[3],
    BuiltInTriangleIntersectionAttributes attr
)
{
    return vertexAttribute[0]
        + attr.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0])
        + attr.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}

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

    // DirectXの画面座標は左上原点なのでYを反転。
    screenPos.y = -screenPos.y;

    // projectionToWorld は ViewProjection の逆行列。
    float4 world = mul(
        float4(screenPos, 0.0f, 1.0f),
        g_sceneCB.projectionToWorld
    );

    world.xyz /= world.w;

    origin = g_sceneCB.cameraPosition.xyz;
    direction = normalize(world.xyz - origin);
}

// -----------------------------------------------------------------------------
// Lighting
// -----------------------------------------------------------------------------
/*
float4 CalculateDiffuseLighting(
    float3 hitPosition,
    float3 normal
)
{
    float3 pixelToLight =
        normalize(g_sceneCB.lightPosition.xyz - hitPosition);

    float nDotL =
        max(0.0f, dot(pixelToLight, normal));

    return g_sphereCB.albedo
        * g_sceneCB.lightDiffuseColor
        * nDotL;
}
*/
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
// Closest Hit Shader
// -----------------------------------------------------------------------------

[shader("closesthit")]
void MyClosestHitShader(
    inout RayPayload payload,
    in MyAttributes attr
)
{
    float3 hitPosition = HitWorldPosition();

    // IndexBufferが uint16_t / DXGI_FORMAT_R16_UINT の前提。
    const uint indexSizeInBytes = 2;
    const uint indicesPerTriangle = 3;
    const uint triangleIndexStride =
        indicesPerTriangle * indexSizeInBytes;

    const uint baseIndex =
        PrimitiveIndex() * triangleIndexStride;

    const uint3 indices =
        Load3x16BitIndices(baseIndex);

    float3 vertexNormals[3] =
    {
        Vertices[indices.x].normal,
        Vertices[indices.y].normal,
        Vertices[indices.z].normal
    };

    float3 triangleNormal =
        normalize(HitAttribute(vertexNormals, attr));
/*
    float4 diffuseColor =
        CalculateDiffuseLighting(hitPosition, triangleNormal);
*/
    float4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

    
    

    payload.color = color;
}

// -----------------------------------------------------------------------------
// Miss Shader
// -----------------------------------------------------------------------------

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
    //赤色
    payload.color = float4(1.0f, 0.0f, 0.0f, 1.0f);
}