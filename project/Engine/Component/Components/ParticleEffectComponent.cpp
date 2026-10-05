#include "Engine/Component/Components/ParticleEffectComponent.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Graphics/Objects/Effect/Manager/EffectManager.h"

#ifdef _USEIMGUI
#include <imgui.h>
#endif

namespace {
    // インスペクター編集用定数 (マジックナンバー排除)
    constexpr size_t kNameBufferSize = 128;
    constexpr float kDragSpeed = 0.05f;
}

void ParticleEffectComponent::Update() {
    if (!isActive_) return;

    if (autoPlay_ && !hasAutoPlayed_) {
        Play();
        hasAutoPlayed_ = true;
    }
}

void ParticleEffectComponent::Play() {
    if (effectName_.empty()) return;

    EffectPlayParam param{};
    Vector3 worldPos = offset_;

    if (owner_) {
        worldPos.x += owner_->GetPosition().x;
        worldPos.y += owner_->GetPosition().y;
        worldPos.z += owner_->GetPosition().z;
        param.rotation = owner_->GetRotate();
        param.scale = owner_->GetScale();
    }

    param.position = worldPos;
    param.isLoop = isLoop_;

    if (auto* mgr = EffectManager::GetInstance()) {
        mgr->PlayEffect3D(effectName_, param);
    }
}

void ParticleEffectComponent::Stop() {
    if (effectName_.empty()) return;

    if (auto* mgr = EffectManager::GetInstance()) {
        mgr->StopEffect(effectName_);
    }
}

void ParticleEffectComponent::DrawInspector() {
#ifdef _USEIMGUI
    // 1. エフェクト名
    char nameBuf[kNameBufferSize]{};
    strncpy_s(nameBuf, effectName_.c_str(), sizeof(nameBuf) - 1);
    if (ImGui::InputText("Effect Name##ParticleEffect", nameBuf, sizeof(nameBuf))) {
        effectName_ = nameBuf;
    }

    // 2. オフセット座標
    float offset[3] = { offset_.x, offset_.y, offset_.z };
    if (ImGui::DragFloat3("Offset##ParticleEffect", offset, kDragSpeed, 0.0f, 0.0f, "%.2f")) {
        offset_ = { offset[0], offset[1], offset[2] };
    }

    // 3. ループフラグ
    bool loop = isLoop_;
    if (ImGui::Checkbox("Loop##ParticleEffect", &loop)) {
        isLoop_ = loop;
    }

    // 4. 自動再生フラグ
    bool autoPlay = autoPlay_;
    if (ImGui::Checkbox("Auto Play##ParticleEffect", &autoPlay)) {
        autoPlay_ = autoPlay;
    }

    // 5. テスト再生 / 停止 ボタン
    if (ImGui::Button("Play Effect##ParticleEffect")) {
        Play();
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop Effect##ParticleEffect")) {
        Stop();
    }
#endif
}
