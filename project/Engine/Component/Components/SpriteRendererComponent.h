#pragma once
#include "Engine/Component/IComponent.h"
#include "Engine/Math/MathStructs.h"
#include <memory>
#include <string>

class SpriteObject;
class Triangle2DObject;
class Circle2DObject;
class Ring2DObject;
class Line2DObject;

/// <summary>
/// 2Dオブジェクト（スプライト、幾何プリミティブ）描画を担うコンポーネント (Unityライクな2Dレンダラー)
/// </summary>
class SpriteRendererComponent : public IComponent {
public:
    enum class ShapeType {
        Sprite,
        Triangle,
        Circle,
        Ring,
        Line
    };

    SpriteRendererComponent();
    ~SpriteRendererComponent() override;

    void Initialize() override;
    void Update() override;
    void Draw2D() override;
    void DrawInspector() override;

    std::string GetComponentTypeName() const override { return "SpriteRenderer"; }

    // ==========================================
    // 形状・パラメータ操作
    // ==========================================
    ShapeType GetShapeType() const { return shapeType_; }
    void SetShapeType(ShapeType type);

    const std::string& GetTextureKey() const { return textureKey_; }
    void SetTextureKey(const std::string& key) { textureKey_ = key; }

    const Vector2& GetSize() const { return size_; }
    void SetSize(const Vector2& size);

    float GetRadius() const { return radius_; }
    void SetRadius(float radius);

    float GetInnerRadius() const { return innerRadius_; }
    void SetInnerRadius(float innerRadius);

    const Vector2& GetLineStart() const { return lineStart_; }
    void SetLineStart(const Vector2& start);

    const Vector2& GetLineEnd() const { return lineEnd_; }
    void SetLineEnd(const Vector2& end);

    float GetLineThickness() const { return lineThickness_; }
    void SetLineThickness(float thickness);

    const Vector4& GetColor() const { return color_; }
    void SetColor(const Vector4& color);

private:
    void RecreateShapeObject();

private:
    ShapeType shapeType_ = ShapeType::Sprite;

    std::unique_ptr<SpriteObject> spriteObject_;
    std::unique_ptr<Triangle2DObject> triangleObject_;
    std::unique_ptr<Circle2DObject> circleObject_;
    std::unique_ptr<Ring2DObject> ringObject_;
    std::unique_ptr<Line2DObject> lineObject_;

    std::string textureKey_ = "white";
    Vector2 size_ = { 100.0f, 100.0f };
    float radius_ = 50.0f;
    float innerRadius_ = 35.0f;
    Vector2 lineStart_ = { 0.0f, 0.0f };
    Vector2 lineEnd_ = { 150.0f, 0.0f };
    float lineThickness_ = 4.0f;
    Vector4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f };
};
