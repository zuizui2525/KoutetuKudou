#pragma once
#include <random>
#include "App/Scene/Game/Player/Shooter.h"
#include "Engine/Graphics/Objects/3d/Cube/CubeObject.h"
#include "App/Scene/Game/Enemy/PartCollider.h"

class Player;

/**
 * @brief 敵キャラクタークラス
 */
class Enemy : public Shooter {
public:
    Enemy();
    ~Enemy() override = default;

    void Initialize() override;
    void Update() override;
    void Draw() override;

    // 衝突判定用インターフェース実装
    Vector3 GetPosition() const override { return cube_->GetPosition(); }
    Vector3 GetSize() const override { return size_; }
    void SetPosition(const Vector3& pos) { cube_->SetPosition(pos); }
    void SetSize(const Vector3& size);
    CubeObject* GetCube() const { return cube_.get(); }

    // コライダーおよびターゲット関連
    PartCollider* GetBodyCollider() const { return bodyCollider_.get(); }
    PartCollider* GetHeadCollider() const { return headCollider_.get(); }
    void SetTargetPlayer(Player* player) { targetPlayer_ = player; }

public:
    enum class EnemyType {
        Normal,      // 従来の突進型
        Stationary,  // 静止砲撃型 (演出用弾・当たり判定なし)
        Swarm,       // 小型並走群れ型 (並走追従し数秒後攻撃)
        Boss         // ボス
    };

    void SetEnemyType(EnemyType type) { enemyType_ = type; }
    EnemyType GetEnemyType() const { return enemyType_; }

    void SetSpawnRadius(float radius) { spawnRadius_ = radius; }
    float GetSpawnRadius() const { return spawnRadius_; }

    void SetSpawnCount(int count) { spawnCount_ = count; }
    int GetSpawnCount() const { return spawnCount_; }

    void SetHangTime(float seconds) { hangTime_ = seconds; }
    float GetHangTime() const { return hangTime_; }

    bool IsVisualOnlyBullet() const { return isVisualOnlyBullet_; }
    void SetVisualOnlyBullet(bool visualOnly) { isVisualOnlyBullet_ = visualOnly; }

    enum class AiState {
        Approach,          // プレイヤーの視界に入るように接近
        TargetLock,        // 視界に入り、画面中央へ移動するためのチャージ蓄積
        MoveToCenter,      // プレイヤーの進行方向真正面ラインへ素早く移動
        AttackCharge,      // 中央到達後、攻撃チャージ
        UnavoidableAttack, // 回避困難な弾を発射
        StationaryShooting,// 静止砲撃（見た目のみ射撃）
        SwarmHanging,      // 小型群れの並走追従
        SwarmAttacking     // 小型群れの突撃/攻撃
    };

    void SetAiState(AiState state) { aiState_ = state; }
    AiState GetAiState() const { return aiState_; }
    float GetToCenterGauge() const { return toCenterGauge_; }
    float GetAttackGauge() const { return attackGauge_; }
    void SetBoss(bool isBoss);
    bool IsBoss() const { return isBoss_; }
    void SetSpawnPoint(bool active);
    bool IsSpawnPoint() const { return isSpawnPoint_; }
    void Damage(int amount, const std::string& effectName);

private:
    // マジックナンバー排除のための定数
    static inline const Vector3 kInitialPosition = { 0.0f, 0.0f, 6.0f };    // 初期位置
    static inline const Vector3 kEnemyScale = { 1.0f, 1.0f, 1.0f };         // サイズ
    static inline const Vector3 kBulletVelocity = { 0.0f, 0.0f, -0.15f };   // 弾の初速度
    static inline const float kSpeed = 0.05f;                               // 移動速度
    static inline const float kMoveLimitX = 8.0f;                           // 左右移動限界
    static inline const std::string kTextureKey = "white";                  // テクスチャ
    static inline const std::string kEnvMapKey = "";                        // 環境マップ
    static inline const int kMinMoveTime = 60;                              // 移動方向転換の最小フレーム
    static inline const int kMaxMoveTime = 180;                             // 移動方向転換の最大フレーム
    static inline const int kMinShotTime = 60;                              // 射撃間隔の最小フレーム
    static inline const int kMaxShotTime = 150;                             // 射撃間隔の最大フレーム
    static inline const Vector4 kEnemyColor = { 1.0f, 0.1f, 0.1f, 1.0f };    // 敵のカラー（赤）
    static inline const std::string kBulletEffectName = "PurpleFire";       // 弾エフェクト名（紫の炎）

    // 頭部およびコライダーのローカル定数
    static inline const Vector3 kHeadLocalPos = { 0.0f, 1.0f, 0.0f };
    static inline const Vector3 kHeadLocalScale = { 0.6f, 0.6f, 0.6f };
    static inline const Vector4 kHeadColor = { 1.0f, 0.8f, 0.8f, 1.0f };
    static inline const Vector3 kBodyColliderSize = { 1.0f, 1.0f, 1.0f };
    static inline const Vector3 kHeadColliderSize = { 0.6f, 0.6f, 0.6f };

private:
    std::unique_ptr<CubeObject> cube_;                 // 胴体モデル
    std::unique_ptr<CubeObject> headCube_;             // 頭部モデル
    std::unique_ptr<PartCollider> bodyCollider_;       // 胴体コライダー
    std::unique_ptr<PartCollider> headCollider_;       // 頭部コライダー
    Player* targetPlayer_ = nullptr;                   // 自機へのポインタ

    // 移動および射撃の制御用タイマーと状態
    float moveDirection_ = 1.0f; // 1.0f または -1.0f
    int moveTimer_ = 0;
    int shotTimer_ = 0;

    // AIステート関連のメンバ変数
    EnemyType enemyType_ = EnemyType::Normal;
    AiState aiState_ = AiState::Approach;
    float toCenterGauge_ = 0.0f;
    float attackGauge_ = 0.0f;
    bool isBoss_ = false;
    bool isSpawnPoint_ = false;                      // エディタ用の出現サークルかどうか
    int hitFlashTimer_ = 0;                          // 被弾時の黄色フラッシュタイマー
    float spawnRadius_ = 5.0f;                       // 発生範囲（半径）
    int spawnCount_ = 1;                             // 発生個数
    float hangTime_ = 3.0f;                          // 小型群れ型の並走時間（秒）
    float hangTimer_ = 0.0f;                         // 小型群れ型の並走経過タイマー
    bool isVisualOnlyBullet_ = false;                // 演出専用弾フラグ（当たり判定なし）

    // AI用定数
    static inline const float kApproachLookDistance = 15.0f; // プレイヤーの何メートル前方に入ったら視界内とするか
    static inline const float kApproachSpeedZ = 0.12f;       // 接近速度
    static inline const float kCenterSpeedX = 0.15f;         // 中央へのスライド速度
    static inline const float kToCenterGaugeMax = 1.0f;
    static inline const float kAttackGaugeMax = 1.0f;
    static inline const float kGaugeIncreaseRate = 0.008f;    // 1フレームあたりのゲージ増加量
    static inline const float kBossHpMultiplier = 10.0f;     // ボスのHP倍率
    static inline const int kNormalEnemyHp = 3;              // 敵の通常HP
    static inline const int kBossEnemyHp = 30;               // ボスのHP

    // 乱数生成器
    std::mt19937 randomEngine_;

    int maxHp_ = 10;
    Vector3 size_ = kEnemyScale;
    void UpdateHpBar(const Vector3& charPos);
};
