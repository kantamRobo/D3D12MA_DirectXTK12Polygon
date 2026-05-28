#pragma once

#ifdef HLSL

struct SceneConstantBuffer
{
    float4x4 projectionToWorld;
    float4 cameraPosition;
    float4 lightPosition;
    float4 lightAmbientColor;
    float4 lightDiffuseColor;
};

struct SphereConstantBuffer
{
    float4 albedo;
};

struct Vertex
{
    float3 position;
    float3 normal;
};

#else
# include "RaytracingSphereHlslCompat.h"
    namespace DirectX
{

    typedef UINT16 Index;

    struct SceneConstantBuffer
    {
        XMMATRIX projectionToWorld;
        XMVECTOR cameraPosition;
        XMVECTOR lightPosition;
        XMVECTOR lightAmbientColor;
        XMVECTOR lightDiffuseColor;
    };

    struct SphereConstantBuffer
    {
        XMFLOAT4 albedo;
    };

    struct Vertex
    {
        XMFLOAT3 position;
        XMFLOAT3 normal;
    };
}
#endif