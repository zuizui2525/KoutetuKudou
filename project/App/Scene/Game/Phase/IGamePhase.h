#pragma once

/**
 * @brief ゲームシーンにおける各フェーズ（描画、走行）の基底インターフェース
 */
class IGamePhase {
public:
    virtual ~IGamePhase() = default;

    // 初期化処理
    virtual void Initialize() = 0;

    // 毎フレーム更新処理
    virtual void Update() = 0;

    // 描画処理
    virtual void Draw() = 0;

    // ImGuiなどのデバッグ表示処理
    virtual void ImGuiControl() = 0;
};
