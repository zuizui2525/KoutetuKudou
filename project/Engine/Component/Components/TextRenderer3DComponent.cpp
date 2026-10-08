#include "Engine/Component/Components/TextRenderer3DComponent.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Graphics/Objects/3d/Text/TextObject3D.h"
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
    constexpr float kMinWorldScale = 0.001f;
    constexpr float kMaxWorldScale = 1.0f;
    constexpr float kWorldScaleDragSpeed = 0.001f;
    constexpr float kDragSpeed = 0.05f;
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

    const char* const kBillboardModeNames[] = {
        "None (3D回転固定)",
        "AllAxis (カメラ全軸正対)",
        "YAxisOnly (垂直軸のみカメラ追従)"
    };
    constexpr int kBillboardModeCount = sizeof(kBillboardModeNames) / sizeof(kBillboardModeNames[0]);

    const char* const kLightingModeNames[] = {
        "None (無効)",
        "Lambert",
        "HalfLambert"
    };
    constexpr int kLightingModeCount = sizeof(kLightingModeNames) / sizeof(kLightingModeNames[0]);
}

TextRenderer3DComponent::TextRenderer3DComponent() {
    textObject_ = std::make_unique<TextObject3D>();
    textureGenerator_ = std::make_unique<TextTextureGenerator>();
}

TextRenderer3DComponent::~TextRenderer3DComponent() {
    if (auto engine = EngineResource::GetEngine()) {
        if (auto dxCommon = engine->GetDxCommon()) {
            dxCommon->FlushGPU();
        }
    }
}

void TextRenderer3DComponent::Initialize() {
    if (textObject_) {
        textObject_->Initialize(enableLighting_);
        textObject_->SetBillboardMode(billboardMode_);
        SceneHierarchy::GetInstance()->Unregister(textObject_.get());
    }
    UpdateTextTexture();
}

void TextRenderer3DComponent::UpdateTextTexture() {
    if (!textureGenerator_ || !textObject_) return;

    textureGenerator_->Generate(
        text_, fontName_, fontSize_, alignment_, isBold_, isItalic_,
        enableOutline_, outlineColor_, outlineWidth_, color_
    );
    float w = static_cast<float>(textureGenerator_->GetWidth()) * worldScale_;
    float h = static_cast<float>(textureGenerator_->GetHeight()) * worldScale_;
    textObject_->SetSize({ w, h });
    if (enableOutline_) {
        // アウトライン有効時はテクスチャにRGBがベイクされているため、アルファ値のみ乗算
        textObject_->SetColor({ 1.0f, 1.0f, 1.0f, color_.w });
    } else {
        textObject_->SetColor(color_);
    }
    isTextureDirty_ = false;
}

void TextRenderer3DComponent::SetText(const std::string& text) {
    if (text_ != text) {
        text_ = text;
        isTextureDirty_ = true;
    }
}

void TextRenderer3DComponent::SetFontName(const std::string& fontName) {
    if (fontName_ != fontName) {
        fontName_ = fontName;
        isTextureDirty_ = true;
    }
}

void TextRenderer3DComponent::SetFontSize(float fontSize) {
    if (fontSize_ != fontSize) {
        fontSize_ = fontSize;
        isTextureDirty_ = true;
    }
}

void TextRenderer3DComponent::SetColor(const Vector4& color) {
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

void TextRenderer3DComponent::SetAlignment(int alignment) {
    if (alignment_ != alignment) {
        alignment_ = alignment;
        isTextureDirty_ = true;
    }
}

void TextRenderer3DComponent::SetBold(bool bold) {
    if (isBold_ != bold) {
        isBold_ = bold;
        isTextureDirty_ = true;
    }
}

void TextRenderer3DComponent::SetItalic(bool italic) {
    if (isItalic_ != italic) {
        isItalic_ = italic;
        isTextureDirty_ = true;
    }
}

void TextRenderer3DComponent::SetOutlineEnabled(bool enable) {
    if (enableOutline_ != enable) {
        enableOutline_ = enable;
        isTextureDirty_ = true;
    }
}

void TextRenderer3DComponent::SetOutlineColor(const Vector4& color) {
    if (outlineColor_.x != color.x || outlineColor_.y != color.y || outlineColor_.z != color.z || outlineColor_.w != color.w) {
        outlineColor_ = color;
        if (enableOutline_) {
            isTextureDirty_ = true;
        }
    }
}

void TextRenderer3DComponent::SetOutlineWidth(float width) {
    if (outlineWidth_ != width) {
        outlineWidth_ = width;
        if (enableOutline_) {
            isTextureDirty_ = true;
        }
    }
}

void TextRenderer3DComponent::SetBillboardMode(TextObject3D::BillboardMode mode) {
    billboardMode_ = mode;
    if (textObject_) {
        textObject_->SetBillboardMode(mode);
    }
}

void TextRenderer3DComponent::SetWorldScale(float scale) {
    if (worldScale_ != scale) {
        worldScale_ = scale;
        isTextureDirty_ = true;
    }
}

void TextRenderer3DComponent::SetLightingMode(int mode) {
    enableLighting_ = mode;
    if (textObject_ && textObject_->GetMaterialData()) {
        textObject_->GetMaterialData()->enableLighting = mode;
    }
}

void TextRenderer3DComponent::Update() {
    if (!owner_) return;

    if (isTextureDirty_) {
        UpdateTextTexture();
    }

    if (textObject_) {
        Transform combinedTr{};
        const auto& ownerTr = owner_->GetTransform();
        Matrix4x4 localMat = Math::MakeAffineMatrix(offset_.scale, offset_.rotate, offset_.translate);
        Matrix4x4 combinedWorld = Math::Multiply(localMat, owner_->GetWorldMatrix());
        combinedTr.translate = { combinedWorld.m[3][0], combinedWorld.m[3][1], combinedWorld.m[3][2] };
        combinedTr.rotate = { ownerTr.rotate.x + offset_.rotate.x, ownerTr.rotate.y + offset_.rotate.y, ownerTr.rotate.z + offset_.rotate.z };
        combinedTr.scale = { ownerTr.scale.x * offset_.scale.x, ownerTr.scale.y * offset_.scale.y, ownerTr.scale.z * offset_.scale.z };

        textObject_->SetTransform(combinedTr);
        textObject_->SetBillboardMode(billboardMode_);
        textObject_->SetVisible(owner_->IsVisible() && isActive_);
        textObject_->Update();
    }
}

void TextRenderer3DComponent::Draw() {
    if (!isActive_ || !owner_ || !owner_->IsVisible() || !textObject_ || !textureGenerator_) return;

    Transform combinedTr{};
    const auto& ownerTr = owner_->GetTransform();
    Matrix4x4 localMat = Math::MakeAffineMatrix(offset_.scale, offset_.rotate, offset_.translate);
    Matrix4x4 combinedWorld = Math::Multiply(localMat, owner_->GetWorldMatrix());
    combinedTr.translate = { combinedWorld.m[3][0], combinedWorld.m[3][1], combinedWorld.m[3][2] };
    combinedTr.rotate = { ownerTr.rotate.x + offset_.rotate.x, ownerTr.rotate.y + offset_.rotate.y, ownerTr.rotate.z + offset_.rotate.z };
    combinedTr.scale = { ownerTr.scale.x * offset_.scale.x, ownerTr.scale.y * offset_.scale.y, ownerTr.scale.z * offset_.scale.z };

    textObject_->SetTransform(combinedTr);
    textObject_->SetBillboardMode(billboardMode_);
    textObject_->SetVisible(true);
    textObject_->Update();
    textObject_->Draw(textureGenerator_->GetGpuHandle());
}

void TextRenderer3DComponent::DrawInspector() {
#ifdef _USEIMGUI
    std::string idPrefix = "##Text3D_" + std::to_string(reinterpret_cast<uintptr_t>(this));

    // ローカルオフセット (個別Transform)
    if (ImGui::TreeNodeEx(("Offset Transform (個別の配置)" + idPrefix).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3(("Position" + idPrefix + "_Pos").c_str(), &offset_.translate.x, kDragSpeed, 0.0f, 0.0f, "%.2f");
        ImGui::DragFloat3(("Rotation" + idPrefix + "_Rot").c_str(), &offset_.rotate.x, kDragSpeed, 0.0f, 0.0f, "%.2f");
        ImGui::DragFloat3(("Scale" + idPrefix + "_Scl").c_str(), &offset_.scale.x, kDragSpeed, kMinScale, kMaxScale, "%.2f");
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
    if (ImGui::DragFloat(("Font Size" + idPrefix).c_str(), &sizeVal, kFontSizeDragSpeed, kMinFontSize, kMaxFontSize, "%.1f px")) {
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

    // ビルボードモード選択 (ユーザー要望の選択肢)
    int billboardVal = static_cast<int>(billboardMode_);
    if (ImGui::Combo(("Billboard Mode" + idPrefix).c_str(), &billboardVal, kBillboardModeNames, kBillboardModeCount)) {
        SetBillboardMode(static_cast<TextObject3D::BillboardMode>(billboardVal));
    }

    // 3D空間スケール
    float scaleVal = worldScale_;
    if (ImGui::DragFloat(("World Scale" + idPrefix).c_str(), &scaleVal, kWorldScaleDragSpeed, kMinWorldScale, kMaxWorldScale, "%.4f")) {
        SetWorldScale(scaleVal);
    }

    // ライティング
    int lightVal = enableLighting_;
    if (ImGui::Combo(("Lighting" + idPrefix).c_str(), &lightVal, kLightingModeNames, kLightingModeCount)) {
        SetLightingMode(lightVal);
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

    // メッシュ寸法情報表示
    if (textObject_) {
        const auto& s = textObject_->GetSize();
        ImGui::TextDisabled("3D Plane Size: %.2f x %.2f m", s.x, s.y);
    }

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
