#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include "Engine/Base/DxCommon/DxCommon.h"
#include "Engine/Base/WindowApp/WindowApp.h"
#include "Engine/Graphics/PSO/Manager/PSOManager.h"
#include "Engine/Base/Log/Log.h"

class DebugEditor;

class Zuizui {
public:
    static Zuizui* GetInstance();

    // 基盤の初期化と終了
    void Initialize(const wchar_t* title = L"title", const int32_t width = WindowApp::kClientWidth, const int32_t height = WindowApp::kClientHeight);
    void Update();
    void Finalize();
    void FinalizeEditor();

    // フレーム制御
    bool ProcessMessage() { return window->ProcessMessage(); }
    void ImGuiBegin();
    void ImGuiEnd();
    void BeginFrame();
    void EndFrame();

    // getter
    ID3D12Device* GetDevice() { return dxCommon->GetDevice(); }

    DxCommon* GetDxCommon() { return dxCommon.get(); }
    PSOManager* GetPSOManager() { return psoManager.get(); }
    WindowApp* GetWindow() { return window.get(); }
#ifdef _USEIMGUI
    DebugEditor* GetDebugEditor() { return debugEditor.get(); }
#endif

private:
    Zuizui() = default;
    static Zuizui* instance;

    std::unique_ptr<WindowApp> window;
    std::unique_ptr<Log> logger;
    std::unique_ptr<DxCommon> dxCommon;
    std::unique_ptr<PSOManager> psoManager;

#ifdef _USEIMGUI
    std::unique_ptr<class ImguiManager> imGui;
    std::unique_ptr<class DebugEditor> debugEditor;
#endif
};
