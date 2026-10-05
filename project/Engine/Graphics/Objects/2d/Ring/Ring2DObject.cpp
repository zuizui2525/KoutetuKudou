#include "Engine/Graphics/Objects/2d/Ring/Ring2DObject.h"
#include "Engine/Graphics/Objects/2d/Drawer/Object2DDrawer.h"
#include "Engine/Zuizui.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Base/DeferredRelease/DeferredReleaseManager.h"
#include <algorithm>
#include <imgui.h>

Ring2DObject::~Ring2DObject() {
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
    constexpr float kHalf = 0.5f;
    constexpr float kTwoPi = 2.0f * static_cast<float>(M_PI);
    constexpr float kDefaultShininess = 30.0f;
    constexpr uint32_t kIndicesPerQuad = 6;
}

void Ring2DObject::SetRadii(float outerRadius, float innerRadius) {
    outerRadius_ = outerRadius;
    innerRadius_ = (std::min)(innerRadius, outerRadius_ - 1.0f);
    UpdateVertexData();
}

void Ring2DObject::UpdateVertexData() {
    if (vertexData_ == nullptr) return;

    for (uint32_t i = 0; i < kSubdivision; ++i) {
        float angle = static_cast<float>(i) / static_cast<float>(kSubdivision) * kTwoPi;
        float cosA = std::cos(angle);
        float sinA = std::sin(angle);

        // 外周頂点 (インデックス 0 ~ kSubdivision - 1)
        float outerX = outerRadius_ + outerRadius_ * cosA;
        float outerY = outerRadius_ + outerRadius_ * sinA;
        float outerU = kHalf + kHalf * cosA;
        float outerV = kHalf + kHalf * sinA;

        vertexData_[i] = {
            { outerX, outerY, 0.0f, 1.0f },
            { outerU, outerV },
            { 0.0f, 0.0f, kNormalZ }
        };

        // 内周頂点 (インデックス kSubdivision ~ 2 * kSubdivision - 1)
        float innerX = outerRadius_ + innerRadius_ * cosA;
        float innerY = outerRadius_ + innerRadius_ * sinA;
        float innerFactor = (outerRadius_ > 0.0f) ? (innerRadius_ / outerRadius_ * kHalf) : 0.0f;
        float innerU = kHalf + innerFactor * cosA;
        float innerV = kHalf + innerFactor * sinA;

        vertexData_[i + kSubdivision] = {
            { innerX, innerY, 0.0f, 1.0f },
            { innerU, innerV },
            { 0.0f, 0.0f, kNormalZ }
        };
    }
}

void Ring2DObject::Initialize(int lightingMode) {
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

    // クワッド帯メッシュのインデックス構築
    for (uint32_t i = 0; i < kSubdivision; ++i) {
        uint32_t next = (i + 1) % kSubdivision;
        uint32_t outerCur = i;
        uint32_t outerNext = next;
        uint32_t innerCur = i + kSubdivision;
        uint32_t innerNext = next + kSubdivision;

        uint32_t base = i * kIndicesPerQuad;
        idx[base + 0] = outerCur;
        idx[base + 1] = outerNext;
        idx[base + 2] = innerCur;

        idx[base + 3] = outerNext;
        idx[base + 4] = innerNext;
        idx[base + 5] = innerCur;
    }

    // ヒエラルキー自動登録
    InitializeGameObject("Ring2D");
}

void Ring2DObject::Update() {
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

void Ring2DObject::Draw(const std::string& textureKey, bool draw) {
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

void Ring2DObject::DrawInspector() {
#ifdef _USEIMGUI
    std::string label = "##" + name_;

    if (ImGui::CollapsingHeader(("Ring2D Settings" + label).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        float radii[2] = { outerRadius_, innerRadius_ };
        if (ImGui::DragFloat2(("Outer / Inner" + label).c_str(), radii, 1.0f, 1.0f, 1000.0f, "%.1f")) {
            SetRadii(radii[0], radii[1]);
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
