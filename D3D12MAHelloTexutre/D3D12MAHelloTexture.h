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
	
	Microsoft::WRL::ComPtr<ID3D12Resource> m_texture;
	Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
	std::unique_ptr<DirectX::DescriptorHeap> m_resourceDescriptors;

	
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
private:
	const UINT TextureWidth = 256;
	 const UINT TextureHeight = 256;
	
};

