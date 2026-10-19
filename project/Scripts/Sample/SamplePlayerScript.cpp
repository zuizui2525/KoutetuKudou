#include "Scripts/Sample/SamplePlayerScript.h"
#include "Engine/Component/ScriptRegistry.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Input/Input.h"
#include "Engine/Base/BaseResource.h"
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
    }
}

void SamplePlayerScript::OnInspectorGUI() {
#ifdef _USEIMGUI
    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.5f, 1.0f), "【参考例】プレイヤー移動スクリプト");
    ImGui::TextDisabled("WASD / 矢印キーでXZ平面を移動します。");
    ImGui::Separator();

    ImGui::DragFloat("移動速度##MoveSpeed", &moveSpeed_, 0.2f, kMinMoveSpeed, kMaxMoveSpeed, "%.1f");
    ImGui::Checkbox("自動旋回テスト##AutoOrbit", &enableAutoOrbit_);

    if (enableAutoOrbit_) {
        ImGui::DragFloat("旋回半径##OrbitRadius", &orbitRadius_, 0.2f, kMinOrbitRadius, kMaxOrbitRadius, "%.1f");
        ImGui::DragFloat("旋回速度##OrbitSpeed", &orbitSpeed_, 0.1f, kMinOrbitSpeed, kMaxOrbitSpeed, "%.1f");
    }
#endif
}
