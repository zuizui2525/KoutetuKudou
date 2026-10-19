#include "Scripts/Route/RouteManagerScript.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/ScriptRegistry.h"
#include "Engine/Math/Matrix/Matrix.h"
#include <cmath>
#include <algorithm>
#ifdef _USEIMGUI
#include <imgui.h>
#endif

REGISTER_SCRIPT(RouteManagerScript)

RouteManagerScript::RouteManagerScript()
    : ScriptComponent("RouteManagerScript") {
}

void RouteManagerScript::Awake() {
    SetupArea(0);
}

void RouteManagerScript::Start() {
}

void RouteManagerScript::OnUpdate() {
}

void RouteManagerScript::SetupArea(int areaIndex) {
    currentAreaIndex_ = areaIndex;
    currentAreaStartZ_ = static_cast<float>(areaIndex) * kAreaLength;
    currentAreaGoalZ_ = currentAreaStartZ_ + kAreaLength;
    ResetRoute();
}

void RouteManagerScript::ResetRoute() {
    rawPoints_.clear();
    pathPoints_.clear();
    accumDistances_.clear();
    totalDistance_ = 0.0f;
    isDrawing_ = false;
    hasReachedGoal_ = false;

    // 初期点としてスタート地点を設定
    Vector3 startPos = { 0.0f, 0.0f, currentAreaStartZ_ };
    rawPoints_.push_back(startPos);
}

void RouteManagerScript::UpdateDrawing(const Vector3& worldIntersectPos, bool isTrigger, bool isPress) {
    if (!isPress) {
        isDrawing_ = false;
        return;
    }

    if (std::abs(worldIntersectPos.x) > kMapBoundaryX ||
        worldIntersectPos.z < currentAreaStartZ_ - 5.0f ||
        worldIntersectPos.z > currentAreaGoalZ_ + 20.0f) {
        return;
    }

    // ドラッグ開始時にスタート位置近くならリセット
    if (isTrigger) {
        float toStartX = worldIntersectPos.x;
        float toStartZ = worldIntersectPos.z - currentAreaStartZ_;
        if (toStartX * toStartX + toStartZ * toStartZ <= kAreaRadius * kAreaRadius) {
            ResetRoute();
        }
    }

    if (rawPoints_.empty()) {
        ResetRoute();
    }

    if (!hasReachedGoal_) {
        isDrawing_ = true;
        const Vector3& lastPoint = rawPoints_.back();
        float dx = worldIntersectPos.x - lastPoint.x;
        float dz = worldIntersectPos.z - lastPoint.z;
        float dist = std::sqrt(dx * dx + dz * dz);

        if (dist >= kMinPointDistance) {
            rawPoints_.push_back(worldIntersectPos);

            // ゴール到達判定
            float toGoalX = worldIntersectPos.x;
            float toGoalZ = worldIntersectPos.z - currentAreaGoalZ_;
            if (toGoalX * toGoalX + toGoalZ * toGoalZ <= kAreaRadius * kAreaRadius) {
                // ゴール中心座標へ自動スナップして終点に追加
                Vector3 goalCenterPos = { 0.0f, 0.0f, currentAreaGoalZ_ };
                rawPoints_.push_back(goalCenterPos);

                hasReachedGoal_ = true;
                isDrawing_ = false;
                FinalizeRoute();
            }
        }
    }
}

void RouteManagerScript::FinalizeRoute() {
    if (rawPoints_.size() < 2) return;

    // 移動平均フィルタで平滑化
    std::vector<Vector3> smoothedPoints = rawPoints_;
    if (smoothedPoints.size() >= 3) {
        for (int iter = 0; iter < 5; ++iter) {
            std::vector<Vector3> temp = smoothedPoints;
            for (size_t i = 1; i < smoothedPoints.size() - 1; ++i) {
                temp[i].x = (smoothedPoints[i - 1].x + smoothedPoints[i].x + smoothedPoints[i + 1].x) / 3.0f;
                temp[i].y = (smoothedPoints[i - 1].y + smoothedPoints[i].y + smoothedPoints[i + 1].y) / 3.0f;
                temp[i].z = (smoothedPoints[i - 1].z + smoothedPoints[i].z + smoothedPoints[i + 1].z) / 3.0f;
            }
            smoothedPoints = temp;
        }
    }

    pathPoints_ = Math::GenerateCatmullRomPath(smoothedPoints, kPathDivision);
    BuildEqualSpacingTable();
}

void RouteManagerScript::BuildEqualSpacingTable() {
    accumDistances_.clear();
    accumDistances_.push_back(0.0f);
    float accum = 0.0f;
    for (size_t i = 1; i < pathPoints_.size(); ++i) {
        float dx = pathPoints_[i].x - pathPoints_[i - 1].x;
        float dy = pathPoints_[i].y - pathPoints_[i - 1].y;
        float dz = pathPoints_[i].z - pathPoints_[i - 1].z;
        accum += std::sqrt(dx * dx + dy * dy + dz * dz);
        accumDistances_.push_back(accum);
    }
    totalDistance_ = accum;
}

Vector3 RouteManagerScript::GetPositionAtDistance(float distance) const {
    if (pathPoints_.empty()) return { 0.0f, 0.0f, currentAreaStartZ_ };
    if (distance <= 0.0f) return pathPoints_.front();
    if (distance >= totalDistance_) return pathPoints_.back();

    size_t idx = 0;
    for (size_t i = 0; i < accumDistances_.size() - 1; ++i) {
        if (accumDistances_[i] <= distance && distance < accumDistances_[i + 1]) {
            idx = i;
            break;
        }
    }

    float tLocal = 0.0f;
    float distDiff = accumDistances_[idx + 1] - accumDistances_[idx];
    if (distDiff > 0.0001f) {
        tLocal = (distance - accumDistances_[idx]) / distDiff;
    }

    return {
        pathPoints_[idx].x + tLocal * (pathPoints_[idx + 1].x - pathPoints_[idx].x),
        pathPoints_[idx].y + tLocal * (pathPoints_[idx + 1].y - pathPoints_[idx].y),
        pathPoints_[idx].z + tLocal * (pathPoints_[idx + 1].z - pathPoints_[idx].z)
    };
}

Vector3 RouteManagerScript::GetTangentAtDistance(float distance) const {
    if (pathPoints_.size() < 2) return { 0.0f, 0.0f, 1.0f };

    float targetDist = std::clamp(distance, 0.0f, totalDistance_);
    size_t idx = 0;
    for (size_t i = 0; i < accumDistances_.size() - 1; ++i) {
        if (accumDistances_[i] <= targetDist && targetDist < accumDistances_[i + 1]) {
            idx = i;
            break;
        }
    }
    if (idx >= pathPoints_.size() - 1) idx = pathPoints_.size() - 2;

    float dx = pathPoints_[idx + 1].x - pathPoints_[idx].x;
    float dy = pathPoints_[idx + 1].y - pathPoints_[idx].y;
    float dz = pathPoints_[idx + 1].z - pathPoints_[idx].z;
    float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len > 0.0001f) {
        return { dx / len, dy / len, dz / len };
    }
    return { 0.0f, 0.0f, 1.0f };
}

Vector3 RouteManagerScript::GetRotationAtDistance(float distance) const {
    Vector3 tangent = GetTangentAtDistance(distance);
    float yaw = std::atan2(tangent.x, tangent.z);
    float pitch = -std::atan2(tangent.y, std::sqrt(tangent.x * tangent.x + tangent.z * tangent.z));
    return { pitch, yaw, 0.0f };
}

void RouteManagerScript::OnInspectorGUI() {
#ifdef _USEIMGUI
    constexpr ImVec4 kHeaderColor = { 0.4f, 0.9f, 0.5f, 1.0f };
    ImGui::TextColored(kHeaderColor, "【ルート管理スクリプト設定】");
    ImGui::Separator();

    ImGui::Text("現在エリア: エリア %d", currentAreaIndex_ + 1);
    ImGui::Text("エリア区間 Z: %.1f ~ %.1f", currentAreaStartZ_, currentAreaGoalZ_);
    ImGui::Text("記録点数: %zu 点", rawPoints_.size());
    ImGui::Text("補間点数: %zu 点", pathPoints_.size());
    ImGui::Text("ルート総距離: %.2f m", totalDistance_);
    ImGui::Text("ゴール到達状態: %s", hasReachedGoal_ ? "ゴール済み (Complete)" : "未到達 (Incomplete)");
#endif
}
