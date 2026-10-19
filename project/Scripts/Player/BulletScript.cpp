#include "Scripts/Player/BulletScript.h"
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
    constexpr float kHitShakeIntensity = 0.05f;
    constexpr float kHitShakeDuration = 0.10f;
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
            // 対象または子オブジェクトの BoxColliderComponent を走査
            auto* boxCol = targetObj->GetComponent<BoxColliderComponent>();
            if (boxCol) {
                // 名前で敵関連か判定（"Enemy", "Boss", "Head", "Body" 等）
                const std::string& name = targetObj->GetName();
                if (name.find("Enemy") != std::string::npos ||
                    name.find("Boss") != std::string::npos ||
                    name.find("Head") != std::string::npos ||
                    name.find("Body") != std::string::npos) {

                    AABB targetAABB = boxCol->GetWorldAABB();
                    if (IsCollision(targetAABB, bulletSphere)) {
                        // 命中フラグ設定
                        isDead_ = true;
                        owner_->SetVisible(false);
                        ShakeCamera(kHitShakeIntensity, kHitShakeDuration);
                        Destroy(owner_);
                        break;
                    }
                }
            }
        }
        // 敵弾の場合：プレイヤーを対象とする
        else {
            const std::string& name = targetObj->GetName();
            if (name == "Player" || name.find("Player") != std::string::npos) {
                auto* boxCol = targetObj->GetComponent<BoxColliderComponent>();
                if (boxCol) {
                    AABB targetAABB = boxCol->GetWorldAABB();
                    if (IsCollision(targetAABB, bulletSphere)) {
                        isDead_ = true;
                        owner_->SetVisible(false);
                        ShakeCamera(kHitShakeIntensity, kHitShakeDuration);
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
