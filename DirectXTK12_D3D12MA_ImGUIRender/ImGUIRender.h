#pragma once
#include "D3D12MemAlloc.h"
#include "DeviceResources.h"
#include "d3dx12.h"
#include "../D3D12MA_DirectXTK12Polygon/third_party/imgui/backends/imgui_impl_dx12.h"
#include <DescriptorHeap.h>
// 4. プラットフォーム (Win32) バックエンドの初期化
#include "../D3D12MA_DirectXTK12Polygon/third_party/imgui/backends/imgui_impl_win32.h"
#include <memory>
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
    //アップロードバッファ生成(CPU書き込み用)
    //Imguiの操作結果をGPUにあるテクスチャに反映させるために使用される
    inline HRESULT CreateUploadBuffer(
        D3D12MA::Allocator* allocator,
        UINT64 buffersize,
        D3D12MA::Allocation** ppallocation,
        ID3D12Resource** ppresource)
    {
        //定数バッファの生成のため、256バイト境界にアラインメントする
        UINT64 alignedSize = (buffersize + 255) & ~255;
        return CreateUploadBuffer(allocator, alignedSize, ppallocation, ppresource);
    }

    //2Dテクスチャ生成関数
    //このプログラムでは、ImGUiパネルの描画に使用するテクスチャを生成するために使用される
    //このプログラム内で使うだけであれば、フラグはヘルパーのデフォルト引数で十分だが
	//UAV,DSV,RTVと汎用的に使えるようにflagsを引数にしている
    inline HRESULT CreateTexture2D(D3D12MA::Allocator* pAllocator,
        UINT64 width,
        UINT height,
        DXGI_FORMAT format,
        D3D12_RESOURCE_FLAGS resourceFlags,
        D3D12_RESOURCE_STATES initialState,
        const D3D12_CLEAR_VALUE* pClearValue,
        D3D12MA::Allocation** ppAllocation,
        ID3D12Resource** ppResource)
    {

        D3D12MA::CALLOCATION_DESC allocDesc(D3D12_HEAP_TYPE_DEFAULT);
        CD3DX12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1, 1, 0, resourceFlags);
		return pAllocator->CreateResource(&allocDesc, &resDesc, initialState, pClearValue, ppAllocation, IID_PPV_ARGS(ppResource));
    }
}

class ImGUIRender
{

    //ImGUIRenderのコンストラクタ
public:
    // アロケーションフラグを指定したカスタムバッファの作成例
    Microsoft::WRL::ComPtr<D3D12MA::Allocation> allocation;
	Microsoft::WRL::ComPtr<ID3D12Resource> ImGUITexture;
    void Init(DX::DeviceResources* DR)
    {

        //Asは、Microsoft::WRL::ComPtrのメソッドで、COMオブジェクトのインターフェースを取得するために使用されます。
        // Asメソッドは、指定されたインターフェースに対して、現在のCOMオブジェクトを変換し、新しいComPtrに格納します。
		auto device = DR->GetD3DDevice();
		Microsoft::WRL::ComPtr<ID3D12Device1> device1;
		device1.As(&device1);
		//CreateAllocatorを使ってAllocatorを作成するのに必要な定義書
        D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
        allocatorDesc.Flags = D3D12MA::ALLOCATOR_FLAG_NONE;
		allocatorDesc.pDevice = device1.Get();
        allocatorDesc.pAdapter = nullptr;

    }

    void initImGUI(DX::DeviceResources* DR)
    {
        // ========================================================================
// ImGui 初期化コード全体 (DirectX 12 + Win32 + DirectXTK12)
// ========================================================================

// 1. DirectXTK12 を活用した SRV 用 DescriptorHeap の作成
// ※ ImGui のフォントテクスチャや ImGui::Image() 用の SRV を配置する領域です
       
		Microsoft::WRL::ComPtr<ID3D12Device> device = DR->GetD3DDevice();
		Microsoft::WRL::ComPtr<ID3D12Device1> device1;
		device.As(&device1);
        // 2. Dear ImGui コンテキストの作成
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;

        // (オプション) 各種機能フラグの設定
        // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // キーボード操作を有効化
        // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;    // ドッキング機能を有効化 (dockingブランチ使用時)

        // 3. UIテーマ・スタイルの設定
        ImGui::StyleColorsDark(); // ダークテーマ (StyleColorsLight() や StyleColorsClassic() も利用可能)

		auto hwnd = DR->GetWindow();
		auto commandueue = DR->GetD3DCommandQueue();
        ImGui_ImplWin32_Init(hwnd);

        // 5. レンダラー (DirectX 12) バックエンドの初期化構造体の設定
        ImGui_ImplDX12_InitInfo initInfo = {};
		initInfo.Device = device1.Get();                       // ID3D12Device1*
		initInfo.CommandQueue = ;          // ID3D
        initInfo.NumFramesInFlight = NUM_FRAMES;               // ダブルバッファリングなら 2
        initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;        // レンダーターゲットのフォーマット
        initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;               // 深度バッファを使わない場合は UNKNOWN
        initInfo.SrvDescriptorHeap = srvHeap->Heap();           // DirectXTK12で作成したヒープ

        // 互換用シングルディスクリプタハンドルの指定
        initInfo.LegacySingleSrvCpuDescriptor = srvHeap->GetCpuHandle(0);
        initInfo.LegacySingleSrvGpuDescriptor = srvHeap->GetGpuHandle(0);

        // バックエンド初期化実行
        bool initSuccess = ImGui_ImplDX12_Init(&initInfo);
	}
};