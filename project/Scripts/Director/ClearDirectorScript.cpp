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
    constexpr float kFireworkZBaseOffset = 2.0f;
    constexpr float kFireworkYBaseOffset = 3.0f;
    constexpr size_t kImGuiBufferLength = 128;
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

    // 祝砲花火エフェクトのランダム打ち上げ
    if (--fireworkTimer_ <= 0) {
        fireworkTimer_ = minInterval_ + rand() % maxIntervalRange_;
        EffectPlayParam param;
        float rx = (static_cast<float>(rand()) / RAND_MAX - kRandCenterOffset) * rangeX_;
        float rz = (static_cast<float>(rand()) / RAND_MAX + kFireworkZBaseOffset) * rangeZ_;
        float ry = (static_cast<float>(rand()) / RAND_MAX + kFireworkYBaseOffset) * minY_;
        param.position = { rx, ry, rz };
        param.scale = { 1.0f, 1.0f, 1.0f };
        param.isLoop = false;
        EffectManager::GetInstance()->PlayEffect3D(kFireworkEffectName, param);
    }
    EffectManager::GetInstance()->Update();
}

/// <summary>
/// 祝砲花火エフェクトの3D描画を実行します。
/// </summary>
void ClearDirectorScript::Draw() {
    // 花火エフェクトの 3D 描画
    EffectManager::GetInstance()->Draw();
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
    ImGui::DragFloat("Range X", &rangeX_, 1.0f, 0.0f, 200.0f);
    ImGui::DragFloat("Range Z", &rangeZ_, 1.0f, 0.0f, 200.0f);
    ImGui::DragFloat("Min Y", &minY_, 0.5f, -50.0f, 50.0f);
#endif
}

REGISTER_SCRIPT(ClearDirectorScript);
