#include "Scripts/Director/ClearDirectorScript.h"
#include "Engine/Component/ScriptRegistry.h"
#include "App/Scene/Core/SceneManager.h"
#include "Engine/Graphics/PostProcess/PostProcess.h"
#include "Engine/Graphics/Objects/Effect/Manager/EffectManager.h"
#include "Engine/Graphics/Objects/Effect/Manager/EffectFactory.h"
#include "Engine/Input/Input.h"
#include "Engine/Base/BaseResource.h"
#include <cstdlib>
#ifdef _USEIMGUI
#include <imgui.h>
#endif

namespace {
    const std::string kFireworkEffectName = "FireworksSet";
    constexpr float kRandCenterOffset = 0.5f;
    constexpr size_t kImGuiBufferLength = 128;
    constexpr uint32_t kGpuFireworkParticleCount = 3000; // 1発あたりのGPU光粒子数
    const Vector3 kManualFireworkPos = { 0.0f, 15.0f, 20.0f }; // 手動発射位置 (上空)
}

ClearDirectorScript::ClearDirectorScript()
    : ScriptComponent("ClearDirectorScript") {
}

ClearDirectorScript::~ClearDirectorScript() = default;

/// <summary>
/// ゲームクリアシーン開始時にラジアルブラーのポストプロセス演出を有効化し、
/// 祝砲花火エフェクトファクトリを初期化します。
/// </summary>
void ClearDirectorScript::Start() {
    // ポストプロセスの演出設定
    if (auto* postProcess = SceneManager::GetInstance()->GetPostProcess()) {
        postProcess->SetRadialBlurActive(true);
    }

    // エフェクトマネージャの初期化
    auto effectMgr = EffectManager::GetInstance();
    effectMgr->Initialize();
    EffectFactory::GetInstance()->RegisterAllEffects();
    fireworkTimer_ = 0;
}

/// <summary>
/// スペースキー入力によるタイトルシーンへの遷移監視、および祝砲花火エフェクトのランダム打ち上げタイマーを更新します。
/// </summary>
void ClearDirectorScript::OnUpdate() {
    // スペースキー押下でタイトルシーンへ遷移
    auto input = InputResource::GetInput();
    if (input && input->Trigger(DIK_SPACE)) {
        SceneManager::GetInstance()->ChangeScene(nextSceneName_);
        return;
    }

    // 祝砲花火エフェクトのランダム打ち上げ (GPUパーティクルによる超大量祝砲花火)
    if (--fireworkTimer_ <= 0) {
        fireworkTimer_ = minInterval_ + rand() % maxIntervalRange_;
        float rx = (static_cast<float>(rand()) / RAND_MAX - kRandCenterOffset) * rangeX_;
        float ry = baseY_ + (static_cast<float>(rand()) / RAND_MAX) * rangeY_;
        float rz = baseZ_ + (static_cast<float>(rand()) / RAND_MAX) * rangeZ_;
        Vector3 fireworkPos = { rx, ry, rz };
        EffectManager::GetInstance()->PlayGPUEffect(fireworkPos, kGpuFireworkParticleCount);
    }
}

/// <summary>
/// 祝砲花火エフェクトの3D描画を実行します（Scene::Draw()にて一括描画されるため空実装）。
/// </summary>
void ClearDirectorScript::Draw() {
}

void ClearDirectorScript::OnInspectorGUI() {
#ifdef _USEIMGUI
    char buffer[kImGuiBufferLength];
    strncpy_s(buffer, nextSceneName_.c_str(), sizeof(buffer) - 1);
    if (ImGui::InputText("Next Scene", buffer, sizeof(buffer))) {
        nextSceneName_ = buffer;
    }
    ImGui::DragInt("Min Interval", &minInterval_, 1.0f, 1, 300);
    ImGui::DragInt("Interval Range", &maxIntervalRange_, 1.0f, 1, 300);
    ImGui::DragFloat("Range X (横幅)", &rangeX_, 1.0f, 0.0f, 200.0f);
    ImGui::DragFloat("Base Y (高さ基準)", &baseY_, 0.5f, -20.0f, 50.0f);
    ImGui::DragFloat("Range Y (高さ幅)", &rangeY_, 0.5f, 0.0f, 50.0f);
    ImGui::DragFloat("Base Z (奥行き基準)", &baseZ_, 0.5f, 0.0f, 100.0f);
    ImGui::DragFloat("Range Z (奥行き幅)", &rangeZ_, 0.5f, 0.0f, 100.0f);

    ImGui::Separator();
    if (ImGui::Button("🎆 GPU祝砲花火を手動発射 (3,000粒子)")) {
        EffectManager::GetInstance()->PlayGPUEffect(kManualFireworkPos, kGpuFireworkParticleCount);
    }
    ImGui::Checkbox("「エフェクト一覧」ウィンドウを表示", EffectManager::GetShowListWindowPtr());
#endif
}

REGISTER_SCRIPT(ClearDirectorScript);
