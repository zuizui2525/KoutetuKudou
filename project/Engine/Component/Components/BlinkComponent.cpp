#include "Engine/Component/Components/BlinkComponent.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/Components/TextRenderer2DComponent.h"
#include "Engine/Component/Components/SpriteRendererComponent.h"
#include <cmath>

#ifdef _USEIMGUI
#include <imgui.h>
#endif

namespace {
    // デルタタイム定数（60FPS基準: マジックナンバー排除）
    constexpr float kDefaultDeltaTime = 1.0f / 60.0f;

    // インスペクター編集用定数
    constexpr float kSpeedDrag = 0.05f;
    constexpr float kAlphaDrag = 0.01f;
    constexpr float kMinAlphaLimit = 0.0f;
    constexpr float kMaxAlphaLimit = 1.0f;
    constexpr float kSineOffset = 1.0f;
    constexpr float kSineScale = 0.5f;
}

void BlinkComponent::Update() {
    if (!owner_ || !isActive_) return;

    timer_ += kDefaultDeltaTime;
    // 0.0f 〜 1.0f の滑らかな波形を生成
    float sinVal = (std::sin(timer_ * blinkSpeed_) + kSineOffset) * kSineScale;
    float currentAlpha = minAlpha_ + (maxAlpha_ - minAlpha_) * sinVal;

    // TextRenderer2DComponent がアタッチされている場合、その色のAlphaを更新
    if (auto* textComp = owner_->GetComponent<TextRenderer2DComponent>()) {
        Vector4 col = textComp->GetColor();
        col.w = currentAlpha;
        textComp->SetColor(col);
    }

    // SpriteRendererComponent がアタッチされている場合、その色のAlphaを更新
    if (auto* spriteComp = owner_->GetComponent<SpriteRendererComponent>()) {
        Vector4 col = spriteComp->GetColor();
        col.w = currentAlpha;
        spriteComp->SetColor(col);
    }
}

void BlinkComponent::DrawInspector() {
#ifdef _USEIMGUI
    std::string idPrefix = "##Blink_" + std::to_string(reinterpret_cast<uintptr_t>(this));

    float speed = blinkSpeed_;
    if (ImGui::DragFloat(("Blink Speed (rad/s)" + idPrefix).c_str(), &speed, kSpeedDrag, 0.0f, 30.0f, "%.2f")) {
        blinkSpeed_ = speed;
    }

    float minA = minAlpha_;
    if (ImGui::SliderFloat(("Min Alpha" + idPrefix).c_str(), &minA, kMinAlphaLimit, maxAlpha_, "%.2f")) {
        minAlpha_ = minA;
    }

    float maxA = maxAlpha_;
    if (ImGui::SliderFloat(("Max Alpha" + idPrefix).c_str(), &maxA, minAlpha_, kMaxAlphaLimit, "%.2f")) {
        maxAlpha_ = maxA;
    }
#endif
}
