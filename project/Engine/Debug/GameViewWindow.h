#pragma once
#include "Engine/Math/MathStructs.h"
#include <string>

#ifdef _USEIMGUI
#include "Engine/Debug/PopAnimation.h"
#endif

class GameViewWindow {
public:
#ifdef _USEIMGUI
    GameViewWindow();
    ~GameViewWindow() = default;

    void Draw(bool* show, bool* isVisible);

    // ポップ演出（再生・一時停止マーク等）の発火
    void TriggerPopAnimation(PopAnimation::Type type) { popAnim_.Trigger(type); }
#endif

    // マウスが有効なゲーム画面領域上にあるか
    static bool IsMouseOnGameView();
    // ゲーム画面ウィンドウが表示中（前面タブ）かどうか
    static bool IsGameViewVisible() { return sIsGameViewVisible_; }
    // ゲーム画面ウィンドウがフォーカスされているかどうか
    static bool IsGameViewFocused() { return sIsGameViewFocused_; }
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
    static inline bool sIsMouseOnGameView_ = false;
    static inline bool sIsGameViewVisible_ = false;
    static inline bool sIsGameViewFocused_ = false;
    static inline Vector2 sGameViewSize_ = { 0.0f, 0.0f };
    static inline Vector2 sGameViewPosMin_ = { 0.0f, 0.0f };
#else
    static inline bool sIsGameViewVisible_ = true;
    static inline bool sIsGameViewFocused_ = true;
#endif
};
