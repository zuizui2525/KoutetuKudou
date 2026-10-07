#include "Engine/Component/Components/SinOscillatorComponent.h"
#include "Engine/Component/GameObject.h"
#include <cmath>

#ifdef _USEIMGUI
#include <imgui.h>
#endif

namespace {
    // デルタタイム定数（60FPS基準: マジックナンバー排除）
    constexpr float kDefaultDeltaTime = 1.0f / 60.0f;

    // インスペクター編集用定数
    constexpr float kDragSpeed = 0.05f;
    constexpr float kFreqDragSpeed = 0.02f;

    const char* const kTargetTypeNames[] = {
        "Position",
        "Rotation",
        "Scale"
    };
    constexpr int kTargetTypeCount = sizeof(kTargetTypeNames) / sizeof(kTargetTypeNames[0]);
}

void SinOscillatorComponent::Initialize() {
    CaptureBaseValue();
}

void SinOscillatorComponent::CaptureBaseValue() {
    if (!owner_) return;

    switch (targetType_) {
    case TargetType::Position:
        baseValue_ = owner_->GetPosition();
        break;
    case TargetType::Rotation:
        baseValue_ = owner_->GetRotate();
        break;
    case TargetType::Scale:
        baseValue_ = owner_->GetScale();
        break;
    }
    isBaseCaptured_ = true;
}

void SinOscillatorComponent::Update() {
    if (!owner_ || !isActive_) return;

    if (!isBaseCaptured_) {
        CaptureBaseValue();
    }

    timer_ += kDefaultDeltaTime;
    float sinFactor = std::sin(timer_ * frequency_ + phase_);

    Vector3 offset = {
        amplitude_.x * sinFactor,
        amplitude_.y * sinFactor,
        amplitude_.z * sinFactor
    };

    Vector3 targetVal = {
        baseValue_.x + offset.x,
        baseValue_.y + offset.y,
        baseValue_.z + offset.z
    };

    switch (targetType_) {
    case TargetType::Position:
        owner_->SetPosition(targetVal);
        break;
    case TargetType::Rotation:
        owner_->SetRotate(targetVal);
        break;
    case TargetType::Scale:
        owner_->SetScale(targetVal);
        break;
    }
}

void SinOscillatorComponent::DrawInspector() {
#ifdef _USEIMGUI
    std::string idPrefix = "##SinOsc_" + std::to_string(reinterpret_cast<uintptr_t>(this));

    // 対象プロパティの選択
    int currentType = static_cast<int>(targetType_);
    if (ImGui::Combo(("Target Property" + idPrefix).c_str(), &currentType, kTargetTypeNames, kTargetTypeCount)) {
        targetType_ = static_cast<TargetType>(currentType);
        CaptureBaseValue();
    }

    // 振幅
    float amp[3] = { amplitude_.x, amplitude_.y, amplitude_.z };
    if (ImGui::DragFloat3(("Amplitude" + idPrefix).c_str(), amp, kDragSpeed, 0.0f, 0.0f, "%.2f")) {
        amplitude_ = { amp[0], amp[1], amp[2] };
    }

    // 周波数
    float freq = frequency_;
    if (ImGui::DragFloat(("Frequency (rad/s)" + idPrefix).c_str(), &freq, kFreqDragSpeed, 0.0f, 50.0f, "%.2f")) {
        frequency_ = freq;
    }

    // 位相
    float ph = phase_;
    if (ImGui::DragFloat(("Phase (rad)" + idPrefix).c_str(), &ph, kFreqDragSpeed, -3.14f, 3.14f, "%.2f")) {
        phase_ = ph;
    }

    // 基準値の再取得
    if (ImGui::Button(("Recapture Base Value" + idPrefix).c_str())) {
        CaptureBaseValue();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(Base: %.1f, %.1f, %.1f)", baseValue_.x, baseValue_.y, baseValue_.z);
#endif
}
