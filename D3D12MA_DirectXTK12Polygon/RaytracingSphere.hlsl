// RaytracingSphere.hlsl
// DirectX Raytracing shader library for sphere rendering.
// Compiled with DXC to DXIL lib_6_3 target.

#ifndef RAYTRACING_SPHERE_HLSL
#define RAYTRACING_SPHERE_HLSL

#define HLSL
#include "RaytracingSphereHlslCompat.h"

// Global root signature bindings (slot ordering matches D3D12RaytracingSphere.cpp):
//   slot 0: UAV output texture        u0
//   slot 1: TLAS (inline SRV)         t0
//   slot 2: Scene constant buffer     b0
//   slot 3: Index + Vertex buffers    t1, t2
//   slot 4: Sphere constant buffer    b1

RaytracingAccelerationStructure   Scene        : register(t0, space0);
RWTexture2D<float4>               RenderTarget : register(u0);
ByteAddressBuffer                 Indices      : register(t1, space0);
StructuredBuffer<Vertex>          Vertices     : register(t2, space0);

ConstantBuffer<SceneConstantBuffer>  g_sceneCB  : register(b0);
ConstantBuffer<SphereConstantBuffer> g_sphereCB : register(b1);

// ---------------------------------------------------------------------------
// Helper: load three packed 16-bit indices from a ByteAddressBuffer.
// ---------------------------------------------------------------------------
uint3 Load3x16BitIndices(uint offsetBytes)
{
    uint3 indices;
    // Loads must be 4-byte aligned, so handle the two possible alignments.
    const uint dwordAlignedOffset = offsetBytes & ~3u;
    const uint2 four16BitIndices  = Indices.Load2(dwordAlignedOffset);

    if (dwordAlignedOffset == offsetBytes)
    {
        indices.x =  four16BitIndices.x        & 0xffff;
        indices.y = (four16BitIndices.x >> 16) & 0xffff;
        indices.z =  four16BitIndices.y        & 0xffff;
    }
    else
    {
        indices.x = (four16BitIndices.x >> 16) & 0xffff;
        indices.y =  four16BitIndices.y        & 0xffff;
        indices.z = (four16BitIndices.y >> 16) & 0xffff;
    }
    return indices;
}

typedef BuiltInTriangleIntersectionAttributes MyAttributes;

struct RayPayload
{
    float4 color;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
float3 HitWorldPosition()
{
    return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
}

// Barycentric interpolation over a triangle attribute.
float3 HitAttribute(float3 vertexAttribute[3], BuiltInTriangleIntersectionAttributes attr)
{
    return vertexAttribute[0]
        + attr.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0])
        + attr.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}

// Generate a perspective camera ray for the dispatched pixel index.
inline void GenerateCameraRay(uint2 index, out float3 origin, out float3 direction)
{
    float2 xy = index + 0.5f;
    float2 screenPos = xy / DispatchRaysDimensions().xy * 2.0f - 1.0f;
    // Invert Y for DirectX-style (top-left origin) screen coordinates.
    screenPos.y = -screenPos.y;

    // Unproject the screen position into world space using the inverse VP matrix.
    float4 world = mul(float4(screenPos, 0.0f, 1.0f), g_sceneCB.projectionToWorld);
    world.xyz /= world.w;

    origin    = g_sceneCB.cameraPosition.xyz;
    direction = normalize(world.xyz - origin);
}

// Simple Lambertian diffuse lighting.
float4 CalculateDiffuseLighting(float3 hitPosition, float3 normal)
{
    float3 pixelToLight = normalize(g_sceneCB.lightPosition.xyz - hitPosition);
    float  fNDotL       = max(0.0f, dot(pixelToLight, normal));
    return g_sphereCB.albedo * g_sceneCB.lightDiffuseColor * fNDotL;
}

// ---------------------------------------------------------------------------
// Ray Generation Shader
// ---------------------------------------------------------------------------
[shader("raygeneration")]
void MyRaygenShader()
{
    float3 rayDir;
    float3 origin;
    GenerateCameraRay(DispatchRaysIndex().xy, origin, rayDir);

    RayDesc ray;
    ray.Origin    = origin;
    ray.Direction = rayDir;
    ray.TMin      = 0.001f;
    ray.TMax      = 10000.0f;

    RayPayload payload = { float4(0, 0, 0, 0) };
    TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0u, 0, 1, 0, ray, payload);

    RenderTarget[DispatchRaysIndex().xy] = payload.color;
}

// ---------------------------------------------------------------------------
// Closest Hit Shader
// ---------------------------------------------------------------------------
[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
    float3 hitPosition = HitWorldPosition();

    // Fetch the three 16-bit indices for the hit triangle.
    const uint indexSizeInBytes      = 2;
    const uint indicesPerTriangle    = 3;
    const uint triangleIndexStride   = indicesPerTriangle * indexSizeInBytes;
    const uint baseIndex             = PrimitiveIndex() * triangleIndexStride;
    const uint3 indices              = Load3x16BitIndices(baseIndex);

    // Retrieve per-vertex normals and interpolate.
    float3 vertexNormals[3] = {
        Vertices[indices[0]].normal,
        Vertices[indices[1]].normal,
        Vertices[indices[2]].normal
    };
    float3 triangleNormal = HitAttribute(vertexNormals, attr);

    // Combine ambient and diffuse contributions.
    float4 diffuseColor = CalculateDiffuseLighting(hitPosition, triangleNormal);
    float4 color        = g_sceneCB.lightAmbientColor + diffuseColor;
    payload.color = color;
}

// ---------------------------------------------------------------------------
// Miss Shader
// ---------------------------------------------------------------------------
[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
    // Background: dark blue gradient.
    payload.color = float4(0.0f, 0.1f, 0.25f, 1.0f);
}

#endif // RAYTRACING_SPHERE_HLSL
