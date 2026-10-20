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

    /// <summary>
    /// 弾丸の進行方向（正規化ベクトル）を設定します。
    /// </summary>
    /// <param name="direction">進行方向ベクトル</param>
    void SetDirection(const Vector3& direction) { direction_ = direction; }
    const Vector3& GetDirection() const { return direction_; }

    /// <summary>
    /// 弾丸の1フレームあたりの移動速度を設定します。
    /// </summary>
    /// <param name="speed">移動速度</param>
    void SetSpeed(float speed) { speed_ = speed; }
    float GetSpeed() const { return speed_; }

    /// <summary>
    /// 自機が発射した弾か、敵が発射した弾かを設定します。
    /// これにより CheckCollisions() での衝突判定対象（敵 or プレイヤー）が切り替わります。
    /// </summary>
    /// <param name="isPlayer">自機弾の場合 true、敵弾の場合 false</param>
    void SetIsPlayerBullet(bool isPlayer) { isPlayerBullet_ = isPlayer; }
    bool IsPlayerBullet() const { return isPlayerBullet_; }

    /// <summary>
    /// 命中時に相手に与える基礎ダメージ量を設定します。
    /// </summary>
    /// <param name="damage">基礎ダメージ量</param>
    void SetDamage(int damage) { damage_ = damage; }
    int GetDamage() const { return damage_; }

    /// <summary>
    /// 球体コライダーの半径を設定します。
    /// </summary>
    /// <param name="radius">当たり判定の球体半径</param>
    void SetCollisionRadius(float radius) { collisionRadius_ = radius; }
    float GetCollisionRadius() const { return collisionRadius_; }

    /// <summary>
    /// 弾丸の最大生存フレーム数を設定します。
    /// </summary>
    /// <param name="maxLifeTime">生存可能フレーム数</param>
    void SetMaxLifeTime(int maxLifeTime) { maxLifeTime_ = maxLifeTime; }
    int GetMaxLifeTime() const { return maxLifeTime_; }

    /// <summary>
    /// 弾丸がすでに命中または寿命で消滅状態にあるかを取得します。
    /// </summary>
    /// <returns>消滅済みの場合 true</returns>
    bool IsDead() const { return isDead_; }

private:
    /// <summary>
    /// 球体コライダーとシーン内対象オブジェクト（AABB）の交差判定を行い、
    /// 命中時にダメージ適用、カメラシェイク、自身の破棄を処理します。
    /// </summary>
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
