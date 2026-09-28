#ifdef _USEIMGUI
#pragma once
#include <memory>
#include <windows.h>

struct ID3D12GraphicsCommandList;

enum class AspectType {
    Aspect16_9_Low,
    Aspect16_9_High,
    Aspect4_3,
    Aspect1_1
};

class DebugEditor {
public:
    DebugEditor();
    ~DebugEditor();

    void Initialize();
    void Draw(ID3D12GraphicsCommandList* commandList);

    // GameViewWindow が表示されているかどうかを取得（App.cppのバッチング制御用）
    bool IsGameViewVisible() const { return isGameViewVisible_; }
    bool IsPaused() const { return isPaused_; }
    void SetPause(bool paused) { isPaused_ = paused; }

private:
    void DrawMenuBar(HWND hwnd);

    // 各ウィンドウクラスのインスタンス
    std::unique_ptr<class GameViewWindow> gameViewWindow_;
    std::unique_ptr<class PerformanceMonitorWindow> perfMonitorWindow_;
    std::unique_ptr<class SceneManagerWindow> sceneManagerWindow_;

    // 各ウィンドウの表示・非表示フラグ
    bool showGameView_ = true;
    bool showPerfMonitor_ = true;
    bool showHierarchy_ = true;
    bool showInspector_ = true;
    bool showSceneManager_ = true;
    bool isGameViewVisible_ = false;

    // ウィンドウ状態管理のメンバ変数
    bool isFullscreen_ = false;
    WINDOWPLACEMENT wpPrev_ = { sizeof(WINDOWPLACEMENT) };
    AspectType currentAspect_ = AspectType::Aspect16_9_Low;
    bool isPaused_ = false;
};
#endif
