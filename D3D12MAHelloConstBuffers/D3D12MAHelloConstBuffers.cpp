#include <ResourceUploadBatch.h>
#include "D3D12MAHelloConstBuffers.h"
#include <d3dx12.h>
using namespace DirectX;
enum Descriptors
{
    WindowsLogo,
    CourierFont,
    ControllerFont,
    GamerPic,
    Count
};


//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************


D3D12MAHelloConstBuffers::D3D12MAHelloConstBuffers(UINT width, UINT height, std::wstring name) :
   
    m_pCbvDataBegin(nullptr),
   
    m_constantBufferData{}
{
}

void D3D12MAHelloConstBuffers::OnInit(DX::DeviceResources* DR)
{
    LoadPipeline(DR);
    LoadAssets(DR);
}

// Load the rendering pipeline dependencies.
void D3D12MAHelloConstBuffers::LoadPipeline(DX::DeviceResources* DR)
{
   
	auto device = DR->GetD3DDevice();
    // Create descriptor heaps.
    {

        // Describe and create a constant buffer view (CBV) descriptor heap.
        // Flags indicate that this descriptor heap can be bound to the pipeline 
        // and that descriptors contained in it can be referenced by a root table.


        m_cbvHeap = std::make_unique<DescriptorHeap>(device, Descriptors::Count);

       
    }


}

// Load the sample assets.
void  D3D12MAHelloConstBuffers::LoadAssets(DX::DeviceResources* DR)
{
    auto device = DR->GetD3DDevice();
    // Create a root signature consisting of a descriptor table with a single CBV.
    {
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

        // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;



        CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
        CD3DX12_ROOT_PARAMETER1 rootParameters[1];

        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);

        // Allow input layout and deny uneccessary access to certain pipeline stages.
        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

        Microsoft::WRL::ComPtr<ID3DBlob> signature;
        Microsoft::WRL::ComPtr<ID3DBlob> error;
        D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error);
        device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
    }

    // Create the pipeline state, which includes compiling and loading shaders.
    {
        Microsoft::WRL::ComPtr<ID3DBlob> vertexShader;
        Microsoft::WRL::ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
        // Enable better shader debugging with the graphics debugging tools.
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT compileFlags = 0;
#endif

        D3DCompileFromFile(L"VertexShader.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr);
        D3DCompileFromFile(L"PixelShader.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr);

        // Define the vertex input layout.
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        psoDesc.pRootSignature = m_rootSignature.Get();
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.SampleDesc.Count = 1;

        device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState));
    }

    // Create the command list.



    // Create the vertex buffer.
    {
        // Define the geometry for a triangle.
        Vertex triangleVertices[] =
        {
            { { 0.0f, 0.25f , 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
            { { 0.25f, -0.25f , 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
            { { -0.25f, -0.25f , 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
        };

        const UINT vertexBufferSize = sizeof(triangleVertices);

        D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
        allocatorDesc.pDevice = device;
        allocatorDesc.pAdapter = static_cast<IDXGIAdapter*>(DR->adapter.Get());
        allocatorDesc.Flags = D3D12MA_RECOMMENDED_ALLOCATOR_FLAGS;
        D3D12MA::Allocator* allocator;
        HRESULT hr = D3D12MA::CreateAllocator(&allocatorDesc, &allocator);


        D3D12MA::CALLOCATION_DESC allocDesc = D3D12MA::CALLOCATION_DESC{
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12MA::ALLOCATION_FLAG_STRATEGY_MIN_TIME };
        auto resdesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
        //リソース生成に直接ヘルパー関数によるリソースデスクの代入はできない。
        D3D12MA::Allocation* alloc;
        allocator->CreateResource(&allocDesc, &resdesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &alloc, IID_PPV_ARGS(&m_vertexBuffer));
        // Note: using upload heaps to transfer static data like vert buffers is not 
        // recommended. Every time the GPU needs it, the upload heap will be marshalled 
        // over. Please read up on Default Heap usage. An upload heap is used here for 
        // code simplicity and because there are very few verts to actually transfer.

        // Copy the triangle data to the vertex buffer.
        UINT8* pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
        memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
        m_vertexBuffer->Unmap(0, nullptr);

        // Initialize the vertex buffer view.
        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        m_vertexBufferView.StrideInBytes = sizeof(Vertex);
        m_vertexBufferView.SizeInBytes = vertexBufferSize;




        // Create the constant buffer.
        {
            const UINT constantBufferSize = sizeof(SceneConstantBuffer);    // CB size is required to be 256-byte aligned.

            auto cbufferdesc = CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize);
            allocator->CreateResource(&allocDesc, &cbufferdesc,
                D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &alloc, IID_PPV_ARGS(&m_constantBuffer));


            // Describe and create a constant buffer view.
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
            cbvDesc.BufferLocation = m_constantBuffer->GetGPUVirtualAddress();
            cbvDesc.SizeInBytes = constantBufferSize;
            device->CreateConstantBufferView(&cbvDesc, m_cbvHeap->GetCpuHandle(0));

            // Map and initialize the constant buffer. We don't unmap this until the
            // app closes. Keeping things mapped for the lifetime of the resource is okay.
            CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
            m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pCbvDataBegin));
            memcpy(m_pCbvDataBegin, &m_constantBufferData, sizeof(m_constantBufferData));
        }

    }
}

// Update frame-based values.
void  D3D12MAHelloConstBuffers::OnUpdate()
{
    const float translationSpeed = 0.005f;
    const float offsetBounds = 1.25f;

    m_constantBufferData.offset.x += translationSpeed;
    if (m_constantBufferData.offset.x > offsetBounds)
    {
        m_constantBufferData.offset.x = -offsetBounds;
    }
    memcpy(m_pCbvDataBegin, &m_constantBufferData, sizeof(m_constantBufferData));
}

// Render the scene.
void  D3D12MAHelloConstBuffers::OnRender(DX::DeviceResources* DR)
{
    // Record all the commands we need to render the scene into the command list.
    PopulateCommandList(DR);

}


// Fill the command list with all the render commands and dependent state.
void  D3D12MAHelloConstBuffers::PopulateCommandList(DX::DeviceResources* DR)
{
    auto device = DR->GetD3DDevice();
    auto  commandList = DR->GetCommandList();
    auto commandqueue = DR->GetCommandQueue();
    ResourceUploadBatch upload(device);

    upload.Begin();


    // Set necessary state.
    commandList->SetPipelineState(m_pipelineState.Get());
    commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    commandList->DrawInstanced(3, 1, 0, 0);

    // Upload the resources to the GPU.
    auto finish = upload.End(commandqueue);

    // Wait for the upload thread to terminate
    finish.wait();
}



