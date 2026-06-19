#include "pch.h"
#include "D3D12MAHelloTexture.h"
#include "D3D12MemAlloc.h"
#include <vector>
#include <DXSampleHelper.h>
using namespace DirectX;
D3D12MAHelloTexture::D3D12MAHelloTexture(DX::DeviceResources* DR)

{
    



}

void D3D12MAHelloTexture::LoadAsset(){

    
    // Create the root signature.
    {
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

        // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

        if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
        {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

        CD3DX12_ROOT_PARAMETER1 rootParameters[1];
        rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

        D3D12_STATIC_SAMPLER_DESC sampler = {};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampler.MipLODBias = 0;
        sampler.MaxAnisotropy = 0;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        sampler.MinLOD = 0.0f;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderRegister = 0;
        sampler.RegisterSpace = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        Microsoft::WRL::ComPtr<ID3DBlob> signature;
        Microsoft::WRL::ComPtr<ID3DBlob> error;
        DX::ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
        DX::ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
    }

    // Create the pipeline state, which includes compiling and loading shaders.
    {
        UINT8* pVertexShaderData = nullptr;
        UINT8* pPixelShaderData = nullptr;
        UINT vertexShaderDataLength = 0;
        UINT pixelShaderDataLength = 0;

        DX::ThrowIfFailed(ReadDataFromFile(GetAssetFullPath(L"shaders_VSMain.cso").c_str(), &pVertexShaderData, &vertexShaderDataLength));
        DX::ThrowIfFailed(ReadDataFromFile(GetAssetFullPath(L"shaders_PSMain.cso").c_str(), &pPixelShaderData, &pixelShaderDataLength));

        // Define the vertex input layout.
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        psoDesc.pRootSignature = m_rootSignature.Get();
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(pVertexShaderData, vertexShaderDataLength);
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(pPixelShaderData, pixelShaderDataLength);
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.SampleDesc.Count = 1;
        DX::ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
    }

    // Create the command list.
    DX::ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));

    // Create the vertex buffer.
    {
        // Define the geometry for a triangle.
        Vertex triangleVertices[] =
        {
            { { 0.0f, 0.25f * m_aspectRatio, 0.0f }, { 0.5f, 0.0f } },
            { { 0.25f, -0.25f * m_aspectRatio, 0.0f }, { 1.0f, 1.0f } },
            { { -0.25f, -0.25f * m_aspectRatio, 0.0f }, { 0.0f, 1.0f } }
        };

        const UINT vertexBufferSize = sizeof(triangleVertices);

        // Note: using upload heaps to transfer static data like vert buffers is not 
        // recommended. Every time the GPU needs it, the upload heap will be marshalled 
        // over. Please read up on Default Heap usage. An upload heap is used here for 
        // code simplicity and because there are very few verts to actually transfer.
        DX::ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_vertexBuffer)));

        // Copy the triangle data to the vertex buffer.
        UINT8* pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        DX::ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
        m_vertexBuffer->Unmap(0, nullptr);

        // Initialize the vertex buffer view.
        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        m_vertexBufferView.StrideInBytes = sizeof(Vertex);
        m_vertexBufferView.SizeInBytes = vertexBufferSize;
    }
    
    // 既存のCreateCommittedResource箇所を以下のように置き換えます
    //テクスチャリソース作成
    {
        // 1. テクスチャリソースの作成（Defaultヒープ）
        D3D12MA::ALLOCATION_DESC texAllocDesc = {};
        texAllocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

        CD3DX12_RESOURCE_DESC texDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height);

        // D3D12MAを使用してリソースを確保
        Microsoft::WRL::ComPtr<D3D12MA::Allocation> texAllocation;
        Microsoft::WRL::ComPtr<ID3D12Resource> texture;
        allocator->CreateResource(&texAllocDesc, &texDesc, D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr, &texAllocation, IID_PPV_ARGS(&texture));

        // 2. アップロード用バッファの作成（Uploadヒープ）
        UINT64 uploadBufferSize = 0;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
        device->GetCopyableFootprints(&texDesc, 0, 1, 0, &layout, nullptr, nullptr, &uploadBufferSize);

        D3D12MA::ALLOCATION_DESC uploadAllocDesc = {};
        uploadAllocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

        Microsoft::WRL::ComPtr<D3D12MA::Allocation> uploadAllocation;
        Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;
        allocator->CreateResource(&uploadAllocDesc, &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            &uploadAllocation, IID_PPV_ARGS(&uploadBuffer));



        // 4. コマンドによるコピー処理  
        D3D12_SUBRESOURCE_DATA textureData = {};
        textureData.pData = &texture[0];
        textureData.RowPitch = TextureWidth * TexturePixelSize;
        textureData.SlicePitch = textureData.RowPitch * TextureHeight;

        UpdateSubresources(m_commandList.Get(), m_texture.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);
        //中間バッファはtextureUploadHeapになる。メンバ変数にID3D12Resourceのポインタを用意する


        // D3D12HelloTexture.h 内
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> m_allocator;

        D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
        allocatorDesc.pDevice = m_device.Get();
        allocatorDesc.pAdapter = hardwareAdapter.Get(); // もしくは warpAdapter
        D3D12MA::CreateAllocator(&allocatorDesc, &m_allocator);



        // -------------------------------------------------------------
        // 1. テクスチャリソース (DEFAULTヒープ) の生成
        // -------------------------------------------------------------
        CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
            DXGI_FORMAT_R8G8B8A8_UNORM, TextureWidth, TextureHeight);

        D3D12MA::ALLOCATION_DESC defaultAllocDesc = {};
        defaultAllocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

        // ヘッダにある m_texture を使用
        HRESULT hr = m_allocator->CreateResource(
            &defaultAllocDesc,
            &textureDesc,
            D3D12_RESOURCE_STATE_COPY_DEST, // 初期状態はコピー先
            nullptr,
            m_textureAllocation.ReleaseAndGetAddressOf(),// D3D12MA::Allocationのアドレスを取得
            IID_PPV_ARGS(m_texture.ReleaseAndGetAddressOf())
        );
        if (FAILED(hr)) { /* 適切なエラーハンドリング */ }
        m_texture->SetName(L"HelloTexture Destination Texture");

        // -------------------------------------------------------------
        // 2. アップロードバッファ (UPLOADヒープ) の生成
        // -------------------------------------------------------------
        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_texture.Get(), 0, 1);
        CD3DX12_RESOURCE_DESC uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

        D3D12MA::ALLOCATION_DESC uploadAllocDesc = {};
        uploadAllocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

        // ヘッダにある uploadBuffer を使用
        hr = m_allocator->CreateResource(
            &uploadAllocDesc,
            &uploadBufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
			m_uploadAllocation.ReleaseAndGetAddressOf(),// D3D12MA::Allocationのアドレスを取得
            IID_PPV_ARGS(uploadBuffer.ReleaseAndGetAddressOf())
        );
        if (FAILED(hr)) { /* 適切なエラーハンドリング */ }
        uploadBuffer->SetName(L"HelloTexture Upload Buffer");

        // -------------------------------------------------------------
        // 3. テクスチャデータのアップロード (ResourceUploadBatchを使用)
        // -------------------------------------------------------------
        // 外部実装の画像データ生成メソッドを呼び出し
        std::vector<UINT8> textureData = GenerateTextureData();

        D3D12_SUBRESOURCE_DATA subresourceData = {};
        subresourceData.pData = textureData.data();
        subresourceData.RowPitch = TextureWidth * 4; // DXGI_FORMAT_R8G8B8A8_UNORM = 4 bytes per pixel
        subresourceData.SlicePitch = subresourceData.RowPitch * TextureHeight;

        ResourceUploadBatch resourceUpload(device);
        resourceUpload.Begin();

        // UPLOADヒープへの書き込みからDEFAULTヒープへのコピー、バリアまでをラップして実行
        resourceUpload.Upload(
            m_texture.Get(),
            0,
            &subresourceData,
            1
        );

        resourceUpload.Transition(
            m_texture.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );

        // コマンドキューを実行し、完了まで待機
        auto uploadResourcesFinished = resourceUpload.End(commandQueue);
        uploadResourcesFinished.wait();

        // -------------------------------------------------------------
        // 4. SRV の作成 (DirectXTK12 DescriptorHeap)
        // -------------------------------------------------------------
        m_resourceDescriptors = std::make_unique<DescriptorHeap>(
            device,
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
            Descriptors::Count // 用意されている列挙型を使用
        );

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = textureDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        device->CreateShaderResourceView(
            m_texture.Get(),
            &srvDesc,
            m_resourceDescriptors->GetCpuHandle(0)
        )
    };
}
