#pragma once
#include "Engine/Math/MathStructs.h"
#include <string>

class SceneViewWindow {
public:
#ifdef _USEIMGUI
    SceneViewWindow();
    ~SceneViewWindow() = default;

    void Draw(bool* show);

    // ギズモ操作モード定数（マジックナンバー排除）
    static constexpr int kGizmoOpTranslate = 7;
    static constexpr int kGizmoOpRotate = 120;
    static constexpr int kGizmoOpScale = 896;

    // ギズモ操作モードの取得・設定
    int GetGizmoOperation() const { return gizmoOperation_; }
    void SetGizmoOperation(int op) { gizmoOperation_ = op; }
#endif

    // カメラ表示モード（俯瞰デバッグカメラ / メインカメラ）
    enum class CameraMode {
        DebugCamera, // 俯瞰フリーカメラ (DebugCamera)
        MainCamera   // ゲーム内メインカメラ (MainCamera)
    };
    static CameraMode GetCameraMode() { return sCameraMode_; }
    static void SetCameraMode(CameraMode mode) { sCameraMode_ = mode; }

#ifdef _USEIMGUI
    // ギズモ表示・非表示の取得・設定
    bool IsShowGizmo() const { return showGizmo_; }
    void SetShowGizmo(bool show) { showGizmo_ = show; }
#endif

    // マウスが有効なシーンビュー領域上にあるか
    static bool IsMouseOnSceneView();
    // シーンビューウィンドウが表示中（前面タブ）かどうか
    static bool IsSceneViewVisible() { return sIsSceneViewVisible_; }
    // シーンビューウィンドウがフォーカスされているかどうか
    static bool IsSceneViewFocused() { return sIsSceneViewFocused_; }
    // シーンビューのサイズ
    static Vector2 GetSceneViewSize();
    // シーンビュー上のローカルマウス座標
    static Vector2 GetMousePosition();
    // シーンビューの左上座標
    static Vector2 GetSceneViewPosMin();
    // 3Dワールド座標からシーンビュー上のスクリーン座標への変換（画面分割対応：左オフセット比率、幅比率）
    static bool WorldToScreen(const Vector3& worldPos, const class BaseCamera* camera, Vector2& outScreenPos, float vpOffsetXRatio = 0.0f, float vpWidthRatio = 1.0f);

private:
#ifdef _USEIMGUI
    bool showGizmo_ = true;                  // ギズモを描画・利用するかどうかのフラグ
    int gizmoOperation_ = kGizmoOpTranslate; // ギズモの操作モード (初期値: TRANSLATE)
    static inline bool sIsMouseOnSceneView_ = false;
    static inline bool sIsSceneViewVisible_ = false;
    static inline bool sIsSceneViewFocused_ = false;
    static inline Vector2 sSceneViewSize_ = { 0.0f, 0.0f };
    static inline Vector2 sSceneViewPosMin_ = { 0.0f, 0.0f };
    static inline CameraMode sCameraMode_ = CameraMode::DebugCamera;

    // ギズモ操作スナップショット (Undo/Redo用)
    bool isGizmoDragging_ = false;
    std::string gizmoDragTargetName_;
    Transform gizmoStartTransform_{};
    Vector2 gizmoStartLineStart_{};
    Vector2 gizmoStartLineEnd_{};
#else
    static inline bool sIsSceneViewVisible_ = false;
    static inline bool sIsSceneViewFocused_ = false;
    static inline CameraMode sCameraMode_ = CameraMode::MainCamera;
#endif
};
