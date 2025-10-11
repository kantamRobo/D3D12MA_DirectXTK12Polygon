#pragma once

#include <d3dcompiler.h>
#include "D3D12MemAlloc.h"
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <wrl.h>
#include "../D3D12MA_DirectXTK12Polygon/DeviceResources.h"
#include <string>
#include <memory>
#include <DescriptorHeap.h>
struct SceneConstantBuffer
{
	DirectX::XMFLOAT4 offset;
	float padding[60]; // Padding so the constant buffer is 256-byte aligned.
};
class D3D12MAHelloConstBuffers
{
public:
	D3D12MAHelloConstBuffers(UINT width, UINT height, std::wstring name);
	void OnInit(DX::DeviceResources* DR);
	
	void LoadPipeline(DX::DeviceResources* DR);
	struct Vertex
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT4 color;
	};
	UINT8* m_pCbvDataBegin;
public:
	void LoadAssets(DX::DeviceResources* DR);

	void OnUpdate();
	
	

	void OnUpdate(DX::DeviceResources* DR);

	void OnRender(DX::DeviceResources* DR);

	void PopulateCommandList(DX::DeviceResources* DR);

private:
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
	SceneConstantBuffer m_constantBufferData;
	// App resources.
	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_constantBuffer;
	std::unique_ptr<DirectX::DescriptorHeap> m_cbvHeap;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
};

