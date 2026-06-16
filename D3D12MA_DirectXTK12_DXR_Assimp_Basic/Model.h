#pragma once
#include <BufferHelpers.h>
#include <vector>
#include <DirectXMath.h>
#include <VertexTypes.h>
#include <DeviceResources.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

// DXC shader compiler API (Windows SDK 10.0.17134+)
// If your SDK is older, copy dxcapi.h from the DirectX Shader Compiler release.
#include <atlbase.h>        // Common COM helpers.
#include <dxcapi.h>         // Be sure to link with dxcompiler.lib.
#include <d3d12shader.h>    // Shader reflection.
#include "D3D12MemAlloc.h"
using namespace DirectX;

// Convenience macro: size of obj in UINT32 units (rounded up).
#define SizeOfInUint32(obj) ((sizeof(obj) - 1) / sizeof(UINT32) + 1)

// Align value up to the given alignment (must be a power of two).
#define ALIGN_UP(value, alignment) (((UINT64)(value) + (alignment) - 1) & ~((UINT64)(alignment) - 1))
// Per-object constant buffer: material properties of the sphere.
struct SphereConstantBuffer
{
    XMFLOAT4 albedo; // Base diffuse color (RGBA)
};

// ---------------------------------------------------------------------------
// Global root-signature parameter slots.
// Must match the HLSL register bindings in RaytracingSphere.hlsl.
// ---------------------------------------------------------------------------
namespace SphereRaytracing
{
    namespace GlobalRootSignatureParams
    {
        enum Value
        {
            OutputViewSlot = 0,  // UAV  u0  - raytracing output texture
            AccelerationStructure = 1,  // SRV  t0  - top-level acceleration structure
            SceneConstant = 2,  // CBV  b0  - SceneConstantBuffer
            VertexBuffers = 3,  // SRV  t1,t2 - index + vertex buffers
            SphereConstant = 4,  // CBV  b1  - SphereConstantBuffer
            Count
        };
    }

    // Descriptor heap layout
    namespace DescriptorIndex
    {
        enum Value
        {
            RaytracingOutputUAV = 0,
            IndexBufferSRV = 1,
            VertexBufferSRV = 2,
            Count
        };
    }
}

// Constant buffer shared across all shaders: camera and lighting data.
struct SceneConstantBuffer
{
    XMMATRIX projectionToWorld; // Inverse of ViewProjection matrix for ray generation
    XMVECTOR cameraPosition;    // World-space camera position
    XMVECTOR lightPosition;     // World-space light position
    XMVECTOR lightAmbientColor; // Ambient light color (RGBA)
    XMVECTOR lightDiffuseColor; // Diffuse light color (RGBA)
};

class Model
{
public:
    ~Model();
    Model() {};
   
    // ---------------------------------------------------------------------------
// Shader entry-point / hit-group names (must match RaytracingSphere.hlsl)
// ---------------------------------------------------------------------------
    const wchar_t* c_raygenShaderName = L"MyRaygenShader";
    const wchar_t* c_closestHitShaderName = L"MyClosestHitShader";
    const wchar_t* c_missShaderName = L"MyMissShader";
    const wchar_t* c_hitGroupName = L"MyHitGroup";


    // -----------------------------------------------------------------------
    // DXR interfaces (obtained via QueryInterface from the base device/list)
    // -----------------------------------------------------------------------
    Microsoft::WRL::ComPtr<ID3D12Device5>              m_dxrDevice;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> m_dxrCommandList;
    Microsoft::WRL::ComPtr<ID3D12StateObject>          m_dxrStateObject;

    // Root signature (global; no local root signature used)
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_globalRootSignature;

    // CBV/SRV/UAV descriptor heap
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;
    UINT m_descriptorSize = 0;
    UINT m_descriptorsAllocated = 0;

    // Scene and sphere constant buffers (persistently mapped upload heap)
    SceneConstantBuffer  m_sceneCBData = {};
    SphereConstantBuffer m_sphereCBData = {};
    Microsoft::WRL::ComPtr<ID3D12Resource> m_sceneCBResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_sphereCBResource;

    // Geometry (vertex/index buffers via D3D12MA)
    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
    UINT m_vertexCount = 0;
    UINT m_indexCount = 0;

    // D3D12MA allocator + allocations for geometry buffers
    D3D12MA::Allocator* m_allocator = nullptr;
    D3D12MA::Allocation* m_vertexAllocation = nullptr;
    D3D12MA::Allocation* m_indexAllocation = nullptr;

    // Acceleration structures (committed resources)
    Microsoft::WRL::ComPtr<ID3D12Resource> m_blasScratch;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_blasResult;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_tlasScratch;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_tlasResult;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_tlasInstanceDescs;

    // Raytracing output UAV texture
    Microsoft::WRL::ComPtr<ID3D12Resource> m_raytracingOutput;
    D3D12_GPU_DESCRIPTOR_HANDLE            m_raytracingOutputUAVGpuDescriptor = {};
    

    Microsoft::WRL::ComPtr<ID3D12Resource> m_rayGenShaderTable;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_missShaderTable;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_hitGroupShaderTable;

    // Back-buffer dimensions
    UINT m_width = 800;
    UINT m_height = 600;
	std::vector<VertexPositionNormalColorTexture> vertices;
    std::vector<uint16_t> indices;
    bool LoadModel(const char* path);
	
    std::vector<DirectX::VertexPositionNormalColorTexture> GenerateVertices(const char* path);
    //Model.cppÇ≈íËã`Ç≥ÇÍÇΩä÷êîÇÃêÈåæ
	void BuildGeometry(DX::DeviceResources* DR);
    void LoadAssets(DX::DeviceResources* DR, const char* filePath);
	void CreateConstantBuffers(ID3D12Device* device);
    void LoadCompiledShaderLibrary(LPCWSTR csoPath, void** ppBytecode, SIZE_T* pBytecodeSize, std::vector<BYTE>& outBlob);
    void CreateRaytracingPipelineStateObject(DX::DeviceResources* DR);
	void CreateGlobalRootSignature(ID3D12Device* device);
	void CreateDescriptorHeap(ID3D12Device* device);
	void CompileDXRShaderLibrary(
		LPCWSTR shaderPath,
		void** ppBytecode,
		SIZE_T* pBytecodeSize,
		std::vector<BYTE>& outBlob);
	void BuildAccelerationStructures(DX::DeviceResources* DR);
	void CreateRaytracingOutputResource(DX::DeviceResources* DR);
	void BuildShaderTables(ID3D12Device* device);
	void UpdateSceneConstants(DX::DeviceResources* DR);
    void Render(DX::DeviceResources* DR);
    void CopyRaytracingOutputToBackbuffer(DX::DeviceResources* DR);
    // Allocate a descriptor from the heap; returns the heap index.
    UINT AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor,
        UINT descriptorIndexToUse = UINT_MAX);
    void CreateUAVBuffer(
        ID3D12Device* device, UINT64 bufferSize, ID3D12Resource** ppResource,
        D3D12_RESOURCE_STATES initialState, const wchar_t* name);
    void CreateUploadBuffer(
        ID3D12Device* device, void* pData, UINT64 dataSize,
        ID3D12Resource** ppResource, const wchar_t* name);
    void PopulateCommandList(DX::DeviceResources* DR);
    void OnWindowSizeChanged(UINT width, UINT height, DX::DeviceResources* DR);
    void CreateRaytracingInterfaces(DX::DeviceResources* DR);
    void LoadAssets(DX::DeviceResources* DR);
    
};

