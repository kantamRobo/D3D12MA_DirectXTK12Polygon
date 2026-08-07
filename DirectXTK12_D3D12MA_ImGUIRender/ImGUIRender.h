#pragma once
#include "D3D12MemAlloc.h"
#include "d3dx12.h"
//
namespace D3D12MAUtils
{
    // ------------------------------------------------------------------------
    // 汎用バッファ生成関数 (D3D12MA::CALLOCATION_DESC を使用)
    // ------------------------------------------------------------------------
    inline HRESULT CreateBuffer(
        D3D12MA::Allocator* allocator,
        UINT64 size,
        D3D12_HEAP_TYPE heapType,
        D3D12_RESOURCE_STATES initialResourceState,
        D3D12_RESOURCE_FLAGS resourceFlags,
        D3D12MA::Allocation** allocation,
        ID3D12Resource** resource)
    {

        if (!allocator)
            return E_POINTER;

        D3D12MA::CALLOCATION_DESC allocDesc(heapType);
        CD3DX12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(size, resourceFlags);

        return allocator->CreateResource(
            &allocDesc,
            &resDesc,
            initialResourceState,
            nullptr,
            allocation,
            IID_PPV_ARGS(resource));
    };

    inline HRESULT CreateBufferEx(
		const D3D12MA::CALLOCATION_DESC& allocDesc,
        D3D12MA::Allocator* allocator,
        UINT64 size,
        D3D12_RESOURCE_STATES initialResourceState,
        D3D12_RESOURCE_FLAGS resourceFlags,
        D3D12MA::Allocation** allocation,
        ID3D12Resource** resource
    )
    {

        if (!allocator) return E_POINTER;

		CD3DX12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(size, resourceFlags);

        return allocator->CreateResource(
            &allocDesc,
            &resDesc,
            initialResourceState,
            nullptr,
            allocation,
			IID_PPV_ARGS(resource)
        );
    }

}