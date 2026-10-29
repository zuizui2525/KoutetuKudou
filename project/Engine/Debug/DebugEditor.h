#ifdef _USEIMGUI
#pragma once
#include <memory>
#include <string>
#include <vector>
#include <windows.h>

struct ID3D12GraphicsCommandList;

enum class AspectType {
    Aspect16_9_Low,
    Aspect16_9_High,
    Aspect4_3,
    Aspect1_1
};

/// <summary>
/// ワークスペース（作業ページ）ごとのウィンドウ表示状態
/// </summary>
struct WorkspaceWindowState {
    bool showSceneView      = false;
    bool showGameView       = false;
    bool showHierarchy       = false;
    bool showInspector       = false;
    bool showConsole         = true;
    bool showParticleStudio  = false;
    bool showPostProcess     = false;
    bool showEffectList      = false;
    bool showPerfMonitor     = false;
    bool showContentBrowser  = false;
};

/// <summary>
/// ワークスペース（作業ページ）の定義情報
/// </summary>
struct WorkspaceDefinition {
    std::string id;                  // 一意識別子
    std::string name;                // タブ表示名
    bool isParticleStudio = false;   // パーティクルスタジオ専用か
    bool isCustom = false;           // ユーザー定義のカスタムページか
    WorkspaceWindowState windowState{}; // そのページで表示されているウィンドウの開閉状態
    std::string layoutData;          // そのページ専用のウィンドウ配置・サイズ・ドッキング情報 (ini)
};

enum class EditorWorkspace {
    Layout,         // シーン・ゲーム開発ワークスペース（通常画面）
    ParticleStudio, // GPUパーティクル制作・編集専用ワークスペース
    Custom          // ユーザー追加カスタムワークスペース
};

class DebugEditor {
public:
    DebugEditor();
    ~DebugEditor();

    void Initialize();
    void Draw(ID3D12GraphicsCommandList* commandList);

    static DebugEditor* GetInstance() { return sInstance_; }
    const std::string& GetCurrentWorkspaceId() const;
    static std::string FormatWorkspaceWindowName(const char* baseTitle, const char* baseId, const char* wsSuffix);

    // GameView または SceneView が表示されているかどうかを取得（App.cppのバッチング制御用）
    bool IsGameViewVisible() const { return isGameViewVisible_ || showSceneView_; }
    bool IsPaused() const { return isPaused_; }
    void SetPause(bool paused) { isPaused_ = paused; }
    void ToggleFullscreen(HWND hwnd);

    // レイアウト固定（ウィンドウ境界リサイズのロック）
    bool IsLayoutLocked() const { return isLayoutLocked_; }
    void SetLayoutLocked(bool locked) { isLayoutLocked_ = locked; }

    // ワークスペース管理 (Blender風切り替え・独自ページ追加対応)
    EditorWorkspace GetWorkspace() const { return currentWorkspace_; }
    void SetWorkspace(EditorWorkspace ws);
    void SwitchToWorkspace(size_t index);
    void AddCustomWorkspace(const std::string& name);
    void RemoveCustomWorkspace(size_t index);

    // ワークスペース状態の同期ヘルパー
    void ApplyWorkspaceWindowState(size_t index);
    void SyncCurrentWorkspaceWindowState();

    // ワークスペース永続化（JSON保存・復元・リセット・ページ別ini保存読込）
    void SaveWorkspacesConfig();
    void LoadWorkspacesConfig();
    void ResetWorkspaceLayout(size_t index);
    void SaveWorkspaceLayoutIni(const WorkspaceDefinition& ws);
    void LoadWorkspaceLayoutIni(WorkspaceDefinition& ws);

private:
    void DrawMenuBar(HWND hwnd);
    void DrawShortcutsWindow();

    // 各ウィンドウクラスのインスタンス
    std::unique_ptr<class SceneViewWindow> sceneViewWindow_;
    std::unique_ptr<class GameViewWindow> gameViewWindow_;
    std::unique_ptr<class PerformanceMonitorWindow> perfMonitorWindow_;
    std::unique_ptr<class ContentBrowserWindow> contentBrowserWindow_;
    std::unique_ptr<class ParticleStudioWindow> particleStudioWindow_;

    // 各ウィンドウの表示・非表示フラグ
    bool showSceneView_ = true;
    bool showGameView_ = true;
    bool showParticleStudio_ = false;
    bool showPerfMonitor_ = true;
    bool showHierarchy_ = true;
    bool showInspector_ = true;
    bool showContentBrowser_ = true;
    bool showShortcutsWindow_ = false;
    int helpWindowTargetTab_ = -1; // ヘルプウィンドウでフォーカスするタブインデックス (-1: 切り替えなし)
    static constexpr size_t kHelpSearchBufferSize = 128;
    char helpSearchBuffer_[kHelpSearchBufferSize] = "";
    bool helpSearchExactMatch_ = false;
    bool isGameViewVisible_ = false;
    bool showInternalObjects_ = false; // 内部オブジェクト（柱など）をヒエラルキーに表示するかどうか

    // ウィンドウ状態管理のメンバ変数
    EditorWorkspace currentWorkspace_ = EditorWorkspace::Layout;
    std::vector<WorkspaceDefinition> workspaces_;
    size_t activeWorkspaceIndex_ = 0;
    bool isFullscreen_ = false;
    WINDOWPLACEMENT wpPrev_ = { sizeof(WINDOWPLACEMENT) };
    AspectType currentAspect_ = AspectType::Aspect16_9_Low;
    bool isPaused_ = false;
    bool isSceneSelected_ = false;
    bool isLayoutLocked_ = false; // ウィンドウ間の境界リサイズを固定・ロックするかどうか

    static inline DebugEditor* sInstance_ = nullptr;
};
#endif
