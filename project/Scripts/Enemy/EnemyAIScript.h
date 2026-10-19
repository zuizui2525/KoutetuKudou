#pragma once
#include "Engine/Component/ScriptComponent.h"
#include "Engine/Math/MathStructs.h"

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
        UnavoidableAttack  // 攻撃発射
    };

    EnemyAIScript();
    ~EnemyAIScript() override = default;

    void Awake() override;
    void Start() override;
    void OnUpdate() override;
    void OnInspectorGUI() override;

    // ダメージ処理（部位倍率対応）
    void TakeDamage(int amount, bool isCritical = false);
    int GetHp() const { return hp_; }
    int GetMaxHp() const { return maxHp_; }
    bool IsDead() const { return isDead_; }

    void SetBoss(bool isBoss);
    bool IsBoss() const { return isBoss_; }

    AiState GetAiState() const { return aiState_; }
    void SetAiState(AiState state) { aiState_ = state; }

private:
    void UpdateAI();
    void FireBullet();
    void HandleHitFlash();

private:
    // マジックナンバー排除のための定数定義
    static constexpr int kDefaultNormalHp = 3;
    static constexpr int kDefaultBossHp = 30;
    static constexpr float kApproachSpeedZ = 0.12f;
    static constexpr float kApproachTransitionOffset = 10.0f; // 視界内に入るZ距離
    static constexpr float kToCenterGaugeSpeed = 0.02f;       // ロックオン蓄積速度
    static constexpr float kMoveToCenterSpeedX = 0.15f;      // 正面移動速度
    static constexpr float kAttackGaugeSpeed = 0.025f;       // 攻撃チャージ速度
    static constexpr float kCullDistanceBack = 60.0f;        // 後方取り残され時のカリング距離
    static constexpr int kHitFlashDuration = 5;              // 被弾点滅持続フレーム数
    static constexpr float kBulletSpawnOffsetZ = -1.0f;       // 弾発射オフセット
    static constexpr float kEnemyBulletSpeed = 0.35f;

    // メンバ変数
    int maxHp_ = kDefaultNormalHp;
    int hp_ = kDefaultNormalHp;
    bool isBoss_ = false;
    bool isDead_ = false;

    AiState aiState_ = AiState::Approach;
    float toCenterGauge_ = 0.0f;
    float attackGauge_ = 0.0f;
    int hitFlashTimer_ = 0;

    Vector4 originalColor_ = { 1.0f, 0.1f, 0.1f, 1.0f }; // 敵基本色（赤）
};
