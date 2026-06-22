#pragma once
#include <d3dx12.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <DirectXMath.h>
#include <DeviceResources.h>
#include <D3D12MemAlloc.h>
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
	
	Microsoft::WRL::ComPtr<ID3D12Resource> m_texture;
	Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
	std::unique_ptr<DirectX::DescriptorHeap> m_resourceDescriptors;

	

private:
	const UINT TextureWidth = 256;
	 const UINT TextureHeight = 256;
	
};

