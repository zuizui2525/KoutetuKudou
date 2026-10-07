#pragma once
#include "Engine/Component/IComponent.h"
#include "Engine/Math/MathStructs.h"
#include <memory>
#include <string>

class TextObject2D;
class TextTextureGenerator;

/**
 * @brief 2Dテキスト描画を担うコンポーネント (Unityライクな2Dテキストレンダラー)
 * GameObjectにアタッチすることで、スクリーン空間上に任意のテキストを描画します。
 */
class TextRenderer2DComponent : public IComponent {
public:
    TextRenderer2DComponent();
    ~TextRenderer2DComponent() override;

    void Initialize() override;
    void Update() override;
    void Draw2D() override;
    void DrawInspector() override;

    std::string GetComponentTypeName() const override { return "TextRenderer2D"; }

    // ==========================================
    // テキスト・フォント・スタイル操作
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

    float GetWidth() const;
    float GetHeight() const;

    // ==========================================
    // ローカルオフセット (個別Transform)
    // ==========================================
    Transform& GetOffset() { return offset_; }
    const Transform& GetOffset() const { return offset_; }
    void SetOffset(const Transform& offset) { offset_ = offset; }

private:
    void UpdateTextTexture();

private:
    std::unique_ptr<TextObject2D> textObject_;
    std::unique_ptr<TextTextureGenerator> textureGenerator_;

    Transform offset_{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

    std::string text_ = "Text";
    std::string fontName_ = "Yu Gothic UI";
    float fontSize_ = 32.0f;
    Vector4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    int alignment_ = 0; // 0: Left, 1: Center, 2: Right
    bool isBold_ = false;
    bool isItalic_ = false;

    // アウトライン
    bool enableOutline_ = false;
    Vector4 outlineColor_ = { 0.0f, 0.0f, 0.0f, 1.0f };
    float outlineWidth_ = 2.0f;

    bool isTextureDirty_ = true;
};
