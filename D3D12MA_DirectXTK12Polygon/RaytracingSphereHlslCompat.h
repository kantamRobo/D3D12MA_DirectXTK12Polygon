#pragma once

// Shared C++/HLSL constant buffer and vertex structures for sphere raytracing.
// In HLSL context define HLSL before including this file.

#ifdef HLSL
typedef float2   XMFLOAT2;
typedef float3   XMFLOAT3;
typedef float4   XMFLOAT4;
typedef float4   XMVECTOR;
typedef float4x4 XMMATRIX;
#else
using namespace DirectX;
typedef UINT16 Index;
#endif

// Constant buffer shared across all shaders: camera and lighting data.
struct SceneConstantBuffer
{
    XMMATRIX projectionToWorld; // Inverse of ViewProjection matrix for ray generation
    XMVECTOR cameraPosition;    // World-space camera position
    XMVECTOR lightPosition;     // World-space light position
    XMVECTOR lightAmbientColor; // Ambient light color (RGBA)
    XMVECTOR lightDiffuseColor; // Diffuse light color (RGBA)
};

// Per-object constant buffer: material properties of the sphere.
struct SphereConstantBuffer
{
    XMFLOAT4 albedo; // Base diffuse color (RGBA)
};

// Vertex layout for sphere geometry (position + normal, no texcoords needed for DXR).
struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT3 normal;
};
