// glTFRay.hlsl
// DirectX Raytracing shader library for glTF-loaded triangle mesh rendering.
// Compile target: lib_6_3 or later
//
// Example:
// dxc.exe -T lib_6_3 -Fo glTFRay.cso glTFRay.hlsl

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
// Matches DirectX::VertexPositionNormalColorTexture (48 bytes)
//
struct Vertex
{
    float3 position;
    float3 normal;
    float4 color;
    float2 textureCoordinate;
};

// -----------------------------------------------------------------------------
// Global resources
// -----------------------------------------------------------------------------
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
// Helper: load three 32-bit indices from a ByteAddressBuffer.
// -----------------------------------------------------------------------------

uint3 Load3x32BitIndices(uint offsetBytes)
{
    return Indices.Load3(offsetBytes);
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

    // IndexBuffer は uint32_t / DXGI_FORMAT_R32_UINT
    const uint indexSizeInBytes = 4;
    const uint indicesPerTriangle = 3;
    const uint triangleIndexStride =
        indicesPerTriangle * indexSizeInBytes;

    const uint baseIndex =
        PrimitiveIndex() * triangleIndexStride;

    const uint3 indices =
        Load3x32BitIndices(baseIndex);

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
    // 赤色
    payload.color = float4(1.0f, 0.0f, 0.0f, 1.0f);
}