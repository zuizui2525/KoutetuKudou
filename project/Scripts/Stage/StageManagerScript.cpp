#include "Scripts/Stage/StageManagerScript.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/Components/MeshRendererComponent.h"
#include "Engine/Component/ScriptRegistry.h"
#include "App/Scene/Core/SceneManager.h"
#include "App/Scene/Generic/GenericScene.h"
#ifdef _USEIMGUI
#include <imgui.h>
#endif

namespace {
    const std::string kPillarModelKey = "cube";
    const std::string kPillarTextureKey = "white";
    constexpr Vector4 kPillarColor = { 0.25f, 0.28f, 0.35f, 1.0f };       // メタリックグレー
    constexpr Vector4 kGatePillarColor = { 0.7f, 0.6f, 0.2f, 1.0f };      // エリア境界ゲートのゴールド
    constexpr float kHalfFactor = 0.5f;
}

REGISTER_SCRIPT(StageManagerScript);

StageManagerScript::StageManagerScript()
    : ScriptComponent("StageManagerScript") {
}

StageManagerScript::~StageManagerScript() = default;

void StageManagerScript::Awake() {
}

void StageManagerScript::Start() {
    BuildStageGeometry();
}

void StageManagerScript::OnUpdate() {
}

/// <summary>
/// 全4エリア（全長480m）の左右境界（X=±50m）に沿って、一定間隔（kPillarSpacingZ = 30m）ごとに柱（Cubeモデル）を配置します。
/// 各エリアの境界（120m, 240m, 360m, 480m）に位置する柱はゴールド色（kGatePillarColor）として強調表示します。
/// </summary>
void StageManagerScript::BuildStageGeometry() {
    auto* scene = dynamic_cast<GenericScene*>(SceneManager::GetInstance()->GetCurrentScene());
    if (!scene) return;

    float totalLength = static_cast<float>(kAreaCount) * kAreaLength;
    int pillarSteps = static_cast<int>(totalLength / kPillarSpacingZ);

    // 左右の境界柱列を生成
    for (int i = 0; i <= pillarSteps; ++i) {
        float z = static_cast<float>(i) * kPillarSpacingZ;
        bool isAreaGate = (static_cast<int>(z) % static_cast<int>(kAreaLength) == 0);

        // 左側の柱
        {
            auto leftPillar = std::make_unique<GameObject>("LeftPillar_" + std::to_string(i));
            leftPillar->SetHideInHierarchy(true);
            leftPillar->SetPosition({ -kBoundaryX, kPillarHeight * kHalfFactor, z });
            leftPillar->SetScale({ kPillarRadius, kPillarHeight, kPillarRadius });
            auto* mr = leftPillar->AddComponent<MeshRendererComponent>();
            mr->SetModelKey(kPillarModelKey);
            mr->SetTextureKey(kPillarTextureKey);
            mr->SetColor(isAreaGate ? kGatePillarColor : kPillarColor);
            stageProps_.push_back(leftPillar.get());
            scene->AddGameObject(std::move(leftPillar));
        }

        // 右側の柱
        {
            auto rightPillar = std::make_unique<GameObject>("RightPillar_" + std::to_string(i));
            rightPillar->SetHideInHierarchy(true);
            rightPillar->SetPosition({ kBoundaryX, kPillarHeight * kHalfFactor, z });
            rightPillar->SetScale({ kPillarRadius, kPillarHeight, kPillarRadius });
            auto* mr = rightPillar->AddComponent<MeshRendererComponent>();
            mr->SetModelKey(kPillarModelKey);
            mr->SetTextureKey(kPillarTextureKey);
            mr->SetColor(isAreaGate ? kGatePillarColor : kPillarColor);
            stageProps_.push_back(rightPillar.get());
            scene->AddGameObject(std::move(rightPillar));
        }
    }
}

void StageManagerScript::OnInspectorGUI() {
#ifdef _USEIMGUI
    constexpr ImVec4 kHeaderColor = { 0.6f, 0.8f, 1.0f, 1.0f };
    ImGui::TextColored(kHeaderColor, "【ステージマップ管理】");
    ImGui::Separator();

    ImGui::Text("総エリア数: %d エリア", kAreaCount);
    ImGui::Text("マップ全長: %.0f m (Z=0 ~ %.0f)", static_cast<float>(kAreaCount) * kAreaLength, static_cast<float>(kAreaCount) * kAreaLength);
    ImGui::Text("左右境界幅: ±%.0f m", kBoundaryX);
    ImGui::Text("生成済みステージオブジェクト: %zu 個", stageProps_.size());
#endif
}
