#include "Scripts/Route/RouteRendererScript.h"
#include "Scripts/Route/RouteManagerScript.h"
#include "Engine/Graphics/Objects/3d/Line/LineObject.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/ScriptRegistry.h"
#include "App/Scene/Core/SceneManager.h"
#include "App/Scene/Core/Scene.h"
#include "App/Scene/Core/SceneDirector.h"
#include <cmath>
#include <numbers>
#ifdef _USEIMGUI
#include <imgui.h>
#endif

namespace {
    const std::string kDefaultTextureKey = "white";
    constexpr float kPi = 3.14159265358979323846f;
}

REGISTER_SCRIPT(RouteRendererScript);

RouteRendererScript::RouteRendererScript()
    : ScriptComponent("RouteRendererScript") {
}

RouteRendererScript::~RouteRendererScript() = default;

void RouteRendererScript::Awake() {
    linePool_.reserve(kInitialLinePoolSize);
}

void RouteRendererScript::Start() {
    if (owner_) {
        routeMgr_ = owner_->GetComponent<RouteManagerScript>();
    }
    if (!routeMgr_) {
        auto* scene = dynamic_cast<Scene*>(SceneManager::GetInstance()->GetCurrentScene());
        if (scene && scene->GetDirector()) {
            routeMgr_ = scene->GetDirector()->GetScript<RouteManagerScript>();
        }
        if (!routeMgr_ && scene) {
            for (const auto& obj : scene->GetGameObjects()) {
                if (obj && obj->GetComponent<RouteManagerScript>()) {
                    routeMgr_ = obj->GetComponent<RouteManagerScript>();
                    break;
                }
            }
        }
    }
}

void RouteRendererScript::OnUpdate() {
    if (!routeMgr_) {
        if (owner_) {
            routeMgr_ = owner_->GetComponent<RouteManagerScript>();
        }
        if (!routeMgr_) {
            auto* scene = dynamic_cast<Scene*>(SceneManager::GetInstance()->GetCurrentScene());
            if (scene && scene->GetDirector()) {
                routeMgr_ = scene->GetDirector()->GetScript<RouteManagerScript>();
            }
        }
    }
}

void RouteRendererScript::Draw() {
    if (!routeMgr_) return;

    size_t lineIdx = 0;

    float startZ = routeMgr_->GetCurrentAreaStartZ();
    float goalZ = routeMgr_->GetCurrentAreaGoalZ();
    float radius = routeMgr_->GetAreaRadius();

    // 1. スタート地点のサークル描画（グリーン）
    Vector3 startCenter = { 0.0f, kGroundYOffset, startZ };
    DrawCircle(startCenter, radius, startCircleColor_, circleThickness_, lineIdx);

    // 2. ゴール地点のサークル描画（レッド）
    Vector3 goalCenter = { 0.0f, kGroundYOffset, goalZ };
    DrawCircle(goalCenter, radius, goalCircleColor_, circleThickness_, lineIdx);

    // 3. ルート軌跡の描画
    if (routeMgr_->HasReachedGoal()) {
        // 完成後の滑らかなCatmull-Rom曲線（描画用に適切な間隔へダウンサンプリング）
        const auto& pathPoints = routeMgr_->GetPathPoints();
        std::vector<Vector3> renderPoints = DownsamplePoints(pathPoints, kRenderMinSpacing);
        DrawPolyline(renderPoints, completedPathColor_, completedPathThickness_, lineIdx);
    } else if (routeMgr_->IsDrawing() || !routeMgr_->GetRawPoints().empty()) {
        // 手書き中のリアルタイム生点列
        const auto& rawPoints = routeMgr_->GetRawPoints();
        DrawPolyline(rawPoints, drawingPathColor_, drawingPathThickness_, lineIdx);
    }
}

void RouteRendererScript::DrawPolyline(
    const std::vector<Vector3>& points,
    const Vector4& color,
    float thickness,
    size_t& linePoolIdx
) {
    if (points.size() < 2) return;

    for (size_t i = 0; i < points.size() - 1; ++i) {
        Vector3 p1 = points[i];
        Vector3 p2 = points[i + 1];
        p1.y += kGroundYOffset;
        p2.y += kGroundYOffset;

        LineObject* line = GetOrCreateLine(linePoolIdx++);
        line->SetStartPoint(p1);
        line->SetEndPoint(p2);
        line->SetThickness(thickness);
        line->SetColor(color);
        line->Update();
        line->Draw(kDefaultTextureKey);
    }
}

void RouteRendererScript::DrawCircle(
    const Vector3& center,
    float radius,
    const Vector4& color,
    float thickness,
    size_t& linePoolIdx
) {
    float angleStep = (2.0f * kPi) / static_cast<float>(kCircleSegments);

    for (int i = 0; i < kCircleSegments; ++i) {
        float a1 = static_cast<float>(i) * angleStep;
        float a2 = static_cast<float>(i + 1) * angleStep;

        Vector3 p1 = {
            center.x + radius * std::cos(a1),
            center.y,
            center.z + radius * std::sin(a1)
        };
        Vector3 p2 = {
            center.x + radius * std::cos(a2),
            center.y,
            center.z + radius * std::sin(a2)
        };

        LineObject* line = GetOrCreateLine(linePoolIdx++);
        line->SetStartPoint(p1);
        line->SetEndPoint(p2);
        line->SetThickness(thickness);
        line->SetColor(color);
        line->Update();
        line->Draw(kDefaultTextureKey);
    }
}

LineObject* RouteRendererScript::GetOrCreateLine(size_t index) {
    if (index >= linePool_.size()) {
        auto newLine = std::make_unique<LineObject>();
        newLine->SetAutoRegisterHierarchy(false); // エディタヒエラルキーへの不要な自動登録を抑止
        newLine->Initialize(0);
        linePool_.push_back(std::move(newLine));
    }
    return linePool_[index].get();
}

std::vector<Vector3> RouteRendererScript::DownsamplePoints(
    const std::vector<Vector3>& points,
    float minSpacing
) const {
    if (points.size() <= 2) return points;

    std::vector<Vector3> sampled;
    sampled.reserve(points.size());
    sampled.push_back(points.front());

    float minSpacingSq = minSpacing * minSpacing;
    for (size_t i = 1; i < points.size() - 1; ++i) {
        float dx = points[i].x - sampled.back().x;
        float dz = points[i].z - sampled.back().z;
        if (dx * dx + dz * dz >= minSpacingSq) {
            sampled.push_back(points[i]);
        }
    }
    sampled.push_back(points.back()); // ゴール終点は必ず保持
    return sampled;
}

void RouteRendererScript::OnInspectorGUI() {
#ifdef _USEIMGUI
    constexpr ImVec4 kHeaderColor = { 0.3f, 0.9f, 1.0f, 1.0f };
    ImGui::TextColored(kHeaderColor, "【ルート可視化レンダラー設定】");
    ImGui::Separator();

    ImGui::DragFloat("完成ルート線幅", &completedPathThickness_, 0.01f, 0.05f, 2.0f);
    ImGui::DragFloat("描画中ルート線幅", &drawingPathThickness_, 0.01f, 0.05f, 2.0f);
    ImGui::DragFloat("サークル枠線幅", &circleThickness_, 0.01f, 0.05f, 2.0f);

    ImGui::ColorEdit4("完成ルート色", &completedPathColor_.x);
    ImGui::ColorEdit4("描画中ルート色", &drawingPathColor_.x);
    ImGui::ColorEdit4("スタート円色", &startCircleColor_.x);
    ImGui::ColorEdit4("ゴール円色", &goalCircleColor_.x);

    ImGui::Text("使用中ラインプール: %zu 本", linePool_.size());
#endif
}
