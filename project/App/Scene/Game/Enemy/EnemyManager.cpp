#include "App/Scene/Game/Enemy/EnemyManager.h"
#include "App/Scene/Game/Player/Player.h"
#include "App/Scene/Game/Camera/PlayCamera.h"
#include "Engine/Input/Input.h"
#include "Engine/Math/Collision/Collision.h"
#include "Engine/Graphics/Objects/Effect/Manager/EffectManager.h"
#include "App/Scene/Core/SceneManager.h"
#include "Engine/Debug/SceneHierarchy.h"

#include <cmath>
#include <algorithm>
#include <cstdlib>

EnemyManager::EnemyManager() = default;

/**
 * @brief 初期化処理
 * @param input キー入力・マウス入力のマネージャ
 * @param playCamera プレイ用のメイン追従カメラ
 */
void EnemyManager::Initialize(Input* input, PlayCamera* playCamera) {
    input_ = input;
    playCamera_ = playCamera;
    Reset();
}

void EnemyManager::Reset() {
#ifdef _USEIMGUI
    SceneHierarchy::GetInstance()->SetSelected(nullptr);
#endif
    enemies_.clear();
    spawnTriggers_.clear();
    lastSpawnZ_ = 0.0f;
    hasBossSpawned_ = false;
    hasPrevPlayerPos_ = false;
}

/**
 * @brief エディタ上に配置されたサークル（ダミー敵）情報を元に湧きトリガーを設定する
 * @param editorEnemies エディタが保持しているダミー敵リスト
 * @param startPlayerZ プレイヤーの初期Z座標
 */
void EnemyManager::SetupSpawnTriggers(const std::vector<std::unique_ptr<Enemy>>& editorEnemies, float startPlayerZ) {
    spawnTriggers_.clear();
    for (const auto& enemy : editorEnemies) {
        if (enemy->IsSpawnPoint()) {
            SpawnTrigger trigger;
            trigger.pos = enemy->GetPosition();
            trigger.z = enemy->GetPosition().z;
            trigger.count = enemy->GetSpawnCount();
            trigger.radius = enemy->GetSpawnRadius();
            trigger.hangTime = enemy->GetHangTime();
            trigger.type = enemy->GetEnemyType();
            trigger.triggered = false;
            spawnTriggers_.push_back(trigger);
        }
    }
#ifdef _USEIMGUI
    SceneHierarchy::GetInstance()->SetSelected(nullptr);
#endif
    enemies_.clear(); // エディタ用ダミーサークルをクリア
    lastSpawnZ_ = startPlayerZ;
    hasBossSpawned_ = false;
    hasPrevPlayerPos_ = false;
}

void EnemyManager::Update(Player* player) {
    if (!player) return;

    Vector3 playerPos = player->GetPosition();
    Vector3 tangent = player->GetDirection();

    // 1. 動的湧き・ボス戦湧き処理
    // (A) エディタトリガーによる湧き判定 (XZ平面上の円形通過判定)
    Vector3 segStart = hasPrevPlayerPos_ ? prevPlayerPos_ : playerPos;
    Vector3 segEnd = playerPos;

    for (auto& trigger : spawnTriggers_) {
        if (!trigger.triggered && IsIntersectingCircleXZ(segStart, segEnd, trigger.pos, trigger.radius)) {
            trigger.triggered = true;

            for (int i = 0; i < trigger.count; ++i) {
                auto enemy = std::make_unique<Enemy>();
                enemy->Initialize();
                enemy->SetSpawnPoint(false);
                enemy->SetEnemyType(trigger.type);
                enemy->SetTargetPlayer(player);
                enemy->SetHangTime(trigger.hangTime);

                if (trigger.type == Enemy::EnemyType::Stationary) {
                    // 静止砲撃型: 指定された設置座標そのまま
                    enemy->SetPosition(trigger.pos);
                    enemy->SetAiState(Enemy::AiState::StationaryShooting);
                } else if (trigger.type == Enemy::EnemyType::Swarm) {
                    // 小型並走群れ型: 範囲内（radius）に少しランダムオフセット
                    float angle = (static_cast<float>(rand()) / RAND_MAX) * 3.14159f * 2.0f;
                    float r = (static_cast<float>(rand()) / RAND_MAX) * trigger.radius;
                    Vector3 offset = { std::cos(angle) * r, 0.0f, std::sin(angle) * r };
                    enemy->SetPosition(Math::Add(trigger.pos, offset));
                    enemy->SetSize({ 0.4f, 0.4f, 0.4f }); // 小型化
                    enemy->SetAiState(Enemy::AiState::SwarmHanging);
                } else {
                    // 通常突進型
                    Vector3 rightVec = { tangent.z, 0.0f, -tangent.x };
                    float spawnDistBack = -10.0f - static_cast<float>(i) * 3.0f;
                    float spawnDistSide = (i % 2 == 0 ? 10.0f : -10.0f) + (static_cast<float>(i / 2) * 1.5f);
                    
                    Vector3 spawnPos = Math::Add(
                        playerPos, 
                        Math::Add(
                            Math::Multiply(spawnDistBack, tangent),
                            Math::Multiply(spawnDistSide, rightVec)
                        )
                    );
                    enemy->SetPosition(spawnPos);
                    enemy->SetAiState(Enemy::AiState::Approach);
                }

                enemies_.push_back(std::move(enemy));
            }
        }
    }

    // (B) 通常の定期的な敵の湧き判定 (120.0f ごと)
    if (playerPos.z - lastSpawnZ_ >= kSpawnIntervalZ) {
        lastSpawnZ_ = playerPos.z;
        
        Vector3 rightVec = { tangent.z, 0.0f, -tangent.x };
        static const float kSpawnDistBack = -10.0f;
        static const float kSpawnDistSide = 10.0f;
        
        Vector3 spawnPos = Math::Add(
            playerPos, 
            Math::Add(
                Math::Multiply(kSpawnDistBack, tangent),
                Math::Multiply((rand() % 2 == 0 ? kSpawnDistSide : -kSpawnDistSide), rightVec)
            )
        );

        auto enemy = std::make_unique<Enemy>();
        enemy->Initialize();
        enemy->SetSpawnPoint(false);
        enemy->SetPosition(spawnPos);
        enemy->SetTargetPlayer(player);
        enemy->SetAiState(Enemy::AiState::Approach);
        
        enemies_.push_back(std::move(enemy));
    }

    // (C) ボス戦の湧き判定 (Z >= 180.0f 以降に1回だけ)
    if (playerPos.z >= kBossSpawnZ && !hasBossSpawned_) {
        hasBossSpawned_ = true;

        Vector3 bossSpawnPos = Math::Add(playerPos, Math::Multiply(20.0f, tangent));
        bossSpawnPos.x = 0.0f; // 正面中央

        auto boss = std::make_unique<Enemy>();
        boss->Initialize();
        boss->SetBoss(true);
        boss->SetPosition(bossSpawnPos);
        boss->SetTargetPlayer(player);
        boss->SetAiState(Enemy::AiState::Approach);

        enemies_.push_back(std::move(boss));
    }

    // 2. 敵 (Enemy) の更新と死後消滅判定 (カリング適用)
    for (auto it = enemies_.begin(); it != enemies_.end();) {
        float distZ = std::abs((*it)->GetPosition().z - playerPos.z);
        if (distZ > kCullingDistance) {
            // はるか後方に置き去りにされた敵は、追いつけないため消滅させてリソース節約
            if ((*it)->GetPosition().z < playerPos.z - kCullingDistance) {
                it = enemies_.erase(it);
            } else {
                ++it;
            }
            continue;
        }

        (*it)->SetTargetPlayer(player);
        (*it)->Update();
        if ((*it)->IsDead()) {
            it = enemies_.erase(it);
        } else {
            ++it;
        }
    }

    // 3. 衝突判定 (自機の被弾判定)
    Vector3 playerSize = player->GetSize();
    AABB playerAABB;
    playerAABB.min = { playerPos.x - playerSize.x * kHalf, playerPos.y - playerSize.y * kHalf, playerPos.z - playerSize.z * kHalf };
    playerAABB.max = { playerPos.x + playerSize.x * kHalf, playerPos.y + playerSize.y * kHalf, playerPos.z + playerSize.z * kHalf };

    for (auto& enemy : enemies_) {
        const auto& enemyBullets = enemy->GetBullets();
        for (auto& bullet : enemyBullets) {
            if (!bullet->IsActive() || bullet->IsVisualOnly()) continue;

            Sphere bulletSphere;
            bulletSphere.center = bullet->GetPosition();
            bulletSphere.radius = kEnemyBulletRadius;

            if (IsCollision(playerAABB, bulletSphere)) {
                player->Damage(kPlayerTakenDamage, bullet->GetEffectName());
                bullet->Kill(); // 被弾した弾を非アクティブ化

                if (playCamera_) {
                    playCamera_->TriggerShake();
                }
            }
        }
    }

    // プレイヤーの死亡（ゲームオーバー）判定
    if (player->IsDead()) {
        SceneManager::GetInstance()->ChangeScene(kGameOverSceneName);
        return;
    }

    // 4. プレイヤーの弾と敵の衝突判定 (物理弾判定)
    const auto& playerBullets = player->GetBullets();
    for (auto& bullet : playerBullets) {
        if (!bullet->IsActive()) continue;

        Sphere bulletSphere;
        bulletSphere.center = bullet->GetPosition();
        bulletSphere.radius = kBulletCollisionRadius;

        for (auto& enemy : enemies_) {
            if (enemy->IsSpawnPoint() || enemy->IsDead()) continue;

            AABB headAABB = enemy->GetHeadCollider()->GetWorldAABB();
            AABB bodyAABB = enemy->GetBodyCollider()->GetWorldAABB();

            // 頭部（クリティカル）優先
            if (IsCollision(headAABB, bulletSphere)) {
                enemy->Damage(kCriticalDamage, player->GetBulletEffectName());
                bullet->Kill();
                if (playCamera_) {
                    playCamera_->TriggerShake();
                }
                break;
            }
            // 胴体判定
            else if (IsCollision(bodyAABB, bulletSphere)) {
                enemy->Damage(kNormalDamage, player->GetBulletEffectName());
                bullet->Kill();
                if (playCamera_) {
                    playCamera_->TriggerShake();
                }
                break;
            }
        }
    }

    // 次フレームの判定用に入力自機位置を記録
    prevPlayerPos_ = playerPos;
    hasPrevPlayerPos_ = true;
}

void EnemyManager::Draw(const Vector3& playerPos) {
    for (auto& enemy : enemies_) {
        float distZ = std::abs(enemy->GetPosition().z - playerPos.z);
        if (distZ <= kCullingDistance) {
            enemy->Draw();
        }
    }
}

bool EnemyManager::IsCollision(const AABB& aabb, const Sphere& sphere) const {
    return ::IsCollision(aabb, sphere);
}

bool EnemyManager::IsCollidingAABB(const Vector3& pos1, const Vector3& size1, const Vector3& pos2, const Vector3& size2) const {
    float minX1 = pos1.x - size1.x * kHalf;
    float maxX1 = pos1.x + size1.x * kHalf;
    float minY1 = pos1.y - size1.y * kHalf;
    float maxY1 = pos1.y + size1.y * kHalf;
    float minZ1 = pos1.z - size1.z * kHalf;
    float maxZ1 = pos1.z + size1.z * kHalf;

    float minX2 = pos2.x - size2.x * kHalf;
    float maxX2 = pos2.x + size2.x * kHalf;
    float minY2 = pos2.y - size2.y * kHalf;
    float maxY2 = pos2.y + size2.y * kHalf;
    float minZ2 = pos2.z - size2.z * kHalf;
    float maxZ2 = pos2.z + size2.z * kHalf;

    return (minX1 <= maxX2 && maxX1 >= minX2) &&
           (minY1 <= maxY2 && maxY1 >= minY2) &&
           (minZ1 <= maxZ2 && maxZ1 >= minZ2);
}

/**
 * @brief 2D平面(XZ)における線分と円の交差判定 (すり抜け防止最近接距離判定)
 * @param segStart 線分の始点 (前フレーム位置)
 * @param segEnd 線分の終点 (現フレーム位置)
 * @param circleCenter 円の中心座標
 * @param radius 円の半径
 * @return 線分が円に交差または包含している場合 true
 */
bool EnemyManager::IsIntersectingCircleXZ(const Vector3& segStart, const Vector3& segEnd, const Vector3& circleCenter, float radius) {
    float segX = segEnd.x - segStart.x;
    float segZ = segEnd.z - segStart.z;
    float segLenSq = segX * segX + segZ * segZ;

    static constexpr float kEpsilon = 1e-6f;
    static constexpr float kZero = 0.0f;
    static constexpr float kOne = 1.0f;

    float t = kZero;
    if (segLenSq > kEpsilon) {
        float toCenterX = circleCenter.x - segStart.x;
        float toCenterZ = circleCenter.z - segStart.z;
        t = (toCenterX * segX + toCenterZ * segZ) / segLenSq;
        t = std::clamp(t, kZero, kOne);
    }

    float closestX = segStart.x + t * segX;
    float closestZ = segStart.z + t * segZ;

    float dx = closestX - circleCenter.x;
    float dz = closestZ - circleCenter.z;
    float distSq = dx * dx + dz * dz;

    return distSq <= (radius * radius);
}
