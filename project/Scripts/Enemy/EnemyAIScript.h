#pragma once
#include "Engine/Component/ScriptComponent.h"
#include "Engine/Math/MathStructs.h"
#include <memory>

class SpriteObject;
class Line2DObject;
class Ring2DObject;
class Circle2DObject;

/// <summary>
/// 敵キャラクターのAIステートマシン、部位別ダメージ判定、追従・攻撃を制御するスクリプトコンポーネント
/// </summary>
class EnemyAIScript : public ScriptComponent {
public:
    enum class AiState {
        Approach,          // 接近フェーズ
        TargetLock,        // ロックオンチャージ
        MoveToCenter,      // 正面へ回り込み
        AttackCharge,      // 攻撃チャージ
        UnavoidableAttack, // 攻撃発射
        Cooldown,          // 攻撃後クールタイム（距離維持＆待機）
        Stun               // 攻撃阻止スタン（カウンター成功で隙を晒す）
    };

    EnemyAIScript();
    ~EnemyAIScript() override;

    void Awake() override;
    void Start() override;
    void OnUpdate() override;
    void Draw() override;
    void OnInspectorGUI() override;

    /// <summary>
    /// 自機弾命中時の被弾ダメージ処理を実行します。
    /// ヘッドショット（isCritical == true）の場合はダメージが2倍となり、被弾フラッシュ色（黄色）を一時的に適用します。
    /// </summary>
    /// <param name="amount">基礎ダメージ量</param>
    /// <param name="isCritical">頭部コライダーへの命中判定</param>
    void TakeDamage(int amount, bool isCritical = false);

    int GetHp() const { return hp_; }
    int GetMaxHp() const { return maxHp_; }

    /// <summary>
    /// 敵が撃破されたか（HPが0以下か）を取得します。
    /// </summary>
    /// <returns>撃破済みの場合 true</returns>
    bool IsDead() const { return isDead_; }

    /// <summary>
    /// ボス属性を設定し、最大HPを高耐久値（kDefaultBossHp）に更新します。
    /// </summary>
    /// <param name="isBoss">ボスの場合 true</param>
    void SetBoss(bool isBoss);
    bool IsBoss() const { return isBoss_; }

    AiState GetAiState() const { return aiState_; }
    void SetAiState(AiState state) { aiState_ = state; }

private:
    /// <summary>
    /// プレイヤー追従、カリング（60m後方置き去り判定）、停止距離維持、ヨー・ピッチ旋回、
    /// および6段階のAIステートマシンを更新します。
    /// </summary>
    void UpdateAI();

    /// <summary>
    /// 自機座標へ向けて敵弾（Bulletプレハブ）を射出します。
    /// </summary>
    void FireBullet();

    /// <summary>
    /// 被弾時の点滅タイマーを更新し、タイマー終了時に元のメッシュマテリアル色へ復帰します。
    /// </summary>
    void HandleHitFlash();

    /// <summary>
    /// トランスフォーマー風アーケードターゲットサイトおよび円形HPバー用描画オブジェクトを初期化します。
    /// </summary>
    void EnsureTargetSightVisuals();

    /// <summary>
    /// 敵の円形ターゲットサイト（外周八角形HPゲージ＋照準クロスヘア＋攻撃予兆収束リング）を描画します。
    /// </summary>
    void DrawTargetSight();

    /// <summary>
    /// 自機との間に遮蔽物（建物）があるかをレイキャストサンプリングで判定します。
    /// </summary>
    bool HasLineOfSightToPlayer() const;

private:
    // マジックナンバー排除のための定数定義
    static constexpr int kDefaultNormalHp = 15;                // 通常敵HP（秒間20発連射で約0.75秒の撃ち応え）
    static constexpr int kDefaultBossHp = 150;                 // ボス敵HP（約7.5秒の集中連射）
    static constexpr float kNormalTargetCenterYOffset = 1.0f;  // 通常敵の胸部・照準中心Yオフセット
    static constexpr float kBossTargetCenterYOffset = 1.8f;    // ボス敵の胸部・照準中心Yオフセット
    static constexpr float kNormalTargetRadius = 38.0f;        // 通常敵のサイトリング半径 (px)
    static constexpr float kBossTargetRadius = 56.0f;          // ボス敵のサイトリング半径 (px)
    static constexpr int kHpSegmentCount = 8;                  // 八角形HPバーのセグメント数
    static constexpr float kHpSegmentThickness = 4.0f;         // HPバーセグメント線の太さ (px)
    static constexpr float kHpSegmentGapRatio = 0.12f;         // セグメント間のスリット隙間率
    static constexpr float kApproachSpeedZ = 0.12f;
    static constexpr float kApproachTransitionOffset = 10.0f;  // 視界内に入るZ距離
    static constexpr float kPreferredDistanceZ = 12.0f;        // 自機前方の理想維持距離 (m)
    static constexpr float kMinDistanceZ = 7.0f;               // 自機への絶対接近限界距離 (m) 重なり防止
    static constexpr float kDistanceAdjustSpeed = 0.08f;       // 停止距離調整速度
    static constexpr float kMoveLerpFactorX = 0.05f;           // 正面回り込みの滑らかなLerp係数
    static constexpr float kToCenterGaugeSpeed = 0.0075f;      // ロックオン蓄積速度（約2.2秒）
    static constexpr float kAttackGaugeSpeed = 0.0083f;        // 攻撃チャージ速度（約2.0秒）
    static constexpr int kDefaultCooldownDuration = 210;       // 攻撃後のクールタイムフレーム数（約3.5秒）
    static constexpr int kEnemyBulletDamage = 1;               // 敵弾ダメージ（1ダメージ）
    static constexpr float kCullDistanceBack = 60.0f;          // 後方取り残され時のカリング距離
    static constexpr int kHitFlashDuration = 5;                // 被弾点滅持続フレーム数
    static constexpr float kBulletSpawnOffsetZ = -1.0f;        // 弾発射オフセット
    static constexpr float kEnemyBulletSpeed = 0.35f;

    // トランスフォーマー風アタックマーカー定数 (マジックナンバー排除)
    static constexpr int kStunDuration = 45;                   // 攻撃阻止スタン持続フレーム数（約0.75秒）
    static constexpr float kTimingMarkerStartRadius = 90.0f;   // タイミング収束リング最大半径 (px)
    static constexpr float kMarkerThickness = 2.5f;            // 収束リング枠線の太さ (px)
    static constexpr float kCenterDotRadius = 4.0f;            // 中心ドット半径 (px)
    static constexpr float kCenterCrosshairSize = 10.0f;       // 中心照準クロスヘアの長さ (px)
    static constexpr float kCenterCrosshairGap = 4.0f;         // クロスヘアとサイトリングの間隔 (px)
    static constexpr float kChargeShakeIntensity = 0.05f;      // チャージ中の微小震動強度

    // メンバ変数
    int maxHp_ = kDefaultNormalHp;
    int hp_ = kDefaultNormalHp;
    bool isBoss_ = false;
    bool isDead_ = false;

    AiState aiState_ = AiState::Approach;
    float toCenterGauge_ = 0.0f;
    float attackGauge_ = 0.0f;
    int cooldownTimer_ = 0;
    int cooldownDuration_ = kDefaultCooldownDuration;
    float preferredDistanceZ_ = kPreferredDistanceZ;
    int bulletDamage_ = kEnemyBulletDamage;
    int hitFlashTimer_ = 0;
    int stunTimer_ = 0;
    float markerPulseTimer_ = 0.0f;

    Vector4 originalColor_ = { 1.0f, 0.1f, 0.1f, 1.0f }; // 敵基本色（赤）

    // 建物進入防止用の直前安全座標
    Vector3 prevSafePos_ = { 0.0f, 0.0f, 0.0f };
    bool hasPrevSafePos_ = false;

    // トランスフォーマー風アーケードターゲットサイト＆円形HPバー描画用オブジェクト (DirectX 12)
    std::unique_ptr<Line2DObject> hpSegments_[kHpSegmentCount]; // 八角形HPセグメントゲージ
    std::unique_ptr<Ring2DObject> targetBaseRing_;              // 下地ベースリング（暗色枠）
    std::unique_ptr<Ring2DObject> attackTimingRing_;            // 攻撃予兆タイミング収束リング
    std::unique_ptr<Circle2DObject> targetCenterDot_;           // 照準中心ドット
    std::unique_ptr<Line2DObject> targetCrossLines_[4];         // 十字照準クロスヘア
};
