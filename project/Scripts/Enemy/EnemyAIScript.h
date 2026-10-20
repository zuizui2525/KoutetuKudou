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
    /// プレイヤー追従、カリング（60m後方置き去り判定）、ヨー・ピッチ旋回、
    /// および5段階のAIステートマシンを更新します。
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
