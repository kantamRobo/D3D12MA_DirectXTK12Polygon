


/*

//
// Game.cpp
//

#include "pch.h"
#include "Game.h"

extern void ExitGame() noexcept;

using namespace DirectX;
using Microsoft::WRL::ComPtr;

Game::Game() noexcept(false)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>();
    // TODO: Provide parameters for swapchain format, depth/stencil format, and backbuffer count.
    //   Add DX::DeviceResources::c_AllowTearing to opt-in to variable rate displays.
    //   Add DX::DeviceResources::c_EnableHDR for HDR10 display.
    //   Add DX::DeviceResources::c_ReverseDepth to optimize depth buffer clears for 0 instead of 1.
    m_deviceResources->RegisterDeviceNotify(this);
}

Game::~Game()
{
    if (m_deviceResources)
    {
        m_deviceResources->WaitForGpu();
    }
}

// Initialize the Direct3D resources required to run.
void Game::Initialize(HWND window, int width, int height)
{
    m_deviceResources->SetWindow(window, width, height);

    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

    // TODO: Change the timer settings if you want something other than the default variable timestep mode.
    // e.g. for 60 FPS fixed timestep update logic, call:
    /*
    m_timer.SetFixedTimeStep(true);
    m_timer.SetTargetElapsedSeconds(1.0 / 60);
    */
//}
/*
#pragma region Frame Update
// Executes the basic game loop.
void Game::Tick()
{
    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();
}

// Updates the world.
void Game::Update(DX::StepTimer const& timer)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    float elapsedTime = float(timer.GetElapsedSeconds());

    // TODO: Add your game logic here.
    elapsedTime;

    PIXEndEvent();
}
#pragma endregion

#pragma region Frame Render
// Draws the scene.
void Game::Render()
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    // Prepare the command list to render a new frame.
    m_deviceResources->Prepare();
    Clear();

    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

    // Raytrace the sphere and copy output to the back buffer.
    m_raytracingSphere.PopulateCommandList(m_deviceResources.get());

    PIXEndEvent(commandList);

    // Show the new frame.
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();

    // If using the DirectX Tool Kit for DX12, uncomment this line:
    // m_graphicsMemory->Commit(m_deviceResources->GetCommandQueue());

    PIXEndEvent();
}

// Helper method to clear the back buffers.
void Game::Clear()
{
    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Clear");

    // Clear the views.
    auto const rtvDescriptor = m_deviceResources->GetRenderTargetView();
    auto const dsvDescriptor = m_deviceResources->GetDepthStencilView();

    commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
    commandList->ClearRenderTargetView(rtvDescriptor, Colors::CornflowerBlue, 0, nullptr);
    commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // Set the viewport and scissor rect.
    auto const viewport = m_deviceResources->GetScreenViewport();
    auto const scissorRect = m_deviceResources->GetScissorRect();
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);

    PIXEndEvent(commandList);
}
#pragma endregion

#pragma region Message Handlers
// Message handlers
void Game::OnActivated()
{
    // TODO: Game is becoming active window.
}

void Game::OnDeactivated()
{
    // TODO: Game is becoming background window.
}

void Game::OnSuspending()
{
    // TODO: Game is being power-suspended (or minimized).
}

void Game::OnResuming()
{
    m_timer.ResetElapsedTime();

    // TODO: Game is being power-resumed (or returning from minimize).
}

void Game::OnWindowMoved()
{
    auto const r = m_deviceResources->GetOutputSize();
    m_deviceResources->WindowSizeChanged(r.right, r.bottom);
}

void Game::OnDisplayChange()
{
    m_deviceResources->UpdateColorSpace();
}

void Game::OnWindowSizeChanged(int width, int height)
{
    m_windowWidth  = width;
    m_windowHeight = height;

    if (!m_deviceResources->WindowSizeChanged(width, height))
        return;

    CreateWindowSizeDependentResources();
}

// Properties
void Game::GetDefaultSize(int& width, int& height) const noexcept
{
    // TODO: Change to desired default window size (note minimum size is 320x200).
    width = 800;
    height = 600;
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Game::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();

    // Check Shader Model 6 support
    D3D12_FEATURE_DATA_SHADER_MODEL shaderModel = { D3D_SHADER_MODEL_6_0 };
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel)))
        || (shaderModel.HighestShaderModel < D3D_SHADER_MODEL_6_0))
    {
#ifdef _DEBUG
        OutputDebugStringA("ERROR: Shader Model 6.0 is not supported!\n");
#endif
        throw std::runtime_error("Shader Model 6.0 is not supported!");
    }

    // If using the DirectX Tool Kit for DX12, uncomment this line:
    // m_graphicsMemory = std::make_unique<GraphicsMemory>(device);

    // TODO: Initialize device dependent objects here (independent of window size).
    m_raytracingSphere.LoadAssets(m_deviceResources.get());
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    auto outputSize = m_deviceResources->GetOutputSize();
    UINT w = static_cast<UINT>(outputSize.right  - outputSize.left);
    UINT h = static_cast<UINT>(outputSize.bottom - outputSize.top);
    if (w == 0) w = static_cast<UINT>(m_windowWidth);
    if (h == 0) h = static_cast<UINT>(m_windowHeight);

    // Recreate the raytracing output texture at the new size.
    m_deviceResources->WaitForGpu();
    m_raytracingSphere.OnWindowSizeChanged(w, h, m_deviceResources.get());
}

void Game::OnDeviceLost()
{
    // Release all device-dependent resources.
    // The sphere renderer cleans up automatically via RAII (ComPtr + ~D3D12RaytracingSphere).
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
}
#pragma endregion
*/



//----------------------------------------------------------------
//Qwen 3.5 9B No thinking LM Studio 2026-05-30 
//RTX 3050 VRAM 8GB
//
// Game.cpp (修正版)
//

#include "pch.h"
#include "Game.h"
#include "Model.h" // Model が必要なので Include
#include <DirectXMath.h>

extern void ExitGame() noexcept;

using namespace DirectX;

using Microsoft::WRL::ComPtr;

Game::Game() noexcept(false)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>();

    // 重要: DXR は通常、Direct3D 12 のバックバッファと同じ解像度で出力されます。
    // Model.h に VertexPositionNormalColorTexture などがあります。
}

Game::~Game()
{
    if (m_deviceResources)
    {
        m_deviceResources->WaitForGpu();
    }
}

// Initialize the Direct3D resources required to run.
void Game::Initialize(HWND window, int width, int height)
{
    m_deviceResources->SetWindow(window, width, height);

    m_deviceResources->CreateDeviceResources();
    // DeviceDependentResources 作成時に Model をロード
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
}

#pragma region Frame Update
void Game::Tick()
{
    m_timer.Tick([&]()
        {
            Update(m_timer);
        });

    Render(); // ここが描画の入口
}

void Game::Update(DX::StepTimer const& timer)
{
    // TODO: Add your game logic here.
}
#pragma endregion

#pragma region Frame Render
// Draws the scene using DirectX Raytracing (DXR).
void Game::Render()
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    // Prepare the command list for DXR rendering.
    // この関数は Graphics Pipeline のバインドを行いません。
    m_deviceResources->Prepare();

    auto commandList = m_deviceResources->GetCommandList();

    // イベント開始（デバッグ用）
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"DXR Render Frame");

    // 【修正ポイント 1】グラフィックスパイプラインのバインド（ClearRenderTargetView など）は不要です。
    // DirectX Raytracing API だけを呼び出します。

    // Model の PopulateCommandList メソッドで、以下の処理が内部で行われます：
    // 1. Scene Constants の更新 (UpdateSceneConstants)
    // 2. レイトレーシングの実行 (Render -> DispatchRays)
    // 3. レイ結果のコピー (CopyRaytracingOutputToBackbuffer)

    // Model が初期化されていない場合は何もしません。
    if (m_model)
    {
        m_model->PopulateCommandList(m_deviceResources.get());
    }

    PIXEndEvent(commandList);

    // Show the new frame.
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
    PIXEndEvent();
}

// Helper method (旧機能のため参照用だが、現在は不要かもしれません)
void Game::Clear()
{
    // 【修正ポイント 2】DXR レンダリングフローでは、バックバッファを「グラフィックスパイプラインを使ってクリア」するのではなく、
    // 「レイトレーシング結果のコピー」がメインです。
    // もし何らかの原因で初期化が必要な場合は、Model::PopulateCommandList の内部で行われるバリアrier処理に任せるのが標準的です。
    // あるいは、単純に Color でクリアしたい場合でも、Graphics Pipeline を通さずに Resource Barrier だけで行うことができますが、
    // DXR の実装（上記 Model.cpp）では CopyResource だけがメインなので、この関数は必要ありません。

    // Model.cpp の PopulateCommandList -> CopyRaytracingOutputToBackbuffer で以下の処理が行われます:
    // 1. BackBuffer (RENDER_TARGET -> COPY_DEST) の遷移
    // 2. Raytracing Output (UNORDERED_ACCESS -> COPY_SOURCE) の遷移
    // 3. コピー実行
    // 4. バックバッファの戻り遷移 (COPY_DEST -> RENDER_TARGET)

    // ※もし画面が真っ暗（未初期化）になる場合、PopulateCommandList の直後に単純なクリアを入れることもありますが、
    // モデル側のロジック（Model.cpp の CreateRaytracingOutputResource など）に任せている前提です。
}
#pragma endregion
