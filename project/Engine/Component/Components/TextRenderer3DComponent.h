#pragma once
#include "Engine/Component/IComponent.h"
#include "Engine/Graphics/Objects/3d/Text/TextObject3D.h"
#include "Engine/Math/MathStructs.h"
#include <memory>
#include <string>

class TextTextureGenerator;

/**
 * @brief 3D空間へのテキスト描画を担うコンポーネント (ビルボード選択可能)
 * 看板、3Dラベル、キャラクター頭上ネームタグ、浮遊ダメージ表示などを実現します。
 */
class TextRenderer3DComponent : public IComponent {
public:
    TextRenderer3DComponent();
    ~TextRenderer3DComponent() override;

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void DrawInspector() override;

    std::string GetComponentTypeName() const override { return "TextRenderer3D"; }

    // ==========================================
    // パラメータ操作
    // ==========================================
    const std::string& GetText() const { return text_; }
    void SetText(const std::string& text);

    const std::string& GetFontName() const { return fontName_; }
    void SetFontName(const std::string& fontName);

    float GetFontSize() const { return fontSize_; }
    void SetFontSize(float fontSize);

    const Vector4& GetColor() const { return color_; }
    void SetColor(const Vector4& color);

    int GetAlignment() const { return alignment_; }
    void SetAlignment(int alignment);

    bool IsBold() const { return isBold_; }
    void SetBold(bool bold);

    bool IsItalic() const { return isItalic_; }
    void SetItalic(bool italic);

    // ==========================================
    // アウトライン (縁取り) 操作
    // ==========================================
    bool IsOutlineEnabled() const { return enableOutline_; }
    void SetOutlineEnabled(bool enable);

    const Vector4& GetOutlineColor() const { return outlineColor_; }
    void SetOutlineColor(const Vector4& color);

    float GetOutlineWidth() const { return outlineWidth_; }
    void SetOutlineWidth(float width);

    TextObject3D::BillboardMode GetBillboardMode() const { return billboardMode_; }
    void SetBillboardMode(TextObject3D::BillboardMode mode);

    float GetWorldScale() const { return worldScale_; }
    void SetWorldScale(float scale);

    int GetLightingMode() const { return enableLighting_; }
    void SetLightingMode(int mode);

    // ==========================================
    // ローカルオフセット (個別Transform)
    // ==========================================
    Transform& GetOffset() { return offset_; }
    const Transform& GetOffset() const { return offset_; }
    void SetOffset(const Transform& offset) { offset_ = offset; }

private:
    void UpdateTextTexture();

private:
    std::unique_ptr<TextObject3D> textObject_;
    std::unique_ptr<TextTextureGenerator> textureGenerator_;

    Transform offset_{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

    std::string text_ = "3D Text";
    std::string fontName_ = "Yu Gothic UI";
    float fontSize_ = 48.0f;
    Vector4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    int alignment_ = 1; // デフォルト中央揃え
    bool isBold_ = false;
    bool isItalic_ = false;

    // アウトライン
    bool enableOutline_ = false;
    Vector4 outlineColor_ = { 0.0f, 0.0f, 0.0f, 1.0f };
    float outlineWidth_ = 2.0f;

    TextObject3D::BillboardMode billboardMode_ = TextObject3D::BillboardMode::AllAxis;
    float worldScale_ = 0.02f; // 1ピクセルあたりの3Dワールド単位 (0.02f = 100pxあたり2.0m)
    int enableLighting_ = 0;   // デフォルト: ライティング無効で明るく描画

    bool isTextureDirty_ = true;
};
