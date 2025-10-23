#pragma once
#include <d3dx12.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <DirectXMath.h>
#include <DeviceResources.h>
#include <DirectXTex.h>
class D3D12MAHelloTexture
{
public:
	D3D12MAHelloTexture() {};
	D3D12MAHelloTexture(DX::DeviceResources* DR);
	Microsoft::WRL::ComPtr<ID3D12Resource> m_texture;
	Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
private:
	const UINT TextureWidth = 256;
	 const UINT TextureHeight = 256;
	
};

