#include "Engine/Graphics/Objects/2d/Circle/Circle2DObject.h"
#include "Engine/Graphics/Objects/2d/Drawer/Object2DDrawer.h"
#include "Engine/Zuizui.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Base/Utils/DxUtils.h"
#include <imgui.h>

namespace {
    constexpr float kHalf = 0.5f;
    constexpr float kTwoPi = 2.0f * static_cast<float>(M_PI);
    constexpr float kDefaultShininess = 30.0f;
}

void Circle2DObject::SetRadius(float radius) {
    radius_ = radius;
    UpdateVertexData();
}

void Circle2DObject::UpdateVertexData() {
    if (vertexData_ == nullptr) return;

    // 中心頂点 (インデックス 0)
    vertexData_[0] = {
        { radius_, radius_, 0.0f, 1.0f },
        { kHalf, kHalf },
        { 0.0f, 0.0f, kNormalZ }
    };

    // 外周頂点 (インデックス 1 ~ kSubdivision)
    for (uint32_t i = 0; i < kSubdivision; ++i) {
        float angle = static_cast<float>(i) / static_cast<float>(kSubdivision) * kTwoPi;
        float cosA = std::cos(angle);
        float sinA = std::sin(angle);

        float x = radius_ + radius_ * cosA;
        float y = radius_ + radius_ * sinA;
        float u = kHalf + kHalf * cosA;
        float v = kHalf + kHalf * sinA;

        vertexData_[i + 1] = {
            { x, y, 0.0f, 1.0f },
            { u, v },
            { 0.0f, 0.0f, kNormalZ }
        };
    }
}

void Circle2DObject::Initialize(int lightingMode) {
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

    // Index
    indexResource_ = DxUtils::CreateBufferResource(engine->GetDevice(), sizeof(uint32_t) * kIndexCount);
    ibView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    ibView_.SizeInBytes = sizeof(uint32_t) * kIndexCount;
    ibView_.Format = DXGI_FORMAT_R32_UINT;
    uint32_t* idx = nullptr;
    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&idx));

    // 扇形三角形のインデックス構築
    for (uint32_t i = 0; i < kSubdivision; ++i) {
        uint32_t next = (i + 1) % kSubdivision;
        idx[i * 3 + 0] = 0;             // 中心
        idx[i * 3 + 1] = 1 + next;      // 次の外周点
        idx[i * 3 + 2] = 1 + i;         // 現在の外周点
    }

    // ヒエラルキー自動登録
    InitializeGameObject("Circle2D");
}

void Circle2DObject::Update() {
    Matrix4x4 world = Math::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    Matrix4x4 wvp = Math::Multiply(
        Math::Multiply(world, CameraResource::GetCameraManager()->GetViewMatrix2D()),
        CameraResource::GetCameraManager()->GetProjectionMatrix2D()
    );
    wvpData_->WVP = wvp;
    wvpData_->world = world;

    Matrix4x4 uv = Math::MakeScaleMatrix(uvTransform_.scale);
    uv = Math::Multiply(uv, Math::MakeRotateZMatrix(uvTransform_.rotate.z));
    uv = Math::Multiply(uv, Math::MakeTranslateMatrix(uvTransform_.translate));
    materialData_->uvtransform = uv;
}

void Circle2DObject::Draw(const std::string& textureKey, bool draw) {
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

void Circle2DObject::DrawInspector() {
#ifdef _USEIMGUI
    std::string label = "##" + name_;

    if (ImGui::CollapsingHeader(("Circle2D Settings" + label).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        float r = radius_;
        if (ImGui::DragFloat(("Radius" + label).c_str(), &r, 1.0f, 1.0f, 1000.0f, "%.1f")) {
            SetRadius(r);
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

    if (ImGui::CollapsingHeader(("UV Transform" + label).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat2(("uvScale" + label).c_str(), &uvTransform_.scale.x, 0.01f, 0.0f, 0.0f, "%.1f");
        ImGui::DragFloat(("uvRotate" + label).c_str(), &uvTransform_.rotate.z, 0.01f, 0.0f, 0.0f, "%.1f");
        ImGui::DragFloat2(("uvTranslate" + label).c_str(), &uvTransform_.translate.x, 0.01f, 0.0f, 0.0f, "%.1f");
    }
#endif
}
