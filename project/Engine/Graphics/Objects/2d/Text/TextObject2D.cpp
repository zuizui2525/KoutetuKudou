#include "Engine/Graphics/Objects/2d/Text/TextObject2D.h"
#include "Engine/Graphics/Objects/2d/Drawer/Object2DDrawer.h"
#include "Engine/Zuizui.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Base/DeferredRelease/DeferredReleaseManager.h"

#ifdef _USEIMGUI
#include <imgui.h>
#endif

namespace {
    // 頂点法線定数
    constexpr float kNormalZ = -1.0f;
}

TextObject2D::TextObject2D() = default;

TextObject2D::~TextObject2D() {
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

void TextObject2D::SetSize(float width, float height) {
    width_ = width;
    height_ = height;
    UpdateVertexData();
}

void TextObject2D::SetColor(const Vector4& color) {
    if (materialData_) {
        materialData_->color = color;
    }
}

void TextObject2D::UpdateVertexData() {
    if (vertexData_ == nullptr) return;

    // 左上原点で四角形頂点を設定
    vertexData_[0] = { {0.0f, height_, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, kNormalZ} }; // 左下
    vertexData_[1] = { {0.0f, 0.0f, 0.0f, 1.0f},    {0.0f, 0.0f}, {0.0f, 0.0f, kNormalZ} }; // 左上
    vertexData_[2] = { {width_, height_, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, kNormalZ} }; // 右下
    vertexData_[3] = { {width_, 0.0f, 0.0f, 1.0f},    {1.0f, 0.0f}, {0.0f, 0.0f, kNormalZ} }; // 右上
}

void TextObject2D::Initialize() {
    auto engine = EngineResource::GetEngine();
    assert(engine != nullptr);
    auto device = engine->GetDevice();

    // Material
    materialResource_ = DxUtils::CreateBufferResource(device, sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
    materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData_->enableLighting = 0;
    materialData_->uvtransform = Math::MakeIdentity();
    materialData_->shininess = kDefaultShininess;

    // WVP
    wvpResource_ = DxUtils::CreateBufferResource(device, sizeof(TransformationMatrix));
    wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpData_));
    wvpData_->WVP = Math::MakeIdentity();
    wvpData_->world = Math::MakeIdentity();

    // Vertex Buffer
    vertexResource_ = DxUtils::CreateBufferResource(device, sizeof(VertexData) * kVertexCount);
    vbView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vbView_.SizeInBytes = sizeof(VertexData) * kVertexCount;
    vbView_.StrideInBytes = sizeof(VertexData);
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));

    UpdateVertexData();

    // Index Buffer
    indexResource_ = DxUtils::CreateBufferResource(device, sizeof(uint32_t) * kIndexCount);
    ibView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    ibView_.SizeInBytes = sizeof(uint32_t) * kIndexCount;
    ibView_.Format = DXGI_FORMAT_R32_UINT;

    uint32_t* idx = nullptr;
    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&idx));
    idx[0] = 0; idx[1] = 1; idx[2] = 2;
    idx[3] = 1; idx[4] = 3; idx[5] = 2;

    InitializeGameObject("Text2D");
}

void TextObject2D::Update() {
    Matrix4x4 world = Math::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    auto cameraMgr = CameraResource::GetCameraManager();
    if (cameraMgr) {
        Matrix4x4 wvp = Math::Multiply(
            Math::Multiply(world, cameraMgr->GetViewMatrix2D()),
            cameraMgr->GetProjectionMatrix2D()
        );
        wvpData_->WVP = wvp;
        wvpData_->world = world;
    }

    Matrix4x4 uv = Math::MakeScaleMatrix(uvTransform_.scale);
    uv = Math::Multiply(uv, Math::MakeRotateZMatrix(uvTransform_.rotate.z));
    uv = Math::Multiply(uv, Math::MakeTranslateMatrix(uvTransform_.translate));
    materialData_->uvtransform = uv;
}

void TextObject2D::Draw(D3D12_GPU_DESCRIPTOR_HANDLE textureHandle, bool draw) {
    if (!draw || textureHandle.ptr == 0) return;

    Object2DDrawer::GetInstance()->DrawIndexedHandle(
        wvpResource_.Get(),
        materialResource_.Get(),
        vbView_,
        ibView_,
        kIndexCount,
        textureHandle,
        isVisible_
    );
}

void TextObject2D::DrawInspector() {
#ifdef _USEIMGUI
    std::string label = "##" + name_;
    if (ImGui::CollapsingHeader(("Text2D Settings" + label).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        float size[2] = { width_, height_ };
        if (ImGui::DragFloat2(("Size" + label).c_str(), size, 1.0f, 0.0f, 0.0f, "%.1f")) {
            SetSize(size[0], size[1]);
        }
    }
    if (ImGui::CollapsingHeader(("Transform" + label).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3(("Scale" + label).c_str(), &transform_.scale.x, 0.01f, 0.0f, 0.0f, "%.2f");
        ImGui::DragFloat3(("Rotate" + label).c_str(), &transform_.rotate.x, 0.01f, 0.0f, 0.0f, "%.2f");
        ImGui::DragFloat3(("Translate" + label).c_str(), &transform_.translate.x, 1.0f, 0.0f, 0.0f, "%.1f");
    }
    if (ImGui::CollapsingHeader(("Color" + label).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit4(("Color" + label).c_str(), &materialData_->color.x, ImGuiColorEditFlags_AlphaBar);
    }
#endif
}
