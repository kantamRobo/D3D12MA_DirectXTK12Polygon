#pragma once
#include <d3dx12.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <DirectXMath.h>
#include <DeviceResources.h>
#include <DirectXTex.h>
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
	void LoadAsset();
	Microsoft::WRL::ComPtr<ID3D12Resource> m_texture;
	Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;

	std::unique_ptr<DirectX::DescriptorHeap> m_resourceDescriptors;

	m_rootSignature;
	m_device;
	m_commandAllocator;
	m_pipelineState;//EffectPipelineStateDescription‚ÉŠ·‘•‚·‚é
	m_commandList;
	commandQueue

private:
	const UINT TextureWidth = 256;
	 const UINT TextureHeight = 256;
	
};

