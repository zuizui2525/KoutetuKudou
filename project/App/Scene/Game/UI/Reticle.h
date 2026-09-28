#pragma once
#include <memory>
#include <string>

class SpriteObject;

/**
 * @brief 走行プレイ時の照準（レティクル）UIを管理するクラス
 */
class Reticle {
public:
    Reticle();
    ~Reticle();

    // 初期化処理
    void Initialize();

    // 更新処理
    void Update();

    // 描画処理
    void Draw();

private:
    // マジックナンバー排除のための定数
    static inline const float kReticleSize = 128.0f;     // レティクルのサイズ
    static inline const float kHalfSize = 64.0f;         // 中心ズレ調整用の半分サイズ
    static inline const std::string kTextureKey = "reticle"; // テクスチャキー

private:
    std::unique_ptr<SpriteObject> reticleSprite_;
};
