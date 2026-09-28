#pragma once
#include <vector>
#include <memory>
#include "App/Scene/Game/Enemy/Enemy.h"
#include "Engine/Math/MathStructs.h"

class Player;
class PlayCamera;
class Input;

/**
 * @brief 敵オブジェクトの動的湧き、描画カリング、および衝突判定を統括管理するクラス
 */
class EnemyManager {
public:
    EnemyManager();
    ~EnemyManager() = default;

    // 初期化処理
    void Initialize(Input* input, PlayCamera* playCamera);

    // 毎フレーム更新 (湧き、移動更新、衝突判定)
    void Update(Player* player);

    // 描画処理 (60.0fのカリングを適用)
    void Draw(const Vector3& playerPos);

    // 開始時の湧きトリガー設定
    void SetupSpawnTriggers(const std::vector<std::unique_ptr<Enemy>>& editorEnemies, float startPlayerZ);

    // 敵リストの取得
    std::vector<std::unique_ptr<Enemy>>& GetEnemies() { return enemies_; }

    // 敵リストのクリア
    void ClearEnemies() { enemies_.clear(); }

    // 敵および湧きトリガーのリセット
    void Reset();

private:
    // 衝突判定ヘルパー関数群
    bool IsCollision(const AABB& aabb, const Sphere& sphere) const;
    bool IsCollidingAABB(const Vector3& pos1, const Vector3& size1, const Vector3& pos2, const Vector3& size2) const;

private:
    // 湧きトリガーデータ
    struct SpawnTrigger {
        Vector3 pos;               // スポーン中心座標
        float z;                   // 湧き判定Z座標
        int count = 1;             // 湧く数
        float radius = 5.0f;       // 発生範囲
        float hangTime = 3.0f;     // 並走時間
        Enemy::EnemyType type = Enemy::EnemyType::Normal; // 敵タイプ
        bool triggered = false;    // 湧いたかフラグ
    };

    // マジックナンバー排除のための定数
    static inline const float kCullingDistance = 60.0f;       // 描画および生存カリングの距離
    static inline const float kSpawnIntervalZ = 120.0f;       // 定期湧きするZ間隔
    static inline const float kBossSpawnZ = 180.0f;           // ボスが湧くZ座標
    static inline const float kBulletCollisionRadius = 0.4f;  // 自機弾の当たり判定半径
    static inline const float kEnemyBulletRadius = 0.5f;      // 敵弾の当たり判定半径
    static inline const int kNormalDamage = 1;                // 胴体への通常被ダメージ
    static inline const int kCriticalDamage = 2;              // 頭部へのクリティカル被ダメージ
    static inline const int kPlayerTakenDamage = 10;          // プレイヤーが敵弾から受ける被ダメージ
    static inline const std::string kGameOverSceneName = "GameOver"; // ゲームオーバーシーン名

    static inline const float kHalf = 0.5f;

private:
    Input* input_ = nullptr;
    PlayCamera* playCamera_ = nullptr;

    std::vector<std::unique_ptr<Enemy>> enemies_;             // 敵オブジェクト配列
    std::vector<SpawnTrigger> spawnTriggers_;                 // 湧きトリガー配列
    float lastSpawnZ_ = 0.0f;                                 // 最後に定期湧きしたZ座標
    bool hasBossSpawned_ = false;                             // ボス出現済みフラグ
};
