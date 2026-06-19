#include "pch.h"
#include "D3D12MAHelloTexture.h"
#include "D3D12MemAlloc.h"
#include <vector>
#include <d3dx12.h>
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
       
        D3D12_SUBRESOURCE_DATA textureData = {};
        textureData.pData = &texture[0];
        textureData.RowPitch = TextureWidth * TexturePixelSize;
        textureData.SlicePitch = textureData.RowPitch * TextureHeight;

        UpdateSubresources(m_commandList.Get(), m_texture.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);
        //リソースバリアはResourceUploadBatchのTransition関数を使う。面倒だからだ！
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));



        // Describe and create a SRV for the texture.
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = textureDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        m_device->CreateShaderResourceView(m_texture.Get(), &srvDesc, m_srvHeap->GetCPUDescriptorHandleForHeapStart());

        //SRVのディスクリプタヒープはDirectXTK12のラッパーを使用する。


	auto commandlist = DR->GetCommandList();
    Microsoft::WRL::ComPtr<ID3D12Resource> textureData;
    DirectX::UpdateSubresources(commandlist, m_texture.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);



}



/*

// Create the texture.
    {
  auto device = DR->GetD3DDevice();
  D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
  allocatorDesc.pDevice = device;
  allocatorDesc.pAdapter = static_cast<IDXGIAdapter*>(DR->adapter.Get());
  allocatorDesc.Flags = D3D12MA_RECOMMENDED_ALLOCATOR_FLAGS;
  D3D12MA::Allocator* allocator;
  HRESULT hr = D3D12MA::CreateAllocator(&allocatorDesc, &allocator);



  //テクスチャをDEFAULTでGPUに送りたいときは、そのままだとテクスチャデータをCPUからGPUに送り込むことができない。
//そのため、使い捨ての一時バッファ、1回限りのGPUへの入場券(切り取り無効)としてテクスチャデータをUPLOADヒープに載せる
//UPLOAD用テクスチャを用意する。まず画像をテクスチャに書き込みたいので、リソースの状態(現在の役割)は「画像データの書き込み先」としておく


//CPU→GPUのUPLOADバッファはm_Uploadtextureとなる
//ヒープ、リソース記述子の命名も"Upload"をベースとしている
  const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_texture.Get(), 0, 1);
  auto UploadtextureDesc = &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
  auto UploadTextureHeap =CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)

  UploadtextureDesc->Width = TextureWidth;
  UploadtextureDesc->Height = TextureHeight;
  UploadtextureDesc->Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;




//初期状態(最初の役割)は画像テクスチャの書き込み先としておく


  allocator->CreateResource(&allocDesc, textureDesc,
D3D12_RESOURCE_STATE_COPY_DEST, NULL, &alloc, IID_PPV_ARGS(&m_Uploadtexture.ReleaseAndGetAddressOf()));



  // テクスチャ本体はGPUローカルなDEFAULTヒープに置く。
//書き込み先のバッファの名前は"m_DstTexture"になる
//後述するUpdateSubResourcesで、UPLOADバッファからDEFAULTバッファにデータを受け渡す

auto texDefaultheap = &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12MA::ALLOCATION_DESC textureAllocationDesc = {};
    textureAllocationDesc.HeapType =
        D3D12_HEAP_TYPE_DEFAULT;

    ThrowIfFailed(
        m_allocator->CreateResource(
            &textureAllocationDesc,
            &textureDesc,

            // 最初はコピー先として使用する。
            D3D12_RESOURCE_STATE_COPY_DEST,

            nullptr,

            m_textureAllocation.ReleaseAndGetAddressOf(),
            IID_PPV_ARGS(m_DstTexture.ReleaseAndGetAddressOf())
        )
    );

//ランタイムエラーやメモリリークがあった時のためにデバッグレイヤーでこのバッファを識別したい。SetNameで名前を付けておく。
m_Uploadtexture->SetName(L"HelloTexture Upload Texture");
m_DstTexture->SetName(L"HelloTexture Destination Texture");


        // Copy data to the intermediate upload heap and then schedule a copy
        // from the upload heap to the Texture2D.
        std::vector<UINT8> texture = GenerateTextureData();

        D3D12_SUBRESOURCE_DATA textureData = {};
        textureData.pData = &texture[0];
        textureData.RowPitch = TextureWidth * TexturePixelSize;
        textureData.SlicePitch = textureData.RowPitch * TextureHeight;

        UpdateSubresources(m_commandList.Get(), m_Dsttexture.Get(), m_Uploadtexture.Get(), 0, 0, 1, &textureData);
        Resourc
//リソースバリアでテクスチャを書き込み可能状態(つまり読み込み可能状態でない)から読み込み専用リソースであるシェーダーリソースの状態にとっかえひっかえする
//DirectX12ネイティブのリソースバリアを使った同期はごちゃごちゃといろいろやることがあって困るので、DirectXTK12のラッパーを使ってシンプルにする。面倒だからだ！
ResourceUploadBatch resourceUpload(device);

resourceUpload.Begin();

resourceUpload.Upload(texture.Get(), 0, &textureData, 1);

resourceUpload.Transition(
    texture.Get(),
    D3D12_RESOURCE_STATE_COPY_DEST,
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());

uploadResourcesFinished.wait();


        // Describe and create a SRV for the texture.
//こちらもDirectXTK12によるDecriptorHeapラッパーを使用した処理に書き換える。
//使用するのはDescriptorHeapラッパーとDirectXHelper.hのCreateShaderResoureceViewヘルパーとなる
//ネイティブの場合、書き間違えがあればデバッグの際に設定項目をしらみっ潰しに追われて面倒だからだ
//くわしい説明を要求されたらラッパーのソースコード見てとたらいまわしできるので・・・それにそっちの方がいいコード読めて勉強する人の学びにもなる
//なお、記述子に関して変更はない
//textureDescはD3D12RESOURECE_DESCのインスタンスを指す
std::unique_ptr<DescriptorHeap>m_resourceDescriptorHeap(device);
resourceDescriptors = std::make_unique<DescriptorHeap>(device,
    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
    D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
    Descriptors::Count);
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = textureDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        m_device->CreateShaderResourceView(m_texture.Get(), &srvDesc, m_srvHeap->GetCPUDescriptorHandleForHeapStart());
    }

*/