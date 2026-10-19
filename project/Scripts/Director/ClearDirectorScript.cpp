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
}

ClearDirectorScript::ClearDirectorScript()
    : ScriptComponent("ClearDirectorScript") {
}

ClearDirectorScript::~ClearDirectorScript() = default;

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
        float rx = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * rangeX_;
        float rz = (static_cast<float>(rand()) / RAND_MAX + 2.0f) * rangeZ_;
        float ry = (static_cast<float>(rand()) / RAND_MAX + 3.0f) * minY_;
        param.position = { rx, ry, rz };
        param.scale = { 1.0f, 1.0f, 1.0f };
        param.isLoop = false;
        EffectManager::GetInstance()->PlayEffect3D(kFireworkEffectName, param);
    }
    EffectManager::GetInstance()->Update();
}

void ClearDirectorScript::Draw() {
    // 花火エフェクトの 3D 描画
    EffectManager::GetInstance()->Draw();
}

void ClearDirectorScript::OnInspectorGUI() {
#ifdef _USEIMGUI
    char buffer[128];
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
