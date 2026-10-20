#include "Scripts/Player/BulletScript.h"
#include "Scripts/Player/PlayerControllerScript.h"
#include "Scripts/Enemy/EnemyAIScript.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/ScriptRegistry.h"
#include "Engine/Component/Components/BoxColliderComponent.h"
#include "Engine/Component/Components/SphereColliderComponent.h"
#include "Engine/Math/Collision/Collision.h"
#include "App/Scene/Core/SceneManager.h"
#include "App/Scene/Generic/GenericScene.h"
#include "Engine/Debug/SceneHierarchy.h"
#ifdef _USEIMGUI
#include <imgui.h>
#endif

REGISTER_SCRIPT(BulletScript)

BulletScript::BulletScript()
    : ScriptComponent("BulletScript") {
}

void BulletScript::Awake() {
    currentLifeTimer_ = 0;
    isDead_ = false;
}

void BulletScript::Start() {
}

namespace {
    constexpr float kHitShakeIntensity = 0.05f;       // 通常ヒット時の反動シェイク振幅
    constexpr float kHitShakeDuration = 0.10f;        // 通常ヒット持続時間
    constexpr float kCriticalShakeIntensity = 0.12f;  // ヘッドショットクリティカル時の強力な反動シェイク振幅
    constexpr float kCriticalShakeDuration = 0.18f;   // クリティカル持続時間
    constexpr float kPlayerHurtShakeIntensity = 0.16f;// 自機被弾時の衝撃シェイク振幅
    constexpr float kPlayerHurtShakeDuration = 0.22f; // 被弾衝撃持続時間

    // 当たり判定対象の識別キーワード
    const std::string kHeadKeyword = "Head";
    const std::string kBodyKeyword = "Body";
    const std::string kEnemyKeyword = "Enemy";
    const std::string kBossKeyword = "Boss";
    const std::string kPlayerKeyword = "Player";
}

void BulletScript::OnUpdate() {
    if (isDead_ || !owner_) {
        return;
    }

    // 1. 寿命タイマーの更新
    currentLifeTimer_++;
    if (currentLifeTimer_ >= maxLifeTime_) {
        isDead_ = true;
        owner_->SetVisible(false);
        Destroy(owner_);
        return;
    }

    // 2. 弾の移動処理
    Vector3 currentPos = owner_->GetPosition();
    currentPos.x += direction_.x * speed_;
    currentPos.y += direction_.y * speed_;
    currentPos.z += direction_.z * speed_;
    owner_->SetPosition(currentPos);

    // 3. 衝突判定の確認
    CheckCollisions();
}

/// <summary>
/// 弾丸の球体コライダーとシーン上の各オブジェクトのAABB（軸平行境界ボックス）との幾何交差判定を実行します。
/// 
/// 【処理の流れ】
/// 1. 自身の座標と半径から Sphere 構造体を構築。
/// 2. シーン内のアクティブな GameObject を走査し、自身と同一または非表示のオブジェクトを除外。
/// 3. 自機弾（isPlayerBullet_ == true）の場合:
///    - 敵関連（Enemy, Boss, Head, Body）の BoxColliderComponent を検出。
///    - IsCollision(AABB, Sphere) で衝突を検知した場合、部位が "Head" ならばクリティカル判定とし、
///      対象オブジェクトまたはその親オブジェクトから EnemyAIScript を取得して TakeDamage(damage_, isCritical) を呼出。
/// 4. 敵弾（isPlayerBullet_ == false）の場合:
///    - 自機（Player）の BoxColliderComponent を検出。
///    - 衝突時に PlayerControllerScript::TakeDamage(damage_) を呼出。
/// 5. 命中時は反動カメラシェイクを誘発し、自身の非表示フラグを設定してシーンから破棄（Destroy）。
/// </summary>
void BulletScript::CheckCollisions() {
    if (!owner_ || isDead_) return;

    Vector3 bulletPos = owner_->GetPosition();
    Sphere bulletSphere;
    bulletSphere.center = bulletPos;
    bulletSphere.radius = collisionRadius_;

    // シーン上の GameObject 群を取得して判定
    auto* scene = dynamic_cast<GenericScene*>(SceneManager::GetInstance()->GetCurrentScene());
    if (!scene) return;

    const auto& gameObjects = scene->GetGameObjects();

    for (const auto& targetObj : gameObjects) {
        if (!targetObj || targetObj.get() == owner_ || !targetObj->IsVisible()) {
            continue;
        }

        // 自機弾の場合：敵タグまたはEnemyAIScriptを持つオブジェクトを対象とする
        if (isPlayerBullet_) {
            auto* boxCol = targetObj->GetComponent<BoxColliderComponent>();
            if (boxCol) {
                const std::string& name = targetObj->GetName();
                bool isEnemyPart = (name.find(kEnemyKeyword) != std::string::npos ||
                                    name.find(kBossKeyword) != std::string::npos ||
                                    name.find(kHeadKeyword) != std::string::npos ||
                                    name.find(kBodyKeyword) != std::string::npos);

                if (isEnemyPart) {
                    AABB targetAABB = boxCol->GetWorldAABB();
                    if (IsCollision(targetAABB, bulletSphere)) {
                        // 部位別判定（頭部ならヘッドショットクリティカル）
                        bool isCritical = (name.find(kHeadKeyword) != std::string::npos);

                        // 敵AIコンポーネントを対象オブジェクトから探索
                        EnemyAIScript* enemyAI = targetObj->GetComponent<EnemyAIScript>();
                        if (enemyAI) {
                            enemyAI->TakeDamage(damage_, isCritical);
                        }

                        // 命中フラグ設定と弾丸の破棄
                        isDead_ = true;
                        owner_->SetVisible(false);
                        float shakeIntensity = isCritical ? kCriticalShakeIntensity : kHitShakeIntensity;
                        float shakeDuration = isCritical ? kCriticalShakeDuration : kHitShakeDuration;
                        ShakeCamera(shakeIntensity, shakeDuration);
                        Destroy(owner_);
                        break;
                    }
                }
            }
        }
        // 敵弾の場合：プレイヤーを対象とする
        else {
            const std::string& name = targetObj->GetName();
            if (name == kPlayerKeyword || name.find(kPlayerKeyword) != std::string::npos) {
                auto* boxCol = targetObj->GetComponent<BoxColliderComponent>();
                if (boxCol) {
                    AABB targetAABB = boxCol->GetWorldAABB();
                    if (IsCollision(targetAABB, bulletSphere)) {
                        // プレイヤーコントローラーを取得してダメージ適用
                        auto* playerScript = targetObj->GetComponent<PlayerControllerScript>();
                        if (playerScript) {
                            playerScript->TakeDamage(damage_);
                        }

                        // 命中フラグ設定と弾丸の破棄
                        isDead_ = true;
                        owner_->SetVisible(false);
                        ShakeCamera(kPlayerHurtShakeIntensity, kPlayerHurtShakeDuration);
                        Destroy(owner_);
                        break;
                    }
                }
            }
        }
    }
}

void BulletScript::OnInspectorGUI() {
#ifdef _USEIMGUI
    constexpr ImVec4 kHeaderColor = { 1.0f, 0.85f, 0.3f, 1.0f };
    ImGui::TextColored(kHeaderColor, "【弾丸スクリプト設定】");
    ImGui::Separator();

    ImGui::DragFloat("弾速##BulletSpeed", &speed_, 0.05f, kMinSpeed, kMaxSpeed, "%.2f");
    ImGui::DragInt("最大寿命##MaxLife", &maxLifeTime_, 1, kMinLifeTime, kMaxLifeTimeLimit);
    ImGui::DragFloat("当たり判定半径##ColRadius", &collisionRadius_, 0.02f, 0.05f, 2.0f, "%.2f");
    ImGui::DragInt("威力##Damage", &damage_, 1, 1, 100);
    ImGui::Checkbox("自機弾フラグ##IsPlayerBullet", &isPlayerBullet_);

    ImGui::Text("寿命進行: %d / %d", currentLifeTimer_, maxLifeTime_);
    ImGui::Text("状態: %s", isDead_ ? "消滅(Dead)" : "アクティブ(Alive)");
#endif
}
