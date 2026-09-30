#pragma once
#include "Engine/Math/MathStructs.h"

#ifdef _USEIMGUI
#include "Engine/Debug/PopAnimation.h"
#endif

class GameViewWindow {
public:
#ifdef _USEIMGUI
    GameViewWindow();
    ~GameViewWindow() = default;

    void Draw(bool* show, bool* isVisible);

    // 左クリック一時停止の有効・無効を取得・設定
    bool IsClickPauseEnabled() const { return isClickPauseEnabled_; }
    void SetClickPauseEnabled(bool enable) { isClickPauseEnabled_ = enable; }

    // ギズモ操作モード定数（マジックナンバー排除）
    static constexpr int kGizmoOpTranslate = 7;
    static constexpr int kGizmoOpRotate = 120;
    static constexpr int kGizmoOpScale = 896;

    // ギズモ操作モードの取得・設定
    int GetGizmoOperation() const { return gizmoOperation_; }
    void SetGizmoOperation(int op) { gizmoOperation_ = op; }

    // ギズモ表示・非表示の取得・設定
    bool IsShowGizmo() const { return showGizmo_; }
    void SetShowGizmo(bool show) { showGizmo_ = show; }
#endif

    // マウスが有効なゲーム画面領域上にあるか
    static bool IsMouseOnGameView();
    // ゲーム画面のサイズ
    static Vector2 GetGameViewSize();
    // ゲーム画面上のローカルマウス座標
    static Vector2 GetMousePosition();
    // ゲーム画面の左上座標
    static Vector2 GetGameViewPosMin();
    // 3Dワールド座標からゲーム画面上のスクリーン座標への変換（画面分割対応：左オフセット比率、幅比率）
    static bool WorldToScreen(const Vector3& worldPos, const class BaseCamera* camera, Vector2& outScreenPos, float vpOffsetXRatio = 0.0f, float vpWidthRatio = 1.0f);

private:
#ifdef _USEIMGUI
    PopAnimation popAnim_;
    bool wasPaused_ = false;
    bool isClickPauseEnabled_ = false; // 左クリックによる一時停止を有効にするかどうかのフラグ（初期値: オフ）
    bool showGizmo_ = false;           // ギズモを描画・利用するかどうかのフラグ（初期値: オフ）
    int gizmoOperation_ = kGizmoOpTranslate; // ギズモの操作モード (初期値: TRANSLATE)
    static inline bool sIsMouseOnGameView_ = false;
    static inline Vector2 sGameViewSize_ = { 0.0f, 0.0f };
    static inline Vector2 sGameViewPosMin_ = { 0.0f, 0.0f };
#endif
};
