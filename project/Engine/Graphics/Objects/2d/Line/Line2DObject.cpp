#include "Engine/Graphics/Objects/2d/Line/Line2DObject.h"
#include "Engine/Graphics/Objects/2d/Drawer/Object2DDrawer.h"
#include "Engine/Zuizui.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Base/Utils/DxUtils.h"
#include <algorithm>
#include <imgui.h>

namespace {
    constexpr float kHalf = 0.5f;
    constexpr float kEpsilon = 0.0001f;
    constexpr float kDefaultShininess = 30.0f;
}

void Line2DObject::SetPoints(const Vector2& start, const Vector2& end) {
    start_ = start;
    end_ = end;
    UpdateVertexData();
}

void Line2DObject::SetThickness(float thickness) {
    thickness_ = (std::max)(thickness, 0.1f);
    UpdateVertexData();
}

void Line2DObject::UpdateVertexData() {
    if (vertexData_ == nullptr) return;

    Vector2 dir = { end_.x - start_.x, end_.y - start_.y };
    float length = std::sqrt(dir.x * dir.x + dir.y * dir.y);

    Vector2 normal = { 0.0f, 1.0f };
    if (length > kEpsilon) {
        normal = { -dir.y / length, dir.x / length };
    }

    float halfThick = thickness_ * kHalf;
    Vector2 offset = { normal.x * halfThick, normal.y * halfThick };

    // 4頂点による帯状クワッド
    vertexData_[0] = { { start_.x - offset.x, start_.y - offset.y, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, kNormalZ } };
    vertexData_[1] = { { start_.x + offset.x, start_.y + offset.y, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, kNormalZ } };
    vertexData_[2] = { { end_.x - offset.x,   end_.y - offset.y,   0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, kNormalZ } };
    vertexData_[3] = { { end_.x + offset.x,   end_.y + offset.y,   0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, kNormalZ } };
}

void Line2DObject::Initialize(int lightingMode) {
    Zuizui* engine = Zuizui::GetInstance();

    // Material
    materialResource_ = DxUtils::CreateBufferResource(engine->GetDevice(), sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
    materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData_->enableLighting = lightingMode;
    materialData_->uvtransform = Math::MakeIdentity();
    materialData_->shininess = kDefaultShininess;

    // WVP
    wvpResource_ = DxUtils::CreateBufferResource(engine->GetDevice(), sizeof(TransformationMatrix));
    wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpData_));
    wvpData_->WVP = Math::MakeIdentity();
    wvpData_->world = Math::MakeIdentity();

    // Vertex
    vertexResource_ = DxUtils::CreateBufferResource(engine->GetDevice(), sizeof(VertexData) * kVertexCount);
    vbView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vbView_.SizeInBytes = sizeof(VertexData) * kVertexCount;
    vbView_.StrideInBytes = sizeof(VertexData);
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));

    UpdateVertexData();

    // Index (クワッド2三角形: 0, 1, 2, 1, 3, 2)
    indexResource_ = DxUtils::CreateBufferResource(engine->GetDevice(), sizeof(uint32_t) * kIndexCount);
    ibView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    ibView_.SizeInBytes = sizeof(uint32_t) * kIndexCount;
    ibView_.Format = DXGI_FORMAT_R32_UINT;
    uint32_t* idx = nullptr;
    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&idx));
    idx[0] = 0;
    idx[1] = 1;
    idx[2] = 2;
    idx[3] = 1;
    idx[4] = 3;
    idx[5] = 2;

    // ヒエラルキー自動登録
    InitializeGameObject("Line2D");
}

void Line2DObject::Update() {
    Matrix4x4 world = Math::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    Matrix4x4 wvp = Math::Multiply(
        Math::Multiply(world, CameraResource::GetCameraManager()->GetViewMatrix2D()),
        CameraResource::GetCameraManager()->GetProjectionMatrix2D()
    );
    wvpData_->WVP = wvp;
    wvpData_->world = world;
}

void Line2DObject::Draw(const std::string& textureKey, bool draw) {
    if (!draw) return;

    Object2DDrawer::GetInstance()->DrawIndexed(
        wvpResource_.Get(),
        materialResource_.Get(),
        vbView_,
        ibView_,
        kIndexCount,
        textureKey,
        isVisible_
    );
}

void Line2DObject::DrawInspector() {
#ifdef _USEIMGUI
    std::string label = "##" + name_;

    if (ImGui::CollapsingHeader(("Line2D Settings" + label).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        float start[2] = { start_.x, start_.y };
        if (ImGui::DragFloat2(("Start Point" + label).c_str(), start, 1.0f, 0.0f, 0.0f, "%.1f")) {
            SetPoints({ start[0], start[1] }, end_);
        }
        float end[2] = { end_.x, end_.y };
        if (ImGui::DragFloat2(("End Point" + label).c_str(), end, 1.0f, 0.0f, 0.0f, "%.1f")) {
            SetPoints(start_, { end[0], end[1] });
        }
        float thick = thickness_;
        if (ImGui::DragFloat(("Thickness" + label).c_str(), &thick, 0.5f, 0.5f, 100.0f, "%.1f")) {
            SetThickness(thick);
        }
    }

    if (ImGui::CollapsingHeader(("Transform" + label).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3(("Scale" + label).c_str(), &transform_.scale.x, 0.01f, 0.0f, 0.0f, "%.1f");
        ImGui::DragFloat3(("Rotate" + label).c_str(), &transform_.rotate.x, 0.01f, 0.0f, 0.0f, "%.1f");
        ImGui::DragFloat3(("Translate" + label).c_str(), &transform_.translate.x, 1.0f, 0.0f, 0.0f, "%.1f");
    }

    if (ImGui::CollapsingHeader(("Color" + label).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit4(("Color" + label).c_str(), &materialData_->color.x, ImGuiColorEditFlags_AlphaBar);
    }
#endif
}
