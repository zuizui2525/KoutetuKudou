#include "Engine/Component/Components/TextRenderer2DComponent.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Graphics/Objects/2d/Text/TextObject2D.h"
#include "Engine/Graphics/Text/TextTextureGenerator.h"
#include "Engine/Debug/SceneHierarchy.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Zuizui.h"
#include "Engine/Base/DxCommon/DxCommon.h"

#ifdef _USEIMGUI
#include <imgui.h>
#endif

namespace {
    // インスペクター編集用定数 (マジックナンバー排除)
    constexpr size_t kTextBufferSize = 512;
    constexpr float kMinFontSize = 8.0f;
    constexpr float kMaxFontSize = 256.0f;
    constexpr float kFontSizeDragSpeed = 0.5f;
    constexpr float kDragSpeed = 0.5f;
    constexpr float kMinScale = 0.001f;
    constexpr float kMaxScale = 1000.0f;

    constexpr float kMinOutlineWidth = 1.0f;
    constexpr float kMaxOutlineWidth = 10.0f;
    constexpr float kOutlineWidthDragSpeed = 0.1f;

    const char* const kFontPresets[] = {
        "Yu Gothic UI",
        "Meiryo",
        "MS Gothic",
        "Arial",
        "Impact",
        "Consolas",
        "Segoe UI"
    };
    constexpr int kFontPresetsCount = sizeof(kFontPresets) / sizeof(kFontPresets[0]);

    const char* const kAlignmentNames[] = {
        "Left",
        "Center",
        "Right"
    };
    constexpr int kAlignmentNamesCount = sizeof(kAlignmentNames) / sizeof(kAlignmentNames[0]);
}

TextRenderer2DComponent::TextRenderer2DComponent() {
    textObject_ = std::make_unique<TextObject2D>();
    textureGenerator_ = std::make_unique<TextTextureGenerator>();
}

TextRenderer2DComponent::~TextRenderer2DComponent() {
    if (auto engine = EngineResource::GetEngine()) {
        if (auto dxCommon = engine->GetDxCommon()) {
            dxCommon->FlushGPU();
        }
    }
}

void TextRenderer2DComponent::Initialize() {
    if (textObject_) {
        textObject_->Initialize();
        SceneHierarchy::GetInstance()->Unregister(textObject_.get());
    }
    UpdateTextTexture();
}

void TextRenderer2DComponent::UpdateTextTexture() {
    if (!textureGenerator_ || !textObject_) return;

    textureGenerator_->Generate(
        text_, fontName_, fontSize_, alignment_, isBold_, isItalic_,
        enableOutline_, outlineColor_, outlineWidth_, color_
    );
    textObject_->SetSize(
        static_cast<float>(textureGenerator_->GetWidth()),
        static_cast<float>(textureGenerator_->GetHeight())
    );
    if (enableOutline_) {
        // アウトライン有効時はテクスチャにRGBがベイクされているため、アルファ値のみ乗算
        textObject_->SetColor({ 1.0f, 1.0f, 1.0f, color_.w });
    } else {
        textObject_->SetColor(color_);
    }
    isTextureDirty_ = false;
}

void TextRenderer2DComponent::SetText(const std::string& text) {
    if (text_ != text) {
        text_ = text;
        isTextureDirty_ = true;
    }
}

void TextRenderer2DComponent::SetFontName(const std::string& fontName) {
    if (fontName_ != fontName) {
        fontName_ = fontName;
        isTextureDirty_ = true;
    }
}

void TextRenderer2DComponent::SetFontSize(float fontSize) {
    if (fontSize_ != fontSize) {
        fontSize_ = fontSize;
        isTextureDirty_ = true;
    }
}

void TextRenderer2DComponent::SetColor(const Vector4& color) {
    const bool isRgbChanged = (color_.x != color.x || color_.y != color.y || color_.z != color.z);
    const bool isAlphaChanged = (color_.w != color.w);

    color_ = color;

    if (enableOutline_) {
        if (isRgbChanged) {
            isTextureDirty_ = true;
        } else if (isAlphaChanged && textObject_) {
            constexpr float kFullWhiteChannel = 1.0f;
            textObject_->SetColor({ kFullWhiteChannel, kFullWhiteChannel, kFullWhiteChannel, color_.w });
        }
    } else if (textObject_) {
        textObject_->SetColor(color_);
    }
}

void TextRenderer2DComponent::SetAlignment(int alignment) {
    if (alignment_ != alignment) {
        alignment_ = alignment;
        isTextureDirty_ = true;
    }
}

void TextRenderer2DComponent::SetBold(bool bold) {
    if (isBold_ != bold) {
        isBold_ = bold;
        isTextureDirty_ = true;
    }
}

void TextRenderer2DComponent::SetItalic(bool italic) {
    if (isItalic_ != italic) {
        isItalic_ = italic;
        isTextureDirty_ = true;
    }
}

void TextRenderer2DComponent::SetOutlineEnabled(bool enable) {
    if (enableOutline_ != enable) {
        enableOutline_ = enable;
        isTextureDirty_ = true;
    }
}

void TextRenderer2DComponent::SetOutlineColor(const Vector4& color) {
    if (outlineColor_.x != color.x || outlineColor_.y != color.y || outlineColor_.z != color.z || outlineColor_.w != color.w) {
        outlineColor_ = color;
        if (enableOutline_) {
            isTextureDirty_ = true;
        }
    }
}

void TextRenderer2DComponent::SetOutlineWidth(float width) {
    if (outlineWidth_ != width) {
        outlineWidth_ = width;
        if (enableOutline_) {
            isTextureDirty_ = true;
        }
    }
}

float TextRenderer2DComponent::GetWidth() const {
    return textObject_ ? textObject_->GetWidth() : 0.0f;
}

float TextRenderer2DComponent::GetHeight() const {
    return textObject_ ? textObject_->GetHeight() : 0.0f;
}

void TextRenderer2DComponent::Update() {
    if (!owner_) return;

    if (isTextureDirty_) {
        UpdateTextTexture();
    }

    if (textObject_) {
        Transform combinedTr{};
        const auto& ownerTr = owner_->GetTransform();
        combinedTr.translate = { ownerTr.translate.x + offset_.translate.x, ownerTr.translate.y + offset_.translate.y, ownerTr.translate.z + offset_.translate.z };
        combinedTr.rotate = { ownerTr.rotate.x + offset_.rotate.x, ownerTr.rotate.y + offset_.rotate.y, ownerTr.rotate.z + offset_.rotate.z };
        combinedTr.scale = { ownerTr.scale.x * offset_.scale.x, ownerTr.scale.y * offset_.scale.y, ownerTr.scale.z * offset_.scale.z };

        textObject_->SetTransform(combinedTr);
        textObject_->SetVisible(owner_->IsVisible() && isActive_);
        textObject_->Update();
    }
}

void TextRenderer2DComponent::Draw2D() {
    if (!isActive_ || !owner_ || !owner_->IsVisible() || !textObject_ || !textureGenerator_) return;

    Transform combinedTr{};
    const auto& ownerTr = owner_->GetTransform();
    combinedTr.translate = { ownerTr.translate.x + offset_.translate.x, ownerTr.translate.y + offset_.translate.y, ownerTr.translate.z + offset_.translate.z };
    combinedTr.rotate = { ownerTr.rotate.x + offset_.rotate.x, ownerTr.rotate.y + offset_.rotate.y, ownerTr.rotate.z + offset_.rotate.z };
    combinedTr.scale = { ownerTr.scale.x * offset_.scale.x, ownerTr.scale.y * offset_.scale.y, ownerTr.scale.z * offset_.scale.z };

    textObject_->SetTransform(combinedTr);
    textObject_->SetVisible(true);
    textObject_->Update();
    textObject_->Draw(textureGenerator_->GetGpuHandle(), true);
}

void TextRenderer2DComponent::DrawInspector() {
#ifdef _USEIMGUI
    std::string idPrefix = "##Text2D_" + std::to_string(reinterpret_cast<uintptr_t>(this));

    // ローカルオフセット (個別Transform)
    if (ImGui::TreeNodeEx(("Offset Transform (個別の配置)" + idPrefix).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3(("Position" + idPrefix + "_Pos").c_str(), &offset_.translate.x, kDragSpeed, 0.0f, 0.0f, "%.1f px");
        ImGui::DragFloat3(("Rotation" + idPrefix + "_Rot").c_str(), &offset_.rotate.x, 0.05f, 0.0f, 0.0f, "%.2f");
        ImGui::DragFloat3(("Scale" + idPrefix + "_Scl").c_str(), &offset_.scale.x, 0.05f, kMinScale, kMaxScale, "%.2f");
        if (ImGui::Button(("Reset Offset" + idPrefix).c_str())) {
            offset_ = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
        }
        ImGui::TreePop();
    }
    ImGui::Separator();

    // テキスト編集
    char buf[kTextBufferSize] = {};
    strncpy_s(buf, text_.c_str(), sizeof(buf) - 1);
    if (ImGui::InputTextMultiline(("Text" + idPrefix).c_str(), buf, sizeof(buf), ImVec2(-1.0f, 60.0f))) {
        SetText(buf);
    }

    // フォント選択 (プリセット)
    int currentFontIndex = -1;
    for (int i = 0; i < kFontPresetsCount; ++i) {
        if (fontName_ == kFontPresets[i]) {
            currentFontIndex = i;
            break;
        }
    }

    const char* previewFont = (currentFontIndex >= 0) ? kFontPresets[currentFontIndex] : fontName_.c_str();
    if (ImGui::BeginCombo(("Font" + idPrefix).c_str(), previewFont)) {
        for (int i = 0; i < kFontPresetsCount; ++i) {
            bool isSelected = (currentFontIndex == i);
            if (ImGui::Selectable(kFontPresets[i], isSelected)) {
                SetFontName(kFontPresets[i]);
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    // フォントサイズ
    float sizeVal = fontSize_;
    if (ImGui::DragFloat(("Size" + idPrefix).c_str(), &sizeVal, kFontSizeDragSpeed, kMinFontSize, kMaxFontSize, "%.1f px")) {
        SetFontSize(sizeVal);
    }

    // 文字色
    float col[4] = { color_.x, color_.y, color_.z, color_.w };
    if (ImGui::ColorEdit4(("Color" + idPrefix).c_str(), col, ImGuiColorEditFlags_AlphaBar)) {
        SetColor({ col[0], col[1], col[2], col[3] });
    }

    // アライメント
    int alignVal = alignment_;
    if (ImGui::Combo(("Alignment" + idPrefix).c_str(), &alignVal, kAlignmentNames, kAlignmentNamesCount)) {
        SetAlignment(alignVal);
    }

    // スタイル (Bold / Italic)
    bool boldVal = isBold_;
    if (ImGui::Checkbox(("Bold" + idPrefix).c_str(), &boldVal)) {
        SetBold(boldVal);
    }
    ImGui::SameLine();
    bool italicVal = isItalic_;
    if (ImGui::Checkbox(("Italic" + idPrefix).c_str(), &italicVal)) {
        SetItalic(italicVal);
    }

    // サイズ情報表示 (読み取り専用)
    ImGui::TextDisabled("Render Size: %.0f x %.0f px", GetWidth(), GetHeight());

    // アウトライン設定
    ImGui::Separator();
    bool outlineVal = enableOutline_;
    if (ImGui::Checkbox(("Enable Outline" + idPrefix).c_str(), &outlineVal)) {
        SetOutlineEnabled(outlineVal);
    }
    if (enableOutline_) {
        float outlineCol[4] = { outlineColor_.x, outlineColor_.y, outlineColor_.z, outlineColor_.w };
        if (ImGui::ColorEdit4(("Outline Color" + idPrefix).c_str(), outlineCol, ImGuiColorEditFlags_AlphaBar)) {
            SetOutlineColor({ outlineCol[0], outlineCol[1], outlineCol[2], outlineCol[3] });
        }
        float outWidthVal = outlineWidth_;
        if (ImGui::DragFloat(("Outline Width" + idPrefix).c_str(), &outWidthVal, kOutlineWidthDragSpeed, kMinOutlineWidth, kMaxOutlineWidth, "%.1f px")) {
            SetOutlineWidth(outWidthVal);
        }
    }
#endif
}
