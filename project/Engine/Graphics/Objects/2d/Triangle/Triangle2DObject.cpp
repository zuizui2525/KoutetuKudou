#include "Engine/Graphics/Objects/2d/Triangle/Triangle2DObject.h"
#include "Engine/Graphics/Objects/2d/Drawer/Object2DDrawer.h"
#include "Engine/Zuizui.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Base/DeferredRelease/DeferredReleaseManager.h"
#include <imgui.h>

Triangle2DObject::~Triangle2DObject() {
    auto deferredMgr = DeferredReleaseManager::GetInstance();
    if (materialResource_) {
        deferredMgr->Enqueue(std::move(materialResource_));
    }
    if (wvpResource_) {
        deferredMgr->Enqueue(std::move(wvpResource_));
    }
    if (vertexResource_) {
        deferredMgr->Enqueue(std::move(vertexResource_));
    }
    if (indexResource_) {
        deferredMgr->Enqueue(std::move(indexResource_));
    }
}

namespace {
    constexpr float kHalfScale = 0.5f;
    constexpr float kDefaultShininess = 30.0f;
}

void Triangle2DObject::SetSize(float width, float height) {
    width_ = width;
    height_ = height;
    UpdateVertexData();
}

void Triangle2DObject::UpdateVertexData() {
    if (vertexData_ == nullptr) return;

    // 上向き二等辺三角形の座標設定
    // 頂点0: 上部中央, 頂点1: 右下, 頂点2: 左下
    vertexData_[0] = { { width_ * kHalfScale, 0.0f, 0.0f, 1.0f }, { kHalfScale, 0.0f }, { 0.0f, 0.0f, kNormalZ } };
    vertexData_[1] = { { width_, height_, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, kNormalZ } };
    vertexData_[2] = { { 0.0f, height_, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, kNormalZ } };
}

void Triangle2DObject::Initialize(int lightingMode) {
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
    idx[0] = 0;
    idx[1] = 1;
    idx[2] = 2;

    // ヒエラルキー自動登録
    InitializeGameObject("Triangle2D");
}

void Triangle2DObject::Update() {
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

void Triangle2DObject::Draw(const std::string& textureKey, bool draw) {
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

void Triangle2DObject::DrawInspector() {
#ifdef _USEIMGUI
    std::string label = "##" + name_;

    if (ImGui::CollapsingHeader(("Triangle2D Settings" + label).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        float size[2] = { width_, height_ };
        if (ImGui::DragFloat2(("Size" + label).c_str(), size, 1.0f, 0.0f, 0.0f, "%.1f")) {
            SetSize(size[0], size[1]);
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
