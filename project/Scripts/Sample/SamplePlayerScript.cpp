#include "Scripts/Sample/SamplePlayerScript.h"
#include "Engine/Component/ScriptRegistry.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/Components/BoxColliderComponent.h"
#include "Engine/Math/Collision/Collision.h"
#include "Engine/Debug/SceneHierarchy.h"
#include "Engine/Input/Input.h"
#include "Engine/Base/BaseResource.h"
#include "Scripts/Player/BulletScript.h"
#include <cmath>

#ifdef _USEIMGUI
#include "externals/imgui/imgui.h"
#endif

// スクリプト自動登録マクロ（エディタに認識させる）
REGISTER_SCRIPT(SamplePlayerScript)

namespace {
    constexpr float kFixedDeltaTime = 1.0f / 60.0f;
    constexpr float kMinMoveSpeed = 1.0f;
    constexpr float kMaxMoveSpeed = 30.0f;
    constexpr float kMinOrbitSpeed = 0.1f;
    constexpr float kMaxOrbitSpeed = 5.0f;
    constexpr float kMinOrbitRadius = 1.0f;
    constexpr float kMaxOrbitRadius = 20.0f;
    constexpr float kTwoPi = 6.2831853f;
    constexpr float kShootCooldownDuration = 0.18f;
    constexpr float kMuzzleOffsetDistance = 1.2f;
    constexpr float kShootShakeIntensity = 0.08f;
    constexpr float kShootShakeDuration = 0.12f;
    constexpr float kMinBulletSpeed = 0.1f;
    constexpr float kMaxBulletSpeed = 3.0f;
}

SamplePlayerScript::SamplePlayerScript()
    : ScriptComponent("SamplePlayerScript") {
}

void SamplePlayerScript::Awake() {
    if (owner_) {
        initialPosition_ = owner_->GetPosition();
    }
}

void SamplePlayerScript::Start() {
    if (owner_) {
        initialPosition_ = owner_->GetPosition();
    }
}

void SamplePlayerScript::OnUpdate() {
    if (!owner_) return;

    if (enableAutoOrbit_) {
        // 自動旋回モード（ターゲットの動きを自動シミュレート）
        orbitAngle_ += orbitSpeed_ * kFixedDeltaTime;
        if (orbitAngle_ > kTwoPi) {
            orbitAngle_ -= kTwoPi;
        }

        Vector3 newPos = initialPosition_;
        newPos.x += std::cos(orbitAngle_) * orbitRadius_;
        newPos.z += std::sin(orbitAngle_) * orbitRadius_;
        owner_->SetPosition(newPos);
    } else {
        // キーボード操作モード (WASD または 矢印キー)
        auto input = InputResource::GetInput();
        if (!input) return;

        Vector3 moveDir = { 0.0f, 0.0f, 0.0f };

        if (input->Press(DIK_W) || input->Press(DIK_UP)) {
            moveDir.z += 1.0f;
        }
        if (input->Press(DIK_S) || input->Press(DIK_DOWN)) {
            moveDir.z -= 1.0f;
        }
        if (input->Press(DIK_A) || input->Press(DIK_LEFT)) {
            moveDir.x -= 1.0f;
        }
        if (input->Press(DIK_D) || input->Press(DIK_RIGHT)) {
            moveDir.x += 1.0f;
        }

        // 移動ベクトルが入力されていれば正規化して適用
        float lengthSq = moveDir.x * moveDir.x + moveDir.z * moveDir.z;
        if (lengthSq > 0.0001f) {
            float invLen = 1.0f / std::sqrt(lengthSq);
            moveDir.x *= invLen;
            moveDir.z *= invLen;

            Vector3 currentPos = owner_->GetPosition();
            currentPos.x += moveDir.x * moveSpeed_ * kFixedDeltaTime;
            currentPos.z += moveDir.z * moveSpeed_ * kFixedDeltaTime;
            owner_->SetPosition(currentPos);
        }

        // 射撃クールダウン更新
        if (shootCooldown_ > 0.0f) {
            shootCooldown_ -= kFixedDeltaTime;
        }

        // マウス左クリックで弾丸（Bullet）発射 ＆ カメラシェイク
        if (input->MouseTrigger(0) && shootCooldown_ <= 0.0f) {
            shootCooldown_ = kShootCooldownDuration;

            Vector3 currentPos = owner_->GetPosition();
            Vector3 currentRot = owner_->GetRotate();

            // プレイヤーの向き（Y軸回転）から発射ベクトルを計算
            Vector3 forwardDir = {
                std::sin(currentRot.y),
                0.0f,
                std::cos(currentRot.y)
            };
            if (forwardDir.x == 0.0f && forwardDir.z == 0.0f) {
                forwardDir.z = 1.0f;
            }

            Vector3 muzzlePos = {
                currentPos.x + forwardDir.x * kMuzzleOffsetDistance,
                currentPos.y,
                currentPos.z + forwardDir.z * kMuzzleOffsetDistance
            };

            // プレハブ動的生成API（Instantiate）
            GameObject* bulletObj = Instantiate("Bullet", muzzlePos, currentRot);
            if (bulletObj) {
                auto* bulletScript = bulletObj->GetComponent<BulletScript>();
                if (bulletScript) {
                    bulletScript->SetDirection(forwardDir);
                    bulletScript->SetSpeed(bulletSpeed_);
                    bulletScript->SetIsPlayerBullet(true);
                }
            }

            // カメラシェイク演出呼び出し
            ShakeCamera(kShootShakeIntensity, kShootShakeDuration);
        }
    }

    // 他のBoxColliderとの押し返し判定（重なり解消および接触判定）
    if (auto* selfCol = owner_->GetComponent<BoxColliderComponent>()) {
        const auto& allObjects = SceneHierarchy::GetInstance()->GetObjects();
        bool anyColliding = false;
        Vector3 currentPos = owner_->GetPosition();

        for (auto* obj : allObjects) {
            auto* go = dynamic_cast<GameObject*>(obj);
            if (!go || go == owner_) continue;

            if (auto* otherCol = go->GetComponent<BoxColliderComponent>()) {
                OBB otherOBB = otherCol->GetWorldOBB();
                OBB selfOBB = selfCol->GetWorldOBB();

                Vector3 pushVec{};
                Vector3 contactNormal{};
                if (CalculateOBBPushVector(otherOBB, selfOBB, pushVec, &contactNormal)) {
                    // めり込みが発生していれば押し戻して重なりを解消
                    currentPos.x += pushVec.x;
                    currentPos.y += pushVec.y;
                    currentPos.z += pushVec.z;
                    owner_->SetPosition(currentPos);

                    anyColliding = true;
                    otherCol->SetColliding(true);
                }
            }
        }
        selfCol->SetColliding(anyColliding);
    }
}

void SamplePlayerScript::OnInspectorGUI() {
#ifdef _USEIMGUI
    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.5f, 1.0f), "【参考例】プレイヤー移動スクリプト");
    ImGui::TextDisabled("WASD / 矢印キーでXZ平面を移動します。");
    ImGui::TextDisabled("マウス左クリックで弾丸を発射し、カメラが揺れます。");
    ImGui::Separator();

    ImGui::DragFloat("移動速度##MoveSpeed", &moveSpeed_, 0.2f, kMinMoveSpeed, kMaxMoveSpeed, "%.1f");
    ImGui::Checkbox("自動旋回テスト##AutoOrbit", &enableAutoOrbit_);

    if (enableAutoOrbit_) {
        ImGui::DragFloat("旋回半径##OrbitRadius", &orbitRadius_, 0.2f, kMinOrbitRadius, kMaxOrbitRadius, "%.1f");
        ImGui::DragFloat("旋回速度##OrbitSpeed", &orbitSpeed_, 0.1f, kMinOrbitSpeed, kMaxOrbitSpeed, "%.1f");
    }

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "【射撃・演出パラメータ】");
    ImGui::DragFloat("弾速##BulletSpeed", &bulletSpeed_, 0.05f, kMinBulletSpeed, kMaxBulletSpeed, "%.2f");
    if (ImGui::Button("カメラシェイクテスト##TestShake", ImVec2(180, 26))) {
        ShakeCamera(kShootShakeIntensity * 1.5f, 0.25f);
    }
#endif
}
