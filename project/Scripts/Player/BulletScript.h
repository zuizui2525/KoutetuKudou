#pragma once
#include "Engine/Component/ScriptComponent.h"
#include "Engine/Math/MathStructs.h"
#include <string>

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
    /// 弾丸の進行方向（正規化ベクトル）を設定し、先端が進行方向を向くよう姿勢を同期します。
    /// </summary>
    /// <param name="direction">進行方向ベクトル</param>
    void SetDirection(const Vector3& direction);
    const Vector3& GetDirection() const { return direction_; }

    /// <summary>
    /// 自機等の移動から受け継ぐ慣性速度ベクトルを設定します。
    /// これにより、走行戦闘中に視点を左右に動かしても自機前進による横ドリフトを相殺し、
    /// レティクル中央軸に沿って100%まっすぐ直進させます。
    /// </summary>
    /// <param name="vel">慣性速度ベクトル</param>
    void SetInheritedVelocity(const Vector3& vel) { inheritedVelocity_ = vel; }
    const Vector3& GetInheritedVelocity() const { return inheritedVelocity_; }

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
    /// 弾丸の見た目のスケール（拡大率）を設定します。
    /// </summary>
    /// <param name="scale">見た目のスケール</param>
    void SetVisualScale(const Vector3& scale) { visualScale_ = scale; }
    const Vector3& GetVisualScale() const { return visualScale_; }

    /// <summary>
    /// 弾丸後方に漂う火花トレイルのグラデーションカラーを設定します。
    /// </summary>
    /// <param name="startColor">発生時カラー</param>
    /// <param name="endColor">消滅時カラー</param>
    void SetTrailColors(const Vector4& startColor, const Vector4& endColor) {
        trailStartColor_ = startColor;
        trailEndColor_ = endColor;
    }

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
    static constexpr float kDefaultCollisionRadius = 0.35f; // 見た目より広めの快適な当たり判定半径 (m)
    static constexpr Vector3 kDefaultVisualScale = { 0.55f, 0.55f, 0.55f }; // 小型化されたスマートな弾丸スケール
    static constexpr int kDefaultDamage = 1;
    static constexpr float kMinSpeed = 0.01f;
    static constexpr float kMaxSpeed = 5.0f;
    static constexpr int kMinLifeTime = 1;
    static constexpr int kMaxLifeTimeLimit = 600;

    // 小型オレンジ・ジェット噴射エフェクト演出定数 (マジックナンバー排除)
    static inline const std::string kEffectBulletTrail = "BulletTrail";
    static inline const std::string kEffectBulletHit = "Explosion";
    static constexpr float kPlayerJetTrailStartScale = 0.045f; // 小型でシャープなノズル口径 (m)
    static constexpr float kPlayerJetTrailEndScale   = 0.005f; // 尖って消滅する尾部 (m)
    static constexpr float kPlayerJetLifeMin         = 0.04f;  // 極短寿命 (約2.4フレーム)
    static constexpr float kPlayerJetLifeMax         = 0.07f;  // 極短寿命 (約4.2フレーム)
    static constexpr float kPlayerJetBackwardSpeed   = 1.5f;   // 弾丸背後への後退噴射初速 (m/s)
    static constexpr float kPlayerJetSpread          = 0.015f; // 横方向の微小拡散半径 (m)
    static constexpr float kDefaultBulletTrailSpawnRate = 180.0f; // 途切れのない濃厚な連続ジェット炎 (/秒)
    static constexpr Vector4 kDefaultBulletSparkStart = { 1.0f, 0.60f, 0.10f, 1.0f }; // 鮮烈発熱オレンジ（白熱核）
    static constexpr Vector4 kDefaultBulletSparkEnd   = { 0.85f, 0.18f, 0.02f, 0.00f }; // ディープバーントオレンジ（冷却消滅）
    static constexpr float kBulletHitScale = 1.2f;
    static constexpr float kTrailStepDistance = 0.5f; // 1フレームあたり1個に抑える間隔 (m)

    // メンバ変数
    float speed_ = kDefaultSpeed;
    int maxLifeTime_ = kDefaultMaxLifeTime;
    int currentLifeTimer_ = 0;
    Vector3 direction_ = { 0.0f, 0.0f, 1.0f };
    Vector3 inheritedVelocity_ = { 0.0f, 0.0f, 0.0f }; // 自機移動慣性ベクトル
    Vector3 visualScale_ = kDefaultVisualScale;
    Vector4 trailStartColor_ = kDefaultBulletSparkStart;
    Vector4 trailEndColor_ = kDefaultBulletSparkEnd;
    Vector3 previousPos_ = { 0.0f, 0.0f, 0.0f };
    bool hasPreviousPos_ = false;
    float collisionRadius_ = kDefaultCollisionRadius;
    int damage_ = kDefaultDamage;
    bool isPlayerBullet_ = true;
    bool isDead_ = false;
};
