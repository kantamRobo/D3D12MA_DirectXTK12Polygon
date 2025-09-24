#pragma once

#include <DeviceResources.h>
#include <d3dcompiler.h>
#include "D3D12MemAlloc.h"
#include <dxgi1_6.h>
#include <DirectXMath.h>
class D3D12MAHelloTriangle
{
	struct Vertex
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT4 color;
	};
public:
	void LoadAssets(DX::DeviceResources* DR);
	void PopulateCommandList(DX::DeviceResources* DR);
	
private:
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;

	// App resources.
	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
};

