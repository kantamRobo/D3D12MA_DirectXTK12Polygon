#include "pch.h"

#include "D3D12MAHelloTexture.h"

#include "EffectPipelineStateDescription.h"
#include "DXSampleHelper.h"
#include "VertexTypes.h"

#include <vector>
#include <string>
#include <filesystem>

#include <d3dcompiler.h>

// DirectXTK12
#include "ResourceUploadBatch.h"
#include "DescriptorHeap.h"

using namespace DirectX;

enum Descriptors
{
    WindowsLogo,
    CourierFont,
    ControllerFont,
    GamerPic,
    Count
};

// アセットファイルのフルパスを取得するユーティリティ関数
std::wstring GetAssetFullPath(const std::wstring& assetName)
{
    std::filesystem::path assetDir = std::filesystem::current_path() / L"assets";
    return (assetDir / assetName).wstring();
}

D3D12MAHelloTexture::D3D12MAHelloTexture(DX::DeviceResources* DR)
{
    LoadAsset(DR);
}

void D3D12MAHelloTexture::LoadAsset(DX::DeviceResources* DR)
{
    auto device = DR->GetD3DDevice();

    // =============================================================
    // 1. Root Signature
    // =============================================================
    {
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

        if (FAILED(device->CheckFeatureSupport(
            D3D12_FEATURE_ROOT_SIGNATURE,
            &featureData,
            sizeof(featureData))))
        {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
        ranges[0].Init(
            D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
            1,
            0,
            0,
            D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC
        );

        CD3DX12_ROOT_PARAMETER1 rootParameters[1];
        rootParameters[0].InitAsDescriptorTable(
            1,
            &ranges[0],
            D3D12_SHADER_VISIBILITY_PIXEL
        );

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
        rootSignatureDesc.Init_1_1(
            _countof(rootParameters),
            rootParameters,
            1,
            &sampler,
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
        );

        Microsoft::WRL::ComPtr<ID3DBlob> signature;
        Microsoft::WRL::ComPtr<ID3DBlob> error;

        DX::ThrowIfFailed(
            D3DX12SerializeVersionedRootSignature(
                &rootSignatureDesc,
                featureData.HighestVersion,
                &signature,
                &error
            )
        );

        DX::ThrowIfFailed(
            device->CreateRootSignature(
                0,
                signature->GetBufferPointer(),
                signature->GetBufferSize(),
                IID_PPV_ARGS(m_rootSignature.ReleaseAndGetAddressOf())
            )
        );
    }

    // =============================================================
    // 2. Pipeline State Object
    // =============================================================
    {
        Microsoft::WRL::ComPtr<ID3DBlob> pVertexShaderData;
        Microsoft::WRL::ComPtr<ID3DBlob> pPixelShaderData;
        Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

#if defined(_DEBUG)
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT compileFlags = 0;
#endif

        DX::ThrowIfFailed(
            D3DCompileFromFile(
                L"VertexShader.hlsl",
                nullptr,
                nullptr,
                "VSMain",
                "vs_5_0",
                compileFlags,
                0,
                pVertexShaderData.ReleaseAndGetAddressOf(),
                errorBlob.ReleaseAndGetAddressOf()
            )
        );

        errorBlob.Reset();

        DX::ThrowIfFailed(
            D3DCompileFromFile(
                L"PixelShader.hlsl",
                nullptr,
                nullptr,
                "PSMain",
                "ps_5_0",
                compileFlags,
                0,
                pPixelShaderData.ReleaseAndGetAddressOf(),
                errorBlob.ReleaseAndGetAddressOf()
            )
        );

        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            {
                "POSITION",
                0,
                DXGI_FORMAT_R32G32B32_FLOAT,
                0,
                0,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0
            },
            {
                "TEXCOORD",
                0,
                DXGI_FORMAT_R32G32_FLOAT,
                0,
                12,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0
            }
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        psoDesc.pRootSignature = m_rootSignature.Get();

        psoDesc.VS.pShaderBytecode = pVertexShaderData->GetBufferPointer();
        psoDesc.VS.BytecodeLength = pVertexShaderData->GetBufferSize();

        psoDesc.PS.pShaderBytecode = pPixelShaderData->GetBufferPointer();
        psoDesc.PS.BytecodeLength = pPixelShaderData->GetBufferSize();

        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;

        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

        psoDesc.NumRenderTargets = 1;
        //レンダーターゲットと一致させる
		psoDesc.RTVFormats[0] = DR->GetBackBufferFormat();

        psoDesc.SampleDesc.Count = 1;

        DX::ThrowIfFailed(
            device->CreateGraphicsPipelineState(
                &psoDesc,
                IID_PPV_ARGS(m_pipelineState.ReleaseAndGetAddressOf())
            )
        );
    }

    // =============================================================
    // 3. Vertex Buffer
    //    小さい三角形なのでUPLOADヒープで簡略化。
    // =============================================================
    {
        VertexPositionTexture triangleVertices[] =
        {
            {
                DirectX::XMFLOAT3(0.0f, 0.25f, 0.0f),
                DirectX::XMFLOAT2(0.5f, 0.0f)
            },
            {
                DirectX::XMFLOAT3(0.25f, -0.25f, 0.0f),
                DirectX::XMFLOAT2(1.0f, 1.0f)
            },
            {
                DirectX::XMFLOAT3(-0.25f, -0.25f, 0.0f),
                DirectX::XMFLOAT2(0.0f, 1.0f)
            }
        };

        const UINT vertexBufferSize = sizeof(triangleVertices);

        auto uploadHeapProperties =
            CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

        auto vertexBufferDesc =
            CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);

        DX::ThrowIfFailed(
            device->CreateCommittedResource(
                &uploadHeapProperties,
                D3D12_HEAP_FLAG_NONE,
                &vertexBufferDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(m_vertexBuffer.ReleaseAndGetAddressOf())
            )
        );

        UINT8* pVertexDataBegin = nullptr;

        CD3DX12_RANGE readRange(0, 0);

        DX::ThrowIfFailed(
            m_vertexBuffer->Map(
                0,
                &readRange,
                reinterpret_cast<void**>(&pVertexDataBegin)
            )
        );

        memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));

        m_vertexBuffer->Unmap(0, nullptr);

        m_vertexBufferView.BufferLocation =
            m_vertexBuffer->GetGPUVirtualAddress();

        // ここは sizeof(Vertex) ではなく、実際に使っている頂点型に合わせる。
        m_vertexBufferView.StrideInBytes = sizeof(VertexPositionTexture);
        m_vertexBufferView.SizeInBytes = vertexBufferSize;
    }

    // =============================================================
    // 4. D3D12MA Allocator
    //    本来はDeviceResources側に1個だけ持たせるのが自然。
    //    ここではクラス内に保持する。
    // =============================================================
    {
        if (!m_allocator)
        {
            D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
            allocatorDesc.pDevice = device;
            allocatorDesc.pAdapter = DR->adapter.Get();
            allocatorDesc.Flags = D3D12MA_RECOMMENDED_ALLOCATOR_FLAGS;

            DX::ThrowIfFailed(
                D3D12MA::CreateAllocator(
                    &allocatorDesc,
                    m_allocator.ReleaseAndGetAddressOf()
                )
            );
        }
    }

    // =============================================================
    // 5. Texture Resource + ResourceUploadBatch
    //    手動のUpdateSubresourcesは使わない。
    //    したがって、このLoadAsset内でcommandListを閉じる必要もない。
    // =============================================================
    {
        CD3DX12_RESOURCE_DESC textureDesc =
            CD3DX12_RESOURCE_DESC::Tex2D(
                DXGI_FORMAT_R8G8B8A8_UNORM,
                TextureWidth,
                TextureHeight
            );

        D3D12MA::ALLOCATION_DESC textureAllocDesc = {};
        textureAllocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

        DX::ThrowIfFailed(
            m_allocator->CreateResource(
                &textureAllocDesc,
                &textureDesc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                m_textureAllocation.ReleaseAndGetAddressOf(),
                IID_PPV_ARGS(m_texture.ReleaseAndGetAddressOf())
            )
        );

        m_texture->SetName(L"HelloTexture Destination Texture");

        std::vector<UINT8> textureData = GenerateTextureData();

        D3D12_SUBRESOURCE_DATA subresourceData = {};
        subresourceData.pData = textureData.data();
        subresourceData.RowPitch =
            static_cast<LONG_PTR>(TextureWidth * TexturePixelSize);
        subresourceData.SlicePitch =
            static_cast<LONG_PTR>(TextureWidth * TextureHeight * TexturePixelSize);

        ResourceUploadBatch resourceUpload(device);

        resourceUpload.Begin();

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

        auto uploadFinished = resourceUpload.End(DR->GetCommandQueue());
        uploadFinished.wait();

        m_resourceDescriptors = std::make_unique<DescriptorHeap>(
            device,
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
            Descriptors::Count
        );

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping =
            D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = textureDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        device->CreateShaderResourceView(
            m_texture.Get(),
            &srvDesc,
            m_resourceDescriptors->GetCpuHandle(Descriptors::WindowsLogo)
        );
    }
}

// Generate a simple black and white checkerboard texture.
std::vector<UINT8> D3D12MAHelloTexture::GenerateTextureData()
{
    const UINT rowPitch = TextureWidth * TexturePixelSize;
    const UINT cellPitch = rowPitch >> 3;
    const UINT cellHeight = TextureWidth >> 3;
    const UINT textureSize = rowPitch * TextureHeight;

    std::vector<UINT8> data(textureSize);

    UINT8* pData = data.data();

    for (UINT n = 0; n < textureSize; n += TexturePixelSize)
    {
        UINT x = n % rowPitch;
        UINT y = n / rowPitch;

        UINT i = x / cellPitch;
        UINT j = y / cellHeight;

        if (i % 2 == j % 2)
        {
            pData[n + 0] = 0x00; // R
            pData[n + 1] = 0x00; // G
            pData[n + 2] = 0x00; // B
            pData[n + 3] = 0xff; // A
        }
        else
        {
            pData[n + 0] = 0xff; // R
            pData[n + 1] = 0xff; // G
            pData[n + 2] = 0xff; // B
            pData[n + 3] = 0xff; // A
        }
    }

    return data;
}

// 描画
void D3D12MAHelloTexture::Render(DX::DeviceResources* DR)
{
    auto commandList = DR->GetCommandList();

    commandList->SetPipelineState(m_pipelineState.Get());
    commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    ID3D12DescriptorHeap* ppHeaps[] =
    {
        m_resourceDescriptors->Heap()
    };

    commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    commandList->SetGraphicsRootDescriptorTable(
        0,
        m_resourceDescriptors->GetGpuHandle(Descriptors::WindowsLogo)
    );

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);

    commandList->DrawInstanced(3, 1, 0, 0);
}