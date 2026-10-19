#pragma once
#include "Engine/Component/ScriptComponent.h"
#include "Engine/Math/MathStructs.h"

/// <summary>
/// 弾丸（自機弾・敵弾）の直進移動、寿命カウント、当たり判定を制御するスクリプトコンポーネント
/// </summary>
class BulletScript : public ScriptComponent {
public:
    BulletScript();
    ~BulletScript() override = default;

    void Awake() override;
    void Start() override;
    void OnUpdate() override;
    void OnInspectorGUI() override;

    // パラメータ設定
    void SetDirection(const Vector3& direction) { direction_ = direction; }
    const Vector3& GetDirection() const { return direction_; }

    void SetSpeed(float speed) { speed_ = speed; }
    float GetSpeed() const { return speed_; }

    void SetIsPlayerBullet(bool isPlayer) { isPlayerBullet_ = isPlayer; }
    bool IsPlayerBullet() const { return isPlayerBullet_; }

    void SetDamage(int damage) { damage_ = damage; }
    int GetDamage() const { return damage_; }

    void SetCollisionRadius(float radius) { collisionRadius_ = radius; }
    float GetCollisionRadius() const { return collisionRadius_; }

    void SetMaxLifeTime(int maxLifeTime) { maxLifeTime_ = maxLifeTime; }
    int GetMaxLifeTime() const { return maxLifeTime_; }

    bool IsDead() const { return isDead_; }

private:
    void CheckCollisions();

private:
    // マジックナンバー排除のためのデフォルト定数
    static constexpr float kDefaultSpeed = 0.5f;
    static constexpr int kDefaultMaxLifeTime = 60;
    static constexpr float kDefaultCollisionRadius = 0.4f;
    static constexpr int kDefaultDamage = 1;
    static constexpr float kMinSpeed = 0.01f;
    static constexpr float kMaxSpeed = 5.0f;
    static constexpr int kMinLifeTime = 1;
    static constexpr int kMaxLifeTimeLimit = 600;

    // メンバ変数
    float speed_ = kDefaultSpeed;
    int maxLifeTime_ = kDefaultMaxLifeTime;
    int currentLifeTimer_ = 0;
    Vector3 direction_ = { 0.0f, 0.0f, 1.0f };
    float collisionRadius_ = kDefaultCollisionRadius;
    int damage_ = kDefaultDamage;
    bool isPlayerBullet_ = true;
    bool isDead_ = false;
};
