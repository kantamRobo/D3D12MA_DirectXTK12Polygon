#pragma once
#include <d3dx12.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <DirectXMath.h>
#include <DeviceResources.h>
#include <D3D12MemAlloc.h>
#include "VertexTypes.h"
#include "pch.h"
struct Vertex
{
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT2 uv;
};

class D3D12MAHelloTexture
{
public:
	
	D3D12MAHelloTexture() {};
	D3D12MAHelloTexture(DX::DeviceResources* DR);
	void LoadAsset(DX::DeviceResources* DR);
	void Render(DX::DeviceResources* DR);
	std::vector<UINT8> GenerateTextureData();
	
	// 破棄順が重要。
	   // C++はメンバ宣言の逆順に破棄する。
	   // m_texture -> m_textureAllocation -> m_allocator の順に破棄したいので、この順で宣言する。
	Microsoft::WRL::ComPtr<D3D12MA::Allocator>  m_allocator;
	Microsoft::WRL::ComPtr<D3D12MA::Allocation> m_textureAllocation;
	Microsoft::WRL::ComPtr<ID3D12Resource>      m_texture;

	Microsoft::WRL::ComPtr<ID3D12Resource>      m_vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;

	std::unique_ptr<DirectX::DescriptorHeap> m_resourceDescriptors;

	
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
private:
	const UINT TextureWidth = 256;
	 const UINT TextureHeight = 256;
	 static constexpr UINT TexturePixelSize = 4; // 1ピクセルあたりのバイト数(RGBA8)
};

