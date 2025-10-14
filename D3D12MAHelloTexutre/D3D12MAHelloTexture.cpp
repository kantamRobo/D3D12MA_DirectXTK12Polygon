#include "pch.h"
#include "D3D12MAHelloTexture.h"
#include "D3D12MemAlloc.h"
#include <vector>
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
        

        Microsoft::WRL::ComPtr<ID3D12Resource> textureUploadHeap;

        // Copy data to the intermediate upload heap and then schedule a copy 
        // from the upload heap to the Texture2D.
        std::vector<UINT8> texture = GenerateTextureData();
    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = &texture[0];
    textureData.RowPitch = TextureWidth * TexturePixelSize;
    textureData.SlicePitch = textureData.RowPitch * TextureHeight;
	auto commandlist = DR->GetCommandList();
	
    UpdateSubresources(commandlist, m_texture.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);
}