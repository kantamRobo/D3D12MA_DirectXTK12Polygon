#include "pch.h"
#include "D3D12MAHelloTexture.h"
#include "D3D12MemAlloc.h"
#include <vector>
using namespace DirectX;
D3D12MAHelloTexture::D3D12MAHelloTexture(DX::DeviceResources* DR)
//Create the texture.

{
    auto device = DR->GetD3DDevice();
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


    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_texture.Get(), 0, 1);
    auto textureDesc = &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

    textureDesc->Width = TextureWidth;
    textureDesc->Height = TextureHeight;
    textureDesc->Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;


    auto texheap = &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    allocator->CreateResource(&allocDesc, textureDesc,
		D3D12_RESOURCE_STATE_COPY_DEST, NULL, &alloc, IID_PPV_ARGS(&m_texture));

    


    

        // Create the GPU upload buffer

        & CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        & CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),

            allocator->CreateResource(&allocDesc, &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
				D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &alloc, IID_PPV_ARGS(&uploadBuffer));
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(hr))
        {
            wprintf(L"Failed to initialize COM (%08X)\n", static_cast<unsigned int>(hr));
           
        }

        ScratchImage image;
        hr = LoadFromWICFile(L"texture.png", WIC_FLAGS_NONE, nullptr, image);
        if (FAILED(hr))
        {
            wprintf(L"Failed to load texture.png (%08X)\n", static_cast<unsigned int>(hr));
            
        }

        Microsoft::WRL::ComPtr<ID3D12Resource> textureUploadHeap;

        // Copy data to the intermediate upload heap and then schedule a copy 
        // from the upload heap to the Texture2D.
       
        DirectX::TexMetadata metadata = {};
        DirectX::ScratchImage scratchImg = {};
        std::vector<D3D12_SUBRESOURCE_DATA> textureSubresources;
        {
            ThrowIfFailed(LoadFromWICFile(L"Assets/test_image.png", DirectX::WIC_FLAGS_NONE, &metadata, scratchImg));
            ThrowIfFailed(PrepareUpload(device_.Get(), scratchImg.GetImages(), scratchImg.GetImageCount(), metadata, textureSubresources));
        }

        // シェーダーリソースビューの生成
        {
            // テクスチャバッファの生成
            D3D12_RESOURCE_DESC textureDesc = {};
            textureDesc.MipLevels = 1;
            textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            textureDesc.Width = static_cast<UINT>(metadata.width);   // ★修正
            textureDesc.Height = static_cast<UINT>(metadata.height); // ★修正
            textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
            textureDesc.DepthOrArraySize = 1;
            textureDesc.SampleDesc.Count = 1;
            textureDesc.SampleDesc.Quality = 0;
            textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            auto textureHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            ThrowIfFailed(device_->CreateCommittedResource(
                &textureHeapProp,
                D3D12_HEAP_FLAG_NONE,
                &textureDesc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(textureBuffer_.ReleaseAndGetAddressOf())));
            // シェーダーリソースビューの生成
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = textureDesc.Format;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;
            device_->CreateShaderResourceView(textureBuffer_.Get(), &srvDesc, srvHeap_->GetCPUDescriptorHandleForHeapStart());
        }


	auto commandlist = DR->GetCommandList();
	
    UpdateSubresources(commandlist, m_texture.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);



}