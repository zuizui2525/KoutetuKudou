#include "Engine/Zuizui.h"
#include "ImguiManager.h"
#include "Engine/Debug/DebugEditor.h"

Zuizui* Zuizui::instance = nullptr;

Zuizui* Zuizui::GetInstance() {
    if (!instance) instance = new Zuizui();
    return instance;
}

void Zuizui::Initialize(const wchar_t* title, const int32_t width, const int32_t height) {
    Log::LogSystemInfo();
    Log::Write(L"========================================= [エンジン起動開始] =========================================");

    window = std::make_unique<WindowApp>();
    window->Initialize(title, width, height);
    window->Show();
    Log::Write(L" ├─ 【ウィンドウ初期化完了】 画面の準備が完了しました。");

    dxCommon = std::make_unique<DxCommon>();
    dxCommon->Initialize(window->GetHWND(), width, height);
    Log::Write(L" ├─ 【DxCommon初期化完了】 DirectX12 のシステム準備が整いました。");

    psoManager = std::make_unique<PSOManager>(dxCommon->GetDevice());
    psoManager->Initialize(dxCommon->GetDxcUtils(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler());
    Log::Write(L" ├─ 【PSOマネージャ初期化完了】 パイプライン状態の準備が整いました。");

#ifdef _USEIMGUI
    imGui = std::make_unique<ImguiManager>();
    imGui->Initialize(window->GetHWND(), dxCommon->GetDevice(), dxCommon->GetBackBufferCount(), dxCommon->GetRtvFormat(), dxCommon->GetRtvHeap(), dxCommon->GetSrvHeap(), dxCommon->GetCommandQueue());
    Log::Write(L" ├─ 【ImGuiマネージャ初期化完了】 デバッグ用GUIの準備が整いました。");
    debugEditor = std::make_unique<DebugEditor>();
    debugEditor->Initialize();
    
#endif

    Log::Write(L"========================================= [エンジン起動完了] =========================================");
}

void Zuizui::Finalize() {
    Log::Write(L"========================================= [エンジン終了処理開始] =========================================");
#ifdef _USEIMGUI
    imGui->Shutdown();
#endif
    // 明示的に開放 (COM オブジェクトをすべて先に破棄する)
    delete instance;
    instance = nullptr;

    // COMの終了処理 (すべての COM オブジェクト破棄後に呼ぶ)
    CoUninitialize();

    Log::Write(L"========================================= [エンジン終了処理完了] =========================================");
}

void Zuizui::ImGuiBegin() {
#ifdef _USEIMGUI
    imGui->Begin();
    if (debugEditor) {
        debugEditor->Draw(dxCommon->GetCommandList());
    }
#endif
}

void Zuizui::ImGuiEnd() {
#ifdef _USEIMGUI
    imGui->End();
#endif
}

void Zuizui::BeginFrame() {
    dxCommon->BeginFrame();
    dxCommon->PreDraw();
}

void Zuizui::EndFrame() {
#ifdef _USEIMGUI
    dxCommon->DrawImGui();
#endif
    dxCommon->EndFrame();
    dxCommon->FrameEnd(60);
}

