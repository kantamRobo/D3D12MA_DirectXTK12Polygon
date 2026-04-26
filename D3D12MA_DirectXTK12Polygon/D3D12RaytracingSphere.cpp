// D3D12RaytracingSphere.cpp
// Integrates DirectX Raytracing (DXR) with DirectXTK12's GeometricPrimitive
// to render a raytraced sphere using D3D12MA for geometry buffer allocation.

#include "pch.h"
#include "D3D12RaytracingSphere.h"

// DirectXTK12 geometry
#include <GeometricPrimitive.h>

// DXC shader compiler API (Windows SDK 10.0.17134+)
// If your SDK is older, copy dxcapi.h from the DirectX Shader Compiler release.
#include <atlbase.h>        // Common COM helpers.
#include <dxcapi.h>         // Be sure to link with dxcompiler.lib.
#include <d3d12shader.h>    // Shader reflection.
using namespace DirectX;
using namespace Microsoft::WRL;

// ---------------------------------------------------------------------------
// Shader entry-point / hit-group names (must match RaytracingSphere.hlsl)
// ---------------------------------------------------------------------------
const wchar_t* D3D12RaytracingSphere::c_raygenShaderName      = L"MyRaygenShader";
const wchar_t* D3D12RaytracingSphere::c_closestHitShaderName  = L"MyClosestHitShader";
const wchar_t* D3D12RaytracingSphere::c_missShaderName        = L"MyMissShader";
const wchar_t* D3D12RaytracingSphere::c_hitGroupName          = L"MyHitGroup";

// ---------------------------------------------------------------------------
// Destructor - release D3D12MA allocations
// ---------------------------------------------------------------------------
D3D12RaytracingSphere::~D3D12RaytracingSphere()
{
    if (m_vertexAllocation) { m_vertexAllocation->Release(); m_vertexAllocation = nullptr; }
    if (m_indexAllocation)  { m_indexAllocation->Release();  m_indexAllocation  = nullptr; }
    if (m_allocator)        { m_allocator->Release();        m_allocator        = nullptr; }
}

// ===========================================================================
// LoadAssets – one-time setup called from CreateDeviceDependentResources().
// ===========================================================================
void D3D12RaytracingSphere::LoadAssets(DX::DeviceResources* DR)
{
    auto device = DR->GetD3DDevice();

    // Release any existing D3D12MA resources (supports re-initialization after device loss).
    if (m_vertexAllocation) { m_vertexAllocation->Release(); m_vertexAllocation = nullptr; }
    if (m_indexAllocation)  { m_indexAllocation->Release();  m_indexAllocation  = nullptr; }
    if (m_allocator)        { m_allocator->Release();        m_allocator        = nullptr; }
    m_dxrDevice.Reset();
    m_dxrCommandList.Reset();
    m_dxrStateObject.Reset();
    m_globalRootSignature.Reset();

    // Cache output dimensions from the current back-buffer size.
    auto outputSize = DR->GetOutputSize();
    m_width  = static_cast<UINT>(outputSize.right  - outputSize.left);
    m_height = static_cast<UINT>(outputSize.bottom - outputSize.top);
    if (m_width  == 0) m_width  = 800;
    if (m_height == 0) m_height = 600;

    // Verify DXR support.
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 opts5 = {};
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &opts5, sizeof(opts5)))
        || opts5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
    {
#ifdef _DEBUG
        OutputDebugStringA("ERROR: DirectX Raytracing is not supported by this device.\n");
#endif
        throw std::runtime_error("DirectX Raytracing is not supported by this device.");
    }

    // Create the D3D12MA allocator (for geometry buffers).
    {
        D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
        allocatorDesc.pDevice  = device;
        allocatorDesc.pAdapter = static_cast<IDXGIAdapter*>(DR->adapter.Get());
        allocatorDesc.Flags    = D3D12MA_RECOMMENDED_ALLOCATOR_FLAGS;
        DX::ThrowIfFailed(D3D12MA::CreateAllocator(&allocatorDesc, &m_allocator));
    }

    // Obtain DXR-capable interfaces.
    CreateRaytracingInterfaces(DR);

    // Build the global root signature.
    CreateGlobalRootSignature(device);

    // Compile HLSL → DXIL and create the raytracing PSO.
    CreateRaytracingPipelineStateObject(DR);

    // Create the CBV/SRV/UAV descriptor heap.
    CreateDescriptorHeap(device);

    // Create constant buffers (scene and sphere material).
    CreateConstantBuffers(device);

    // Generate sphere geometry and upload to GPU via D3D12MA.
    BuildGeometry(DR);

    // Build BLAS + TLAS (includes world transform matrix in instance descriptor).
    BuildAccelerationStructures(DR);

    // Create the raytracing output texture (UAV) and register its descriptor.
    CreateRaytracingOutputResource(DR);

    // Build the shader tables.
    BuildShaderTables(device);
}





しおり
void D3D12RaytracingSphere::CompileDXRShaderLibrary(
    LPCWSTR shaderPath,
    void** ppBytecode,
    SIZE_T* pBytecodeSize,
    std::vector<BYTE>& outBlob)
    // 
    // Create compiler and utils.
    //
    CComPtr<IDxcUtils> pUtils;
    CComPtr<IDxcCompiler3> pCompiler;
    DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils));
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler));

    //
    // Create default include handler. (You can create your own...)
    //
    CComPtr<IDxcIncludeHandler> pIncludeHandler;
    pUtils->CreateDefaultIncludeHandler(&pIncludeHandler);


    //
    // COMMAND LINE:
    // dxc myshader.hlsl -E main -T ps_6_0 -Zi -D MYDEFINE=1 -Fo myshader.bin -Fd myshader.pdb -Qstrip_reflect
    //
    LPCWSTR pszArgs[] =
    {
        L"myshader.hlsl",            // Optional shader source file name for error reporting
        // and for PIX shader source view.  
L"-E", L"main",              // Entry point.
L"-T", L"ps_6_0",            // Target.
L"-Zs",                      // Enable debug information (slim format)
L"-D", L"MYDEFINE=1",        // A single define.
L"-Fo", L"myshader.bin",     // Optional. Stored in the pdb. 
L"-Fd", L"myshader.pdb",     // The file name of the pdb. This must either be supplied
// or the autogenerated file name must be used.
L"-Qstrip_reflect",          // Strip reflection into a separate blob. 
    };


    //
    // Open source file.  
    //
    CComPtr<IDxcBlobEncoding> pSource = nullptr;
    pUtils->LoadFile(L"myshader.hlsl", nullptr, &pSource);
    DxcBuffer Source;
    Source.Ptr = pSource->GetBufferPointer();
    Source.Size = pSource->GetBufferSize();
    Source.Encoding = DXC_CP_ACP; // Assume BOM says UTF8 or UTF16 or this is ANSI text.


    //
    // Compile it with specified arguments.
    //
    CComPtr<IDxcResult> pResults;
    pCompiler->Compile(
        &Source,                // Source buffer.
        pszArgs,                // Array of pointers to arguments.
        _countof(pszArgs),      // Number of arguments.
        pIncludeHandler,        // User-provided interface to handle #include directives (optional).
        IID_PPV_ARGS(&pResults) // Compiler output status, buffer, and errors.
    );

    //
    // Print errors if present.
    //
    CComPtr<IDxcBlobUtf8> pErrors = nullptr;
    pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
    // Note that d3dcompiler would return null if no errors or warnings are present.
    // IDxcCompiler3::Compile will always return an error buffer, but its length
    // will be zero if there are no warnings or errors.
    if (pErrors != nullptr && pErrors->GetStringLength() != 0)
        wprintf(L"Warnings and Errors:\n%S\n", pErrors->GetStringPointer());

    //
    // Quit if the compilation failed.
    //
    HRESULT hrStatus;
    pResults->GetStatus(&hrStatus);
    if (FAILED(hrStatus))
    {
        wprintf(L"Compilation Failed\n");
        return 1;
    }

    //
    // Save shader binary.
    //
    CComPtr<IDxcBlob> pShader = nullptr;
    CComPtr<IDxcBlobUtf16> pShaderName = nullptr;
    pResults->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&pShader), &pShaderName);
    if (pShader != nullptr)
    {
        FILE* fp = NULL;

        _wfopen_s(&fp, pShaderName->GetStringPointer(), L"wb");
        fwrite(pShader->GetBufferPointer(), pShader->GetBufferSize(), 1, fp);
        fclose(fp);
    }

    //
    // Save pdb.
    //
    CComPtr<IDxcBlob> pPDB = nullptr;
    CComPtr<IDxcBlobUtf16> pPDBName = nullptr;
    pResults->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&pPDB), &pPDBName);
    {
        FILE* fp = NULL;

        // Note that if you don't specify -Fd, a pdb name will be automatically generated.
        // Use this file name to save the pdb so that PIX can find it quickly.
        _wfopen_s(&fp, pPDBName->GetStringPointer(), L"wb");
        fwrite(pPDB->GetBufferPointer(), pPDB->GetBufferSize(), 1, fp);
        fclose(fp);
    }

    //
    // Print hash.
    //
    CComPtr<IDxcBlob> pHash = nullptr;
    pResults->GetOutput(DXC_OUT_SHADER_HASH, IID_PPV_ARGS(&pHash), nullptr);
    if (pHash != nullptr)
    {
        wprintf(L"Hash: ");
        DxcShaderHash* pHashBuf = (DxcShaderHash*)pHash->GetBufferPointer();
        for (int i = 0; i < _countof(pHashBuf->HashDigest); i++)
            wprintf(L"%.2x", pHashBuf->HashDigest[i]);
        wprintf(L"\n");
    }

    //
    // Demonstrate getting the hash from the PDB blob using the IDxcUtils::GetPDBContents API
    //
    CComPtr<IDxcBlob> pHashDigestBlob = nullptr;
    CComPtr<IDxcBlob> pDebugDxilContainer = nullptr;
    if (SUCCEEDED(pUtils->GetPDBContents(pPDB, &pHashDigestBlob, &pDebugDxilContainer)))
    {
        // This API returns the raw hash digest, rather than a DxcShaderHash structure.
        // This will be the same as the DxcShaderHash::HashDigest returned from
        // IDxcResult::GetOutput(DXC_OUT_SHADER_HASH, ...).
        wprintf(L"Hash from PDB: ");
        const BYTE* pHashDigest = (const BYTE*)pHashDigestBlob->GetBufferPointer();
        assert(pHashDigestBlob->GetBufferSize() == 16); // hash digest is always 16 bytes.
        for (int i = 0; i < pHashDigestBlob->GetBufferSize(); i++)
            wprintf(L"%.2x", pHashDigest[i]);
        wprintf(L"\n");

        // The pDebugDxilContainer blob will contain a DxilContainer formatted
        // binary, but with different parts than the pShader blob retrieved
        // earlier.
        // The parts in this container will vary depending on debug options and
        // the compiler version.
        // This blob is not meant to be directly interpreted by an application.
    }

    //
    // Get separate reflection.
    //
    CComPtr<IDxcBlob> pReflectionData;
    pResults->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&pReflectionData), nullptr);
    if (pReflectionData != nullptr)
    {
        // Optionally, save reflection blob for later here.

        // Create reflection interface.
        DxcBuffer ReflectionData;
        ReflectionData.Encoding = DXC_CP_ACP;
        ReflectionData.Ptr = pReflectionData->GetBufferPointer();
        ReflectionData.Size = pReflectionData->GetBufferSize();

        CComPtr< ID3D12ShaderReflection > pReflection;
        pUtils->CreateReflection(&ReflectionData, IID_PPV_ARGS(&pReflection));

        // Use reflection interface here.

    }
}

// ===========================================================================
// PopulateCommandList – per-frame rendering.
// The command list is already open when this is called.
// ===========================================================================
void D3D12RaytracingSphere::PopulateCommandList(DX::DeviceResources* DR)
{
    // Refresh scene constants (camera, lights) for this frame.
    UpdateSceneConstants(DR);

    // Dispatch rays into the output UAV.
    DoRaytracing(DR);

    // Copy raytracing result to the swap-chain back buffer.
    CopyRaytracingOutputToBackbuffer(DR);
}

// ===========================================================================
// OnWindowSizeChanged – recreate the output texture when the window resizes.
// ===========================================================================
void D3D12RaytracingSphere::OnWindowSizeChanged(UINT width, UINT height, DX::DeviceResources* DR)
{
    m_width  = width;
    m_height = height;

    // Reset descriptor count back to the output UAV slot and recreate.
    m_descriptorsAllocated = SphereRaytracing::DescriptorIndex::RaytracingOutputUAV;
    m_raytracingOutput.Reset();
    CreateRaytracingOutputResource(DR);
}

// ===========================================================================
// CreateRaytracingInterfaces
// ===========================================================================
void D3D12RaytracingSphere::CreateRaytracingInterfaces(DX::DeviceResources* DR)
{
    auto device      = DR->GetD3DDevice();
    auto commandList = DR->GetCommandList();

    HRESULT hr = device->QueryInterface(IID_PPV_ARGS(&m_dxrDevice));
    if (FAILED(hr))
    {
#ifdef _DEBUG
        OutputDebugStringA("ERROR: Couldn't get ID3D12Device5 (DXR) interface.\n");
#endif
        DX::ThrowIfFailed(hr);
    }
    hr = commandList->QueryInterface(IID_PPV_ARGS(&m_dxrCommandList));
    if (FAILED(hr))
    {
#ifdef _DEBUG
        OutputDebugStringA("ERROR: Couldn't get ID3D12GraphicsCommandList4 (DXR) interface.\n");
#endif
        DX::ThrowIfFailed(hr);
    }
}

// ===========================================================================
// CreateGlobalRootSignature
// Binds:
//   slot 0 – Descriptor table: UAV  u0  (output texture)
//   slot 1 – Inline SRV       t0       (TLAS)
//   slot 2 – Inline CBV       b0       (SceneConstantBuffer)
//   slot 3 – Descriptor table: SRV t1,t2 (index + vertex buffers)
//   slot 4 – Inline CBV       b1       (SphereConstantBuffer)
// ===========================================================================
void D3D12RaytracingSphere::CreateGlobalRootSignature(ID3D12Device* device)
{
    // Descriptor ranges
    CD3DX12_DESCRIPTOR_RANGE uavRange;
    uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0); // u0

    CD3DX12_DESCRIPTOR_RANGE vbRange;
    vbRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 1);  // t1, t2

    CD3DX12_ROOT_PARAMETER rootParams[SphereRaytracing::GlobalRootSignatureParams::Count];
    rootParams[SphereRaytracing::GlobalRootSignatureParams::OutputViewSlot]
        .InitAsDescriptorTable(1, &uavRange);
    rootParams[SphereRaytracing::GlobalRootSignatureParams::AccelerationStructure]
        .InitAsShaderResourceView(0); // t0 – inline GPU VA
    rootParams[SphereRaytracing::GlobalRootSignatureParams::SceneConstant]
        .InitAsConstantBufferView(0); // b0 – inline GPU VA
    rootParams[SphereRaytracing::GlobalRootSignatureParams::VertexBuffers]
        .InitAsDescriptorTable(1, &vbRange);
    rootParams[SphereRaytracing::GlobalRootSignatureParams::SphereConstant]
        .InitAsConstantBufferView(1); // b1 – inline GPU VA

    CD3DX12_ROOT_SIGNATURE_DESC desc(
        ARRAYSIZE(rootParams), rootParams,
        0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_NONE);

    ComPtr<ID3DBlob> blob, error;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1,
                                             &blob, &error);
    if (FAILED(hr))
    {
        if (error)
            OutputDebugStringA(static_cast<const char*>(error->GetBufferPointer()));
        DX::ThrowIfFailed(hr);
    }
    DX::ThrowIfFailed(device->CreateRootSignature(
        0, blob->GetBufferPointer(), blob->GetBufferSize(),
        IID_PPV_ARGS(&m_globalRootSignature)));
    m_globalRootSignature->SetName(L"GlobalRootSignature");
}

// ===========================================================================
// CompileDXRShaderLibrary – runtime DXIL compilation via dxcompiler.dll
// Requires dxcompiler.dll (ships with Windows SDK 10.0.17134+ / DirectX SDK).
// Copy dxcompiler.dll and dxil.dll next to the executable if needed.
// ===========================================================================
void D3D12RaytracingSphere::CompileDXRShaderLibrary(
    LPCWSTR shaderPath,
    void**  ppBytecode,
    SIZE_T* pBytecodeSize,
    std::vector<BYTE>& outBlob)
{
    // Load DXC dynamically to avoid a hard link-time dependency on dxcompiler.lib.
    static HMODULE s_hDxc = nullptr;
    typedef HRESULT(WINAPI* PfnDxcCreateInstance)(REFCLSID, REFIID, LPVOID*);
    static PfnDxcCreateInstance s_pfnCreate = nullptr;

    if (!s_hDxc)
    {
        s_hDxc = LoadLibraryW(L"dxcompiler.dll");
        if (!s_hDxc)
            throw std::runtime_error(
                "Failed to load dxcompiler.dll. "
                "Install the Windows SDK (10.0.17134+) or place dxcompiler.dll next to the exe.");
        s_pfnCreate = reinterpret_cast<PfnDxcCreateInstance>(
            GetProcAddress(s_hDxc, "DxcCreateInstance"));
        if (!s_pfnCreate)
            throw std::runtime_error("DxcCreateInstance not found in dxcompiler.dll.");
    }

    // Create DXC library and compiler instances using SDK-defined CLSIDs.
    ComPtr<IDxcLibrary> dxcLibrary;
    DX::ThrowIfFailed(s_pfnCreate(CLSID_DxcLibrary, IID_PPV_ARGS(&dxcLibrary)));

    ComPtr<IDxcCompiler> dxcCompiler;
    DX::ThrowIfFailed(s_pfnCreate(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler)));

    // Load the HLSL source file.
    ComPtr<IDxcBlobEncoding> sourceBlob;
    DX::ThrowIfFailed(dxcLibrary->CreateBlobFromFile(shaderPath, nullptr, &sourceBlob));

    // Compile as a DXR shader library (lib_6_3).
    ComPtr<IDxcIncludeHandler> includeHandler;
    DX::ThrowIfFailed(dxcLibrary->CreateIncludeHandler(&includeHandler));

    ComPtr<IDxcOperationResult> result;
    DX::ThrowIfFailed(dxcCompiler->Compile(
        sourceBlob.Get(),
        shaderPath,
        L"",         // No single entry point – this is a library.
        L"lib_6_3",  // DXR shader library target
        nullptr, 0,  // No extra compiler arguments
        nullptr, 0,  // No preprocessor defines
        includeHandler.Get(),
        &result));

    HRESULT compileStatus = S_OK;
    result->GetStatus(&compileStatus);
    if (FAILED(compileStatus))
    {
        ComPtr<IDxcBlobEncoding> errorBlob;
        if (SUCCEEDED(result->GetErrorBuffer(&errorBlob)) && errorBlob)
        {
            OutputDebugStringA("DXC shader compile error:\n");
            OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        }
        DX::ThrowIfFailed(compileStatus);
    }

    // Copy DXIL bytecode into the output vector (keeping it alive for the RTPSO creation).
    ComPtr<IDxcBlob> dxilBlob;
    DX::ThrowIfFailed(result->GetResult(&dxilBlob));

    SIZE_T size = dxilBlob->GetBufferSize();
    outBlob.resize(size);
    memcpy(outBlob.data(), dxilBlob->GetBufferPointer(), size);

    *ppBytecode    = outBlob.data();
    *pBytecodeSize = size;
}

// ===========================================================================
// CreateRaytracingPipelineStateObject
// Subobjects:
//   1. DXIL library (all three shaders)
//   2. Triangle hit group
//   3. Shader config (payload / attribute sizes)
//   4. Global root signature
//   5. Pipeline config (max recursion = 1)
// ===========================================================================
void D3D12RaytracingSphere::CreateRaytracingPipelineStateObject(DX::DeviceResources* DR)
{
    // Compile the shader library.
    std::vector<BYTE> dxilBlob;
    void*  pBytecode    = nullptr;
    SIZE_T bytecodeSize = 0;
    CompileDXRShaderLibrary(L"RaytracingSphere.hlsl", &pBytecode, &bytecodeSize, dxilBlob);

    // Build the RTPSO with 5 subobjects.
    CD3DX12_STATE_OBJECT_DESC raytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

    // 1. DXIL library
    {
        auto lib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
        D3D12_SHADER_BYTECODE libdxil = { pBytecode, bytecodeSize };
        lib->SetDXILLibrary(&libdxil);
        lib->DefineExport(c_raygenShaderName);
        lib->DefineExport(c_closestHitShaderName);
        lib->DefineExport(c_missShaderName);
    }

    // 2. Triangle hit group (closest-hit only)
    {
        auto hitGroup = raytracingPipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
        hitGroup->SetClosestHitShaderImport(c_closestHitShaderName);
        hitGroup->SetHitGroupExport(c_hitGroupName);
        hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);
    }

    // 3. Shader config
    {
        auto shaderConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
        UINT payloadSize   = 4 * sizeof(float); // float4 color
        UINT attributeSize = 2 * sizeof(float); // float2 barycentrics
        shaderConfig->Config(payloadSize, attributeSize);
    }

    // 4. Global root signature
    {
        auto globalRootSig = raytracingPipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
        globalRootSig->SetRootSignature(m_globalRootSignature.Get());
    }

    // 5. Pipeline config (no secondary rays → max recursion = 1)
    {
        auto pipelineConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
        pipelineConfig->Config(1);
    }

    DX::ThrowIfFailed(m_dxrDevice->CreateStateObject(
        raytracingPipeline, IID_PPV_ARGS(&m_dxrStateObject)));
}

// ===========================================================================
// CreateDescriptorHeap
// Layout (indices match DescriptorIndex enum):
//   0 – UAV  for raytracing output
//   1 – SRV  for index buffer  (t1)
//   2 – SRV  for vertex buffer (t2)
// ===========================================================================
void D3D12RaytracingSphere::CreateDescriptorHeap(ID3D12Device* device)
{
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = SphereRaytracing::DescriptorIndex::Count;
    heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    DX::ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_descriptorHeap)));
    m_descriptorHeap->SetName(L"SphereRaytracingDescriptorHeap");
    m_descriptorSize = device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

// ===========================================================================
// CreateConstantBuffers
// ===========================================================================
void D3D12RaytracingSphere::CreateConstantBuffers(ID3D12Device* device)
{
    // Align to 256 bytes (D3D12 CBV requirement).
    const UINT64 sceneCBSize  = ALIGN_UP(sizeof(SceneConstantBuffer),  D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    const UINT64 sphereCBSize = ALIGN_UP(sizeof(SphereConstantBuffer), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    // Scene CB
    {
        auto bufDesc = CD3DX12_RESOURCE_DESC::Buffer(sceneCBSize);
        DX::ThrowIfFailed(device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&m_sceneCBResource)));
        m_sceneCBResource->SetName(L"SceneCB");
    }

    // Sphere material CB
    {
        auto bufDesc = CD3DX12_RESOURCE_DESC::Buffer(sphereCBSize);
        DX::ThrowIfFailed(device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&m_sphereCBResource)));
        m_sphereCBResource->SetName(L"SphereCB");

        // Write sphere albedo once (cornflower-blue-ish sphere).
        m_sphereCBData.albedo = XMFLOAT4(0.2f, 0.6f, 1.0f, 1.0f);
        void* pMapped = nullptr;
        CD3DX12_RANGE readRange(0, 0);
        DX::ThrowIfFailed(m_sphereCBResource->Map(0, &readRange, &pMapped));
        memcpy(pMapped, &m_sphereCBData, sizeof(m_sphereCBData));
        m_sphereCBResource->Unmap(0, nullptr);
    }
}

// ===========================================================================
// BuildGeometry
// Uses DirectXTK12 GeometricPrimitive::CreateSphere to get vertex/index data,
// then uploads position+normal into D3D12MA UPLOAD-heap buffers.
// ===========================================================================
void D3D12RaytracingSphere::BuildGeometry(DX::DeviceResources* DR)
{
    auto device = DR->GetD3DDevice();

    // Generate sphere geometry (unit sphere, 32 subdivisions, LH coordinates).
    std::vector<GeometricPrimitive::VertexType> dxtkVertices;
    std::vector<uint16_t> dxtkIndices;
    GeometricPrimitive::CreateSphere(dxtkVertices, dxtkIndices, 1.0f, 32, false);

    m_vertexCount = static_cast<UINT>(dxtkVertices.size());
    m_indexCount  = static_cast<UINT>(dxtkIndices.size());

    // Strip texture coordinates: convert VertexPositionNormalTexture → Vertex.
    std::vector<Vertex> vertices(m_vertexCount);
    for (UINT i = 0; i < m_vertexCount; ++i)
    {
        vertices[i].position = dxtkVertices[i].position;
        vertices[i].normal   = dxtkVertices[i].normal;
    }

    // ----- Vertex buffer (D3D12MA UPLOAD heap) -----
    {
        UINT64 vbSize = sizeof(Vertex) * m_vertexCount;
        D3D12MA::CALLOCATION_DESC allocDesc(D3D12_HEAP_TYPE_UPLOAD,
                                            D3D12MA::ALLOCATION_FLAG_NONE);
        auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(vbSize);
        DX::ThrowIfFailed(m_allocator->CreateResource(
            &allocDesc, &resDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            &m_vertexAllocation, IID_PPV_ARGS(&m_vertexBuffer)));
        m_vertexBuffer->SetName(L"SphereVertexBuffer");

        void* pData;
        CD3DX12_RANGE readRange(0, 0);
        DX::ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, &pData));
        memcpy(pData, vertices.data(), static_cast<size_t>(vbSize));
        m_vertexBuffer->Unmap(0, nullptr);
    }

    // ----- Index buffer (D3D12MA UPLOAD heap) -----
    {
        UINT64 ibSize = sizeof(uint16_t) * m_indexCount;
        D3D12MA::CALLOCATION_DESC allocDesc(D3D12_HEAP_TYPE_UPLOAD,
                                            D3D12MA::ALLOCATION_FLAG_NONE);
        auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(ibSize);
        DX::ThrowIfFailed(m_allocator->CreateResource(
            &allocDesc, &resDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            &m_indexAllocation, IID_PPV_ARGS(&m_indexBuffer)));
        m_indexBuffer->SetName(L"SphereIndexBuffer");

        void* pData;
        CD3DX12_RANGE readRange(0, 0);
        DX::ThrowIfFailed(m_indexBuffer->Map(0, &readRange, &pData));
        memcpy(pData, dxtkIndices.data(), static_cast<size_t>(ibSize));
        m_indexBuffer->Unmap(0, nullptr);
    }
}

// ---------------------------------------------------------------------------
// Helpers for acceleration-structure buffer allocation (DEFAULT heap).
// ---------------------------------------------------------------------------
void D3D12RaytracingSphere::CreateUAVBuffer(
    ID3D12Device* device, UINT64 bufferSize, ID3D12Resource** ppResource,
    D3D12_RESOURCE_STATES initialState, const wchar_t* name)
{
    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto bufDesc   = CD3DX12_RESOURCE_DESC::Buffer(
        bufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    DX::ThrowIfFailed(device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc,
        initialState, nullptr, IID_PPV_ARGS(ppResource)));
    if (name && *ppResource)
        (*ppResource)->SetName(name);
}

void D3D12RaytracingSphere::CreateUploadBuffer(
    ID3D12Device* device, void* pData, UINT64 dataSize,
    ID3D12Resource** ppResource, const wchar_t* name)
{
    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufDesc   = CD3DX12_RESOURCE_DESC::Buffer(dataSize);
    DX::ThrowIfFailed(device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(ppResource)));
    if (name && *ppResource)
        (*ppResource)->SetName(name);
    if (pData)
    {
        void* pMapped;
        (*ppResource)->Map(0, nullptr, &pMapped);
        memcpy(pMapped, pData, static_cast<size_t>(dataSize));
        (*ppResource)->Unmap(0, nullptr);
    }
}

// ===========================================================================
// BuildAccelerationStructures
// Uses a dedicated temporary command list to record and execute AS builds
// without interfering with the main rendering command list.
//
// The TLAS instance descriptor stores the world transform matrix, which
// positions and scales the sphere in world space.
// ===========================================================================
void D3D12RaytracingSphere::BuildAccelerationStructures(DX::DeviceResources* DR)
{
    auto device       = DR->GetD3DDevice();
    auto commandQueue = DR->GetCommandQueue();

    // Create a temporary command allocator + command list for AS builds.
    ComPtr<ID3D12CommandAllocator> cmdAlloc;
    DX::ThrowIfFailed(device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc)));

    ComPtr<ID3D12GraphicsCommandList4> cmdList;
    DX::ThrowIfFailed(device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        cmdAlloc.Get(), nullptr, IID_PPV_ARGS(&cmdList)));

    // ---- BLAS ----
    D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
    geometryDesc.Type                                 = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geometryDesc.Triangles.IndexBuffer                = m_indexBuffer->GetGPUVirtualAddress();
    geometryDesc.Triangles.IndexCount                 = m_indexCount;
    geometryDesc.Triangles.IndexFormat                = DXGI_FORMAT_R16_UINT;
    geometryDesc.Triangles.Transform3x4               = 0;
    geometryDesc.Triangles.VertexFormat               = DXGI_FORMAT_R32G32B32_FLOAT;
    geometryDesc.Triangles.VertexCount                = m_vertexCount;
    geometryDesc.Triangles.VertexBuffer.StartAddress  = m_vertexBuffer->GetGPUVirtualAddress();
    geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
    geometryDesc.Flags                                = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS blasInputs = {};
    blasInputs.DescsLayout    = D3D12_ELEMENTS_LAYOUT_ARRAY;
    blasInputs.Flags          = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    blasInputs.NumDescs       = 1;
    blasInputs.pGeometryDescs = &geometryDesc;
    blasInputs.Type           = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO blasPrebuild = {};
    m_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&blasInputs, &blasPrebuild);

    CreateUAVBuffer(device, blasPrebuild.ScratchDataSizeInBytes, &m_blasScratch,
                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"BlasScratch");
    CreateUAVBuffer(device, blasPrebuild.ResultDataMaxSizeInBytes, &m_blasResult,
                    D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, L"BlasResult");

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blasBuildDesc = {};
    blasBuildDesc.Inputs                           = blasInputs;
    blasBuildDesc.ScratchAccelerationStructureData = m_blasScratch->GetGPUVirtualAddress();
    blasBuildDesc.DestAccelerationStructureData    = m_blasResult->GetGPUVirtualAddress();
    cmdList->BuildRaytracingAccelerationStructure(&blasBuildDesc, 0, nullptr);

    // UAV barrier: ensure BLAS is complete before building TLAS.
    D3D12_RESOURCE_BARRIER blasBarrier = CD3DX12_RESOURCE_BARRIER::UAV(m_blasResult.Get());
    cmdList->ResourceBarrier(1, &blasBarrier);

    // ---- TLAS instance descriptor (stores the sphere world-transform matrix) ----
    D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
    // World transform: 3×4 row-major affine matrix.
    // Here: identity (sphere centered at origin, unit scale).
    // To translate/scale the sphere, modify these values before building the TLAS.
    instanceDesc.Transform[0][0] = 1.0f; // X-axis scale
    instanceDesc.Transform[1][1] = 1.0f; // Y-axis scale
    instanceDesc.Transform[2][2] = 1.0f; // Z-axis scale
    // Translation (column 3): sphere at origin
    instanceDesc.Transform[0][3] = 0.0f;
    instanceDesc.Transform[1][3] = 0.0f;
    instanceDesc.Transform[2][3] = 0.0f;
    instanceDesc.InstanceMask               = 1;
    instanceDesc.Flags                      = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
    instanceDesc.AccelerationStructure      = m_blasResult->GetGPUVirtualAddress();

    CreateUploadBuffer(device, &instanceDesc, sizeof(instanceDesc),
                       &m_tlasInstanceDescs, L"TlasInstanceDescs");

    // ---- TLAS ----
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs = {};
    tlasInputs.DescsLayout           = D3D12_ELEMENTS_LAYOUT_ARRAY;
    tlasInputs.Flags                 = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    tlasInputs.NumDescs              = 1;
    tlasInputs.InstanceDescs       = m_tlasInstanceDescs->GetGPUVirtualAddress();
    tlasInputs.Type                  = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tlasPrebuild = {};
    m_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&tlasInputs, &tlasPrebuild);

    CreateUAVBuffer(device, tlasPrebuild.ScratchDataSizeInBytes, &m_tlasScratch,
                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"TlasScratch");
    CreateUAVBuffer(device, tlasPrebuild.ResultDataMaxSizeInBytes, &m_tlasResult,
                    D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, L"TlasResult");

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlasBuildDesc = {};
    tlasBuildDesc.Inputs                           = tlasInputs;
    tlasBuildDesc.ScratchAccelerationStructureData = m_tlasScratch->GetGPUVirtualAddress();
    tlasBuildDesc.DestAccelerationStructureData    = m_tlasResult->GetGPUVirtualAddress();
    cmdList->BuildRaytracingAccelerationStructure(&tlasBuildDesc, 0, nullptr);

    // Close, execute, and wait for AS builds to complete.
    DX::ThrowIfFailed(cmdList->Close());
    ID3D12CommandList* ppCmdLists[] = { cmdList.Get() };
    commandQueue->ExecuteCommandLists(1, ppCmdLists);

    // Synchronize using a temporary fence.
    ComPtr<ID3D12Fence> fence;
    DX::ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
    DX::ThrowIfFailed(commandQueue->Signal(fence.Get(), 1));
    HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!event) DX::ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    DX::ThrowIfFailed(fence->SetEventOnCompletion(1, event));
    WaitForSingleObjectEx(event, INFINITE, FALSE);
    CloseHandle(event);
}

// ===========================================================================
// CreateRaytracingOutputResource
// Creates a 2D texture with UAV for the raytracing output and registers its
// UAV descriptor in the heap.
// ===========================================================================
void D3D12RaytracingSphere::CreateRaytracingOutputResource(DX::DeviceResources* DR)
{
    auto device = DR->GetD3DDevice();

    // Use the same format as the back buffer so CopyResource works directly.
    DXGI_FORMAT backBufferFmt = DR->GetBackBufferFormat();

    D3D12_RESOURCE_DESC outputDesc = {};
    outputDesc.DepthOrArraySize = 1;
    outputDesc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    outputDesc.Format           = backBufferFmt;
    outputDesc.Flags            = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    outputDesc.Width            = m_width;
    outputDesc.Height           = m_height;
    outputDesc.Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    outputDesc.MipLevels        = 1;
    outputDesc.SampleDesc       = { 1, 0 };

    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    DX::ThrowIfFailed(device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &outputDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
        IID_PPV_ARGS(&m_raytracingOutput)));
    m_raytracingOutput->SetName(L"RaytracingOutput");

    // Register UAV descriptor (heap index 0).
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
    UINT idx = AllocateDescriptor(&cpuHandle,
                                  SphereRaytracing::DescriptorIndex::RaytracingOutputUAV);

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    device->CreateUnorderedAccessView(m_raytracingOutput.Get(), nullptr, &uavDesc, cpuHandle);

    // Store the GPU handle for use during ray dispatch.
    m_raytracingOutputUAVGpuDescriptor =
        CD3DX12_GPU_DESCRIPTOR_HANDLE(
            m_descriptorHeap->GetGPUDescriptorHandleForHeapStart(),
            static_cast<INT>(idx), m_descriptorSize);

    // Register SRV descriptors for index (t1) and vertex (t2) buffers.
    // (These are set once here; the UAV heap slot is 0, SRVs are 1 and 2.)

    // Index buffer SRV (ByteAddressBuffer, t1)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE srvCpu;
        AllocateDescriptor(&srvCpu, SphereRaytracing::DescriptorIndex::IndexBufferSRV);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension           = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Format                  = DXGI_FORMAT_R32_TYPELESS;
        srvDesc.Buffer.Flags            = D3D12_BUFFER_SRV_FLAG_RAW;
        srvDesc.Buffer.NumElements      =
            static_cast<UINT>(m_indexCount * sizeof(uint16_t) / sizeof(UINT32));
        device->CreateShaderResourceView(m_indexBuffer.Get(), &srvDesc, srvCpu);
    }

    // Vertex buffer SRV (StructuredBuffer<Vertex>, t2)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE srvCpu;
        AllocateDescriptor(&srvCpu, SphereRaytracing::DescriptorIndex::VertexBufferSRV);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping     = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension               = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Format                      = DXGI_FORMAT_UNKNOWN;
        srvDesc.Buffer.NumElements          = m_vertexCount;
        srvDesc.Buffer.StructureByteStride  = sizeof(Vertex);
        srvDesc.Buffer.Flags                = D3D12_BUFFER_SRV_FLAG_NONE;
        device->CreateShaderResourceView(m_vertexBuffer.Get(), &srvDesc, srvCpu);
    }
}

// ===========================================================================
// BuildShaderTables
// Each table entry = shader identifier (32 bytes), no local root arguments.
// ===========================================================================
void D3D12RaytracingSphere::BuildShaderTables(ID3D12Device* device)
{
    ComPtr<ID3D12StateObjectProperties> stateProps;
    DX::ThrowIfFailed(m_dxrStateObject.As(&stateProps));

    void* rayGenID  = stateProps->GetShaderIdentifier(c_raygenShaderName);
    void* missID    = stateProps->GetShaderIdentifier(c_missShaderName);
    void* hitGrpID  = stateProps->GetShaderIdentifier(c_hitGroupName);

    const UINT shaderIdSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES; // 32

    auto makeShaderTable = [&](void* shaderId, const wchar_t* name,
                                ID3D12Resource** ppOut)
    {
        auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bufDesc   = CD3DX12_RESOURCE_DESC::Buffer(shaderIdSize);
        DX::ThrowIfFailed(device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(ppOut)));
        (*ppOut)->SetName(name);

        void* pMapped;
        (*ppOut)->Map(0, nullptr, &pMapped);
        memcpy(pMapped, shaderId, shaderIdSize);
        (*ppOut)->Unmap(0, nullptr);
    };

    makeShaderTable(rayGenID, L"RayGenShaderTable", &m_rayGenShaderTable);
    makeShaderTable(missID,   L"MissShaderTable",   &m_missShaderTable);
    makeShaderTable(hitGrpID, L"HitGroupShaderTable", &m_hitGroupShaderTable);
}

// ===========================================================================
// UpdateSceneConstants – rebuild camera / light data each frame.
// ===========================================================================
void D3D12RaytracingSphere::UpdateSceneConstants(DX::DeviceResources* DR)
{
    float aspectRatio = static_cast<float>(m_width) / static_cast<float>(m_height);

    // Camera: slightly above and in front of the sphere.
    XMVECTOR eye = XMVectorSet(0.0f, 1.5f, -4.0f, 1.0f);
    XMVECTOR at  = XMVectorSet(0.0f, 0.0f,  0.0f, 1.0f);
    XMVECTOR up  = XMVectorSet(0.0f, 1.0f,  0.0f, 0.0f);

    XMMATRIX view     = XMMatrixLookAtLH(eye, at, up);
    XMMATRIX proj     = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspectRatio, 0.1f, 1000.0f);
    XMMATRIX viewProj = XMMatrixMultiply(view, proj);

    // 重要: HLSL既定(column-major)に合わせて転置して渡す
    m_sceneCBData.projectionToWorld = XMMatrixTranspose(XMMatrixInverse(nullptr, viewProj));
    m_sceneCBData.cameraPosition    = eye;
    m_sceneCBData.lightPosition     = XMVectorSet(3.0f, 5.0f, -3.0f, 1.0f);
    m_sceneCBData.lightAmbientColor = XMVectorSet(0.15f, 0.15f, 0.15f, 1.0f);
    m_sceneCBData.lightDiffuseColor = XMVectorSet(0.85f, 0.85f, 0.85f, 1.0f);

    void* pMapped = nullptr;
    CD3DX12_RANGE readRange(0, 0);
    DX::ThrowIfFailed(m_sceneCBResource->Map(0, &readRange, &pMapped));
    memcpy(pMapped, &m_sceneCBData, sizeof(m_sceneCBData));
    m_sceneCBResource->Unmap(0, nullptr);
}

// ===========================================================================
// DoRaytracing – set state and dispatch rays.
// ===========================================================================
void D3D12RaytracingSphere::DoRaytracing(DX::DeviceResources* DR)
{
    auto commandList = m_dxrCommandList.Get();

    // Descriptor heap
    ID3D12DescriptorHeap* pHeaps[] = { m_descriptorHeap.Get() };
    commandList->SetDescriptorHeaps(1, pHeaps);

    // Global root signature
    commandList->SetComputeRootSignature(m_globalRootSignature.Get());

    // slot 0: UAV output texture (descriptor table)
    commandList->SetComputeRootDescriptorTable(
        SphereRaytracing::GlobalRootSignatureParams::OutputViewSlot,
        m_raytracingOutputUAVGpuDescriptor);

    // slot 1: TLAS (inline SRV – GPU virtual address)
    commandList->SetComputeRootShaderResourceView(
        SphereRaytracing::GlobalRootSignatureParams::AccelerationStructure,
        m_tlasResult->GetGPUVirtualAddress());

    // slot 2: Scene constant buffer (inline CBV)
    commandList->SetComputeRootConstantBufferView(
        SphereRaytracing::GlobalRootSignatureParams::SceneConstant,
        m_sceneCBResource->GetGPUVirtualAddress());

    // slot 3: Index + Vertex buffers (descriptor table, starts at heap index 1)
    auto vbGpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
        m_descriptorHeap->GetGPUDescriptorHandleForHeapStart(),
        SphereRaytracing::DescriptorIndex::IndexBufferSRV,
        m_descriptorSize);
    commandList->SetComputeRootDescriptorTable(
        SphereRaytracing::GlobalRootSignatureParams::VertexBuffers,
        vbGpuHandle);

    // slot 4: Sphere constant buffer (inline CBV)
    commandList->SetComputeRootConstantBufferView(
        SphereRaytracing::GlobalRootSignatureParams::SphereConstant,
        m_sphereCBResource->GetGPUVirtualAddress());

    // Set PSO and dispatch rays.
    commandList->SetPipelineState1(m_dxrStateObject.Get());

    D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
    dispatchDesc.RayGenerationShaderRecord.StartAddress =
        m_rayGenShaderTable->GetGPUVirtualAddress();
    dispatchDesc.RayGenerationShaderRecord.SizeInBytes =
        D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

    dispatchDesc.MissShaderTable.StartAddress  = m_missShaderTable->GetGPUVirtualAddress();
    dispatchDesc.MissShaderTable.SizeInBytes   = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    dispatchDesc.MissShaderTable.StrideInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

    dispatchDesc.HitGroupTable.StartAddress  = m_hitGroupShaderTable->GetGPUVirtualAddress();
    dispatchDesc.HitGroupTable.SizeInBytes   = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    dispatchDesc.HitGroupTable.StrideInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

    dispatchDesc.Width  = m_width;
    dispatchDesc.Height = m_height;
    dispatchDesc.Depth  = 1;

    commandList->DispatchRays(&dispatchDesc);
}

// ===========================================================================
// CopyRaytracingOutputToBackbuffer
// Copies the UAV texture to the current back buffer.
// The back buffer arrives in RENDER_TARGET state (from Clear()) and is
// returned to RENDER_TARGET state so that Present() can transition to PRESENT.
// ===========================================================================
void D3D12RaytracingSphere::CopyRaytracingOutputToBackbuffer(DX::DeviceResources* DR)
{
    auto commandList = m_dxrCommandList.Get();
    auto backBuffer  = DR->GetRenderTarget();

    D3D12_RESOURCE_BARRIER preCopyBarriers[2] = {
        CD3DX12_RESOURCE_BARRIER::Transition(
            backBuffer,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_COPY_DEST),
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_raytracingOutput.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_SOURCE)
    };
    commandList->ResourceBarrier(2, preCopyBarriers);

    commandList->CopyResource(backBuffer, m_raytracingOutput.Get());

    D3D12_RESOURCE_BARRIER postCopyBarriers[2] = {
        CD3DX12_RESOURCE_BARRIER::Transition(
            backBuffer,
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_RENDER_TARGET),
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_raytracingOutput.Get(),
            D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
    };
    commandList->ResourceBarrier(2, postCopyBarriers);
}

// ===========================================================================
// AllocateDescriptor
// ===========================================================================
UINT D3D12RaytracingSphere::AllocateDescriptor(
    D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse)
{
    auto descriptorHeapCpuBase = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    if (descriptorIndexToUse == UINT_MAX)
        descriptorIndexToUse = m_descriptorsAllocated++;
    *cpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(
        descriptorHeapCpuBase,
        static_cast<INT>(descriptorIndexToUse),
        m_descriptorSize);
    return descriptorIndexToUse;
}
