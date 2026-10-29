#include "Engine/Component/Components/GPUParticleComponent.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Graphics/Objects/Effect/GPU/GPUParticleObject.h"
#include "Engine/Graphics/Objects/Effect/GPU/GPUParticleSetting.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Zuizui.h"
#include "Engine/Base/DxCommon/DxCommon.h"
#include <algorithm>

#ifdef _USEIMGUI
#include <imgui.h>
#endif

namespace {
    // インスペクター編集用定数 (マジックナンバー排除)
    constexpr size_t kNameBufferSize = 128;
    constexpr float kDragSpeed = 0.05f;
    constexpr float kSpawnRateMin = 0.0f;
    constexpr float kSpawnRateMax = 500.0f;
    constexpr uint32_t kDefaultBurstCount = 30;
}

GPUParticleComponent::GPUParticleComponent() = default;

GPUParticleComponent::~GPUParticleComponent() {
    ReleaseParticleObject();
}

void GPUParticleComponent::Initialize() {
    AcquireParticleObject();
    ApplyPreset(presetName_);
}

void GPUParticleComponent::AcquireParticleObject() {
    if (particleObject_) return;

    if (!sParticlePool_.empty()) {
        particleObject_ = std::move(sParticlePool_.back());
        sParticlePool_.pop_back();
        if (particleObject_) {
            particleObject_->Reset();
        }
    } else {
        particleObject_ = std::make_unique<GPUParticleObject>();
        particleObject_->Initialize(kDefaultComponentMaxParticles);
    }
}

void GPUParticleComponent::ReleaseParticleObject() {
    if (particleObject_) {
        // GPUパイプラインストール (FlushGPU) を防ぐため、破棄せずプールへ返却して再利用
        particleObject_->Reset();
        sParticlePool_.push_back(std::move(particleObject_));
    }
}

void GPUParticleComponent::ClearPool() {
    sParticlePool_.clear();
}

void GPUParticleComponent::ApplyPreset(const std::string& presetName) {
    presetName_ = presetName;
    if (!particleObject_) return;

    GPUParticleSetting setting;
    std::string jsonPath = GPUParticlePresets::kParticleDirectory + "/" + presetName + GPUParticlePresets::kFileExtension;
    if (!GPUParticlePresets::LoadFromJson(jsonPath, setting)) {
        setting = GPUParticlePresets::GetPresetByName(presetName);
    }

    setting.isAutoSpawning = isAutoSpawning_;
    setting.spawnRatePerSec = isAutoSpawning_ ? spawnRatePerSec_ : 0.0f;
    textureKey_ = setting.textureKey;

    size_t bmIdx = static_cast<size_t>(setting.blendMode);
    if (bmIdx < static_cast<size_t>(ParticleBlendMode::Count)) {
        blendMode_ = static_cast<ParticleBlendMode>(bmIdx);
    }

    particleObject_->ApplySetting(setting);
}

void GPUParticleComponent::SetPresetName(const std::string& name) {
    ApplyPreset(name);
}

void GPUParticleComponent::SetBlendMode(ParticleBlendMode mode) {
    blendMode_ = mode;
    if (particleObject_) {
        particleObject_->SetBlendMode(mode);
    }
}

void GPUParticleComponent::SetAutoSpawning(bool active) {
    isAutoSpawning_ = active;
    if (particleObject_) {
        auto& s = particleObject_->GetSetting();
        s.isAutoSpawning = active;
        s.spawnRatePerSec = active ? spawnRatePerSec_ : 0.0f;
    }
}

void GPUParticleComponent::SetSpawnRate(float rate) {
    spawnRatePerSec_ = rate;
    if (particleObject_ && isAutoSpawning_) {
        particleObject_->GetSetting().spawnRatePerSec = rate;
    }
}

void GPUParticleComponent::EmitBurst(uint32_t count) {
    if (particleObject_) {
        particleObject_->EmitBurst(count);
    }
}

void GPUParticleComponent::Update() {
    if (!isActive_ || !particleObject_) return;

    Vector3 worldPos = offset_;
    if (owner_) {
        const Vector3& pos = owner_->GetPosition();
        worldPos.x += pos.x;
        worldPos.y += pos.y;
        worldPos.z += pos.z;
    }

    particleObject_->SetPosition(worldPos);
    particleObject_->Update(kDefaultDeltaTime);
}

void GPUParticleComponent::Draw() {
    if (!isActive_ || !particleObject_) return;

    if (auto engine = EngineResource::GetEngine()) {
        if (auto dxCommon = engine->GetDxCommon()) {
            auto* cmdList = dxCommon->GetCommandList();
            particleObject_->DispatchCompute(cmdList);
            particleObject_->Draw(cmdList, textureKey_, blendMode_);
        }
    }
}

void GPUParticleComponent::DrawInspector() {
#ifdef _USEIMGUI
    constexpr ImVec4 kHeaderColor = { 0.4f, 0.8f, 1.0f, 1.0f };
    ImGui::TextColored(kHeaderColor, "【GPUパーティクルコンポーネント】");

    // 1. プリセット選択
    if (ImGui::BeginCombo("Preset##GPUParticleComp", presetName_.c_str())) {
        auto presets = GPUParticlePresets::GetPresetNames();
        for (const auto& preset : presets) {
            bool isSelected = (presetName_ == preset);
            if (ImGui::Selectable(preset.c_str(), isSelected)) {
                ApplyPreset(preset);
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    // 2. ブレンドモード選択
    const char* blendNames[] = { "Normal (半透明)", "Add (加算発光)", "Subtract (減算)", "Multiply (乗算)" };
    int currentBlend = static_cast<int>(blendMode_);
    if (ImGui::Combo("Blend Mode##GPUParticleComp", &currentBlend, blendNames, IM_ARRAYSIZE(blendNames))) {
        SetBlendMode(static_cast<ParticleBlendMode>(currentBlend));
    }

    // 3. テクスチャキー
    char texBuf[kNameBufferSize]{};
    strncpy_s(texBuf, textureKey_.c_str(), sizeof(texBuf) - 1);
    if (ImGui::InputText("Texture Key##GPUParticleComp", texBuf, sizeof(texBuf))) {
        textureKey_ = texBuf;
    }

    // 3. オフセット
    float offset[3] = { offset_.x, offset_.y, offset_.z };
    if (ImGui::DragFloat3("Offset##GPUParticleComp", offset, kDragSpeed, 0.0f, 0.0f, "%.2f")) {
        offset_ = { offset[0], offset[1], offset[2] };
    }

    // 4. 自動発生フラグ & レート
    bool autoSpawn = isAutoSpawning_;
    if (ImGui::Checkbox("Auto Spawn##GPUParticleComp", &autoSpawn)) {
        SetAutoSpawning(autoSpawn);
    }

    if (isAutoSpawning_) {
        float rate = spawnRatePerSec_;
        if (ImGui::DragFloat("Spawn Rate##GPUParticleComp", &rate, 1.0f, kSpawnRateMin, kSpawnRateMax, "%.1f/s")) {
            SetSpawnRate(rate);
        }
    }

    // 5. 即時バーストテスト
    if (ImGui::Button("Emit Burst (30)##GPUParticleComp")) {
        EmitBurst(kDefaultBurstCount);
    }

    // 6. 内部詳細設定
    if (particleObject_) {
        if (ImGui::TreeNode("詳細シミュレーションパラメータ##GPUParticleDetails")) {
            particleObject_->ImGuiControl("Setting");
            ImGui::TreePop();
        }
    }
#endif
}
