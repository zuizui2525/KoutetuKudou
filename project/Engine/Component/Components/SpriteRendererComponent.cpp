#include "Engine/Component/Components/SpriteRendererComponent.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Graphics/Objects/2d/Sprite/SpriteObject.h"
#include "Engine/Graphics/Objects/2d/Triangle/Triangle2DObject.h"
#include "Engine/Graphics/Objects/2d/Circle/Circle2DObject.h"
#include "Engine/Graphics/Objects/2d/Ring/Ring2DObject.h"
#include "Engine/Graphics/Objects/2d/Line/Line2DObject.h"
#include "Engine/Debug/SceneHierarchy.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Zuizui.h"
#include "Engine/Base/DxCommon/DxCommon.h"

#ifdef _USEIMGUI
#include <imgui.h>
#endif

namespace {
    // インスペクター編集用定数 (マジックナンバー排除)
    constexpr size_t kTextBufferSize = 128;
    constexpr float kSizeDragSpeed = 1.0f;
    constexpr float kMinSize = 1.0f;
    constexpr float kMaxSize = 4096.0f;
    constexpr float kRadiusDragSpeed = 0.5f;
    constexpr float kMinRadius = 0.5f;
    constexpr float kMaxRadius = 2048.0f;
    constexpr float kLineDragSpeed = 1.0f;
    constexpr float kMinThickness = 0.5f;
    constexpr float kMaxThickness = 256.0f;
    constexpr float kDragSpeed = 0.5f;
    constexpr float kMinScale = 0.001f;
    constexpr float kMaxScale = 1000.0f;

    constexpr const char* kShapeNames[] = {
        "Sprite",
        "Triangle",
        "Circle",
        "Ring",
        "Line"
    };
}

SpriteRendererComponent::SpriteRendererComponent() {
    spriteObject_ = std::make_unique<SpriteObject>();
}

SpriteRendererComponent::~SpriteRendererComponent() {
    // コンポーネント破棄時にGPU処理完了を待機（リソース破棄クラッシュを完全防止）
    if (auto engine = EngineResource::GetEngine()) {
        if (auto dxCommon = engine->GetDxCommon()) {
            dxCommon->FlushGPU();
        }
    }
}

void SpriteRendererComponent::Initialize() {
    RecreateShapeObject();
}

void SpriteRendererComponent::SetShapeType(ShapeType type) {
    if (shapeType_ == type) return;
    shapeType_ = type;
    RecreateShapeObject();
}

void SpriteRendererComponent::RecreateShapeObject() {
    // 形状切替前にGPU完了待機
    if (auto engine = EngineResource::GetEngine()) {
        if (auto dxCommon = engine->GetDxCommon()) {
            dxCommon->FlushGPU();
        }
    }

    spriteObject_.reset();
    triangleObject_.reset();
    circleObject_.reset();
    ringObject_.reset();
    lineObject_.reset();

    switch (shapeType_) {
    case ShapeType::Sprite:
        spriteObject_ = std::make_unique<SpriteObject>();
        spriteObject_->Initialize();
        spriteObject_->SetSize(size_.x, size_.y);
        if (auto* mat = spriteObject_->GetMaterialData()) mat->color = color_;
        SceneHierarchy::GetInstance()->Unregister(spriteObject_.get());
        break;
    case ShapeType::Triangle:
        triangleObject_ = std::make_unique<Triangle2DObject>();
        triangleObject_->Initialize();
        triangleObject_->SetSize(size_.x, size_.y);
        if (auto* mat = triangleObject_->GetMaterialData()) mat->color = color_;
        SceneHierarchy::GetInstance()->Unregister(triangleObject_.get());
        break;
    case ShapeType::Circle:
        circleObject_ = std::make_unique<Circle2DObject>();
        circleObject_->Initialize();
        circleObject_->SetRadius(radius_);
        if (auto* mat = circleObject_->GetMaterialData()) mat->color = color_;
        SceneHierarchy::GetInstance()->Unregister(circleObject_.get());
        break;
    case ShapeType::Ring:
        ringObject_ = std::make_unique<Ring2DObject>();
        ringObject_->Initialize();
        ringObject_->SetRadii(radius_, innerRadius_);
        if (auto* mat = ringObject_->GetMaterialData()) mat->color = color_;
        SceneHierarchy::GetInstance()->Unregister(ringObject_.get());
        break;
    case ShapeType::Line:
        lineObject_ = std::make_unique<Line2DObject>();
        lineObject_->Initialize();
        lineObject_->SetPoints(lineStart_, lineEnd_);
        lineObject_->SetThickness(lineThickness_);
        if (auto* mat = lineObject_->GetMaterialData()) mat->color = color_;
        SceneHierarchy::GetInstance()->Unregister(lineObject_.get());
        break;
    }
}

void SpriteRendererComponent::SetSize(const Vector2& size) {
    size_ = size;
    if (spriteObject_) {
        spriteObject_->SetSize(size_.x, size_.y);
    }
    if (triangleObject_) {
        triangleObject_->SetSize(size_.x, size_.y);
    }
}

void SpriteRendererComponent::SetRadius(float radius) {
    radius_ = radius;
    if (circleObject_) {
        circleObject_->SetRadius(radius_);
    }
    if (ringObject_) {
        ringObject_->SetRadii(radius_, innerRadius_);
    }
}

void SpriteRendererComponent::SetInnerRadius(float innerRadius) {
    innerRadius_ = innerRadius;
    if (ringObject_) {
        ringObject_->SetRadii(radius_, innerRadius_);
    }
}

void SpriteRendererComponent::SetLineStart(const Vector2& start) {
    lineStart_ = start;
    if (lineObject_) {
        lineObject_->SetPoints(lineStart_, lineEnd_);
    }
}

void SpriteRendererComponent::SetLineEnd(const Vector2& end) {
    lineEnd_ = end;
    if (lineObject_) {
        lineObject_->SetPoints(lineStart_, lineEnd_);
    }
}

void SpriteRendererComponent::SetLineThickness(float thickness) {
    lineThickness_ = thickness;
    if (lineObject_) {
        lineObject_->SetThickness(lineThickness_);
    }
}

void SpriteRendererComponent::SetColor(const Vector4& color) {
    color_ = color;
    if (spriteObject_) {
        if (auto* mat = spriteObject_->GetMaterialData()) mat->color = color_;
    }
    if (triangleObject_) {
        if (auto* mat = triangleObject_->GetMaterialData()) mat->color = color_;
    }
    if (circleObject_) {
        if (auto* mat = circleObject_->GetMaterialData()) mat->color = color_;
    }
    if (ringObject_) {
        if (auto* mat = ringObject_->GetMaterialData()) mat->color = color_;
    }
    if (lineObject_) {
        if (auto* mat = lineObject_->GetMaterialData()) mat->color = color_;
    }
}

void SpriteRendererComponent::Update() {
    if (!owner_) return;
    const auto& tr = owner_->GetTransform();
    Transform combinedTr{};
    combinedTr.translate = { tr.translate.x + offset_.translate.x, tr.translate.y + offset_.translate.y, tr.translate.z + offset_.translate.z };
    combinedTr.rotate = { tr.rotate.x + offset_.rotate.x, tr.rotate.y + offset_.rotate.y, tr.rotate.z + offset_.rotate.z };
    combinedTr.scale = { tr.scale.x * offset_.scale.x, tr.scale.y * offset_.scale.y, tr.scale.z * offset_.scale.z };
    bool vis = owner_->IsVisible() && isActive_;

    if (spriteObject_) {
        spriteObject_->SetTransform(combinedTr);
        spriteObject_->SetVisible(vis);
        spriteObject_->Update();
    } else if (triangleObject_) {
        triangleObject_->SetTransform(combinedTr);
        triangleObject_->SetVisible(vis);
        triangleObject_->Update();
    } else if (circleObject_) {
        circleObject_->SetTransform(combinedTr);
        circleObject_->SetVisible(vis);
        circleObject_->Update();
    } else if (ringObject_) {
        ringObject_->SetTransform(combinedTr);
        ringObject_->SetVisible(vis);
        ringObject_->Update();
    } else if (lineObject_) {
        lineObject_->SetTransform(combinedTr);
        lineObject_->SetVisible(vis);
        lineObject_->Update();
    }
}

void SpriteRendererComponent::Draw2D() {
    if (!isActive_ || !owner_ || !owner_->IsVisible()) return;
    const auto& tr = owner_->GetTransform();
    Transform combinedTr{};
    combinedTr.translate = { tr.translate.x + offset_.translate.x, tr.translate.y + offset_.translate.y, tr.translate.z + offset_.translate.z };
    combinedTr.rotate = { tr.rotate.x + offset_.rotate.x, tr.rotate.y + offset_.rotate.y, tr.rotate.z + offset_.rotate.z };
    combinedTr.scale = { tr.scale.x * offset_.scale.x, tr.scale.y * offset_.scale.y, tr.scale.z * offset_.scale.z };

    if (spriteObject_) {
        spriteObject_->SetTransform(combinedTr);
        spriteObject_->SetVisible(true);
        spriteObject_->Update();
        spriteObject_->Draw(textureKey_, true);
    } else if (triangleObject_) {
        triangleObject_->SetTransform(combinedTr);
        triangleObject_->SetVisible(true);
        triangleObject_->Update();
        triangleObject_->Draw(textureKey_, true);
    } else if (circleObject_) {
        circleObject_->SetTransform(combinedTr);
        circleObject_->SetVisible(true);
        circleObject_->Update();
        circleObject_->Draw(textureKey_, true);
    } else if (ringObject_) {
        ringObject_->SetTransform(combinedTr);
        ringObject_->SetVisible(true);
        ringObject_->Update();
        ringObject_->Draw(textureKey_, true);
    } else if (lineObject_) {
        lineObject_->SetTransform(combinedTr);
        lineObject_->SetVisible(true);
        lineObject_->Update();
        lineObject_->Draw(textureKey_, true);
    }
}

void SpriteRendererComponent::DrawInspector() {
#ifdef _USEIMGUI
    std::string idPrefix = "##SpriteRenderer_" + std::to_string(reinterpret_cast<uintptr_t>(this));

    // 0. ローカルオフセット (個別Transform)
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

    // 1. 形状切り替え Combo
    int currentShape = static_cast<int>(shapeType_);
    if (ImGui::Combo(("Shape Type" + idPrefix).c_str(), &currentShape, kShapeNames, IM_ARRAYSIZE(kShapeNames))) {
        SetShapeType(static_cast<ShapeType>(currentShape));
    }

    // 2. テクスチャキー
    char texBuf[kTextBufferSize]{};
    strncpy_s(texBuf, textureKey_.c_str(), sizeof(texBuf) - 1);
    if (ImGui::InputText("Texture Key##SpriteRenderer", texBuf, sizeof(texBuf))) {
        textureKey_ = texBuf;
    }

    // 3. 形状別パラメータ
    if (shapeType_ == ShapeType::Sprite || shapeType_ == ShapeType::Triangle) {
        float sz[2] = { size_.x, size_.y };
        if (ImGui::DragFloat2("Size##SpriteRenderer", sz, kSizeDragSpeed, kMinSize, kMaxSize, "%.1f")) {
            SetSize({ sz[0], sz[1] });
        }
    } else if (shapeType_ == ShapeType::Circle) {
        float r = radius_;
        if (ImGui::DragFloat("Radius##SpriteRenderer", &r, kRadiusDragSpeed, kMinRadius, kMaxRadius, "%.1f")) {
            SetRadius(r);
        }
    } else if (shapeType_ == ShapeType::Ring) {
        float r = radius_;
        float ir = innerRadius_;
        if (ImGui::DragFloat("Outer Radius##SpriteRenderer", &r, kRadiusDragSpeed, kMinRadius, kMaxRadius, "%.1f")) {
            SetRadius(r);
        }
        if (ImGui::DragFloat("Inner Radius##SpriteRenderer", &ir, kRadiusDragSpeed, kMinRadius, r - 0.1f, "%.1f")) {
            SetInnerRadius(ir);
        }
    } else if (shapeType_ == ShapeType::Line) {
        float start[2] = { lineStart_.x, lineStart_.y };
        float end[2] = { lineEnd_.x, lineEnd_.y };
        float th = lineThickness_;
        if (ImGui::DragFloat2("Start##SpriteRenderer", start, kLineDragSpeed)) {
            SetLineStart({ start[0], start[1] });
        }
        if (ImGui::DragFloat2("End##SpriteRenderer", end, kLineDragSpeed)) {
            SetLineEnd({ end[0], end[1] });
        }
        if (ImGui::DragFloat("Thickness##SpriteRenderer", &th, kRadiusDragSpeed, kMinThickness, kMaxThickness, "%.1f")) {
            SetLineThickness(th);
        }
    }

    // 4. カラーピッカー
    float col[4] = { color_.x, color_.y, color_.z, color_.w };
    if (ImGui::ColorEdit4("Color##SpriteRenderer", col)) {
        SetColor({ col[0], col[1], col[2], col[3] });
    }
#endif
}
