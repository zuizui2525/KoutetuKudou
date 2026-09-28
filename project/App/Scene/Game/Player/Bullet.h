#pragma once
#include <memory>
#include <string>
#include "Engine/Graphics/Objects/3d/Cube/CubeObject.h"
#include "Engine/Math/MathStructs.h"

/**
 * @brief プレイヤーおよび敵が発射する弾クラス
 */
class Bullet {
public:
    Bullet(const Vector3& position, const Vector3& velocity, const std::string& effectName = "Fire");
    ~Bullet() = default;

    void Initialize();
    void Update();
    void Draw();

    // Getter
    bool IsActive() const { return isActive_; }
    Vector3 GetPosition() const { return cube_->GetPosition(); }
    Vector3 GetSize() const { return cube_->GetSize(); }
    const std::string& GetEffectName() const { return effectName_; }

    // Setter
    void Kill() { isActive_ = false; }
    bool IsVisualOnly() const { return isVisualOnly_; }
    void SetVisualOnly(bool visualOnly) { isVisualOnly_ = visualOnly; }

private:
    // マジックナンバー排除のための定数
    static inline const Vector3 kBulletScale = { 0.2f, 0.2f, 0.2f }; // 弾のサイズ
    static inline const std::string kTextureKey = "white";            // 描画テクスチャ
    static inline const std::string kEnvMapKey = "";                  // 環境マップ
    static inline const int kMaxLifeTime = 60;                       // 最大寿命（フレーム数）

private:
    std::unique_ptr<CubeObject> cube_;
    Vector3 velocity_;
    bool isActive_ = true;
    bool isVisualOnly_ = false;
    int lifeTimer_ = 0;
    std::string effectName_ = "Fire";
};
