#include "Scripts/Sample/SampleFollowerScript.h"
#include "Engine/Component/ScriptRegistry.h"
#include "Engine/Component/GameObject.h"
#include <cmath>

#ifdef _USEIMGUI
#include "externals/imgui/imgui.h"
#endif

// スクリプト自動登録マクロ（エディタに認識させる）
REGISTER_SCRIPT(SampleFollowerScript)

namespace {
    constexpr float kFixedDeltaTime = 1.0f / 60.0f;
    constexpr float kMinFollowSpeed = 0.5f;
    constexpr float kMaxFollowSpeed = 20.0f;
    constexpr float kMinStoppingDist = 0.5f;
    constexpr float kMaxStoppingDist = 10.0f;
    constexpr float kEpsilonDist = 0.0001f;
    constexpr float kRadToDeg = 57.2957795f;
}

SampleFollowerScript::SampleFollowerScript()
    : ScriptComponent("SampleFollowerScript") {
}

void SampleFollowerScript::Awake() {
}

void SampleFollowerScript::Start() {
}

void SampleFollowerScript::OnUpdate() {
    if (!owner_ || !targetObject_) return;

    Vector3 selfPos = owner_->GetPosition();
    Vector3 targetPos = targetObject_->GetPosition();

    // ターゲットまでの相対ベクトルを計算
    Vector3 diff = {
        targetPos.x - selfPos.x,
        targetPos.y - selfPos.y,
        targetPos.z - selfPos.z
    };

    float distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
    float dist = std::sqrt(distSq);

    // ターゲットを注視する回転（Y軸旋回）
    if (distSq > kEpsilonDist) {
        float yaw = std::atan2(diff.x, diff.z) * kRadToDeg;
        Vector3 rot = owner_->GetRotate();
        rot.y = yaw;
        owner_->SetRotate(rot);
    }

    // 停止距離より遠い場合のみ接近
    if (dist > stoppingDistance_) {
        float moveDist = followSpeed_ * kFixedDeltaTime;
        if (moveDist > dist - stoppingDistance_) {
            moveDist = dist - stoppingDistance_;
        }

        float invDist = 1.0f / dist;
        Vector3 dir = { diff.x * invDist, diff.y * invDist, diff.z * invDist };

        selfPos.x += dir.x * moveDist;
        selfPos.y += dir.y * moveDist;
        selfPos.z += dir.z * moveDist;

        owner_->SetPosition(selfPos);
    }
}

void SampleFollowerScript::OnInspectorGUI() {
#ifdef _USEIMGUI
    ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "【参考例】追尾フォロワースクリプト");
    ImGui::TextDisabled("バインドされたターゲットへ自動的に接近・追従します。");
    ImGui::Separator();

    // ターゲットオブジェクト接続用スロット（D&D受領 & クリア機能）
    DrawGameObjectSlot("追尾ターゲット", targetUUID_, targetObject_);

    ImGui::Spacing();
    ImGui::DragFloat("追従速度##FollowSpeed", &followSpeed_, 0.1f, kMinFollowSpeed, kMaxFollowSpeed, "%.1f");
    ImGui::DragFloat("目標停止距離##StopDist", &stoppingDistance_, 0.1f, kMinStoppingDist, kMaxStoppingDist, "%.1f");
    ImGui::Checkbox("スムーズ補間##Smooth", &enableSmoothing_);
#endif
}
