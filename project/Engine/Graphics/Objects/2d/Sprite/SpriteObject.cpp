#include "Engine/Graphics/Objects/2d/Sprite/SpriteObject.h"
#include "Engine/Graphics/Objects/2d/Drawer/Object2DDrawer.h"
#include "Engine/Zuizui.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Base/DeferredRelease/DeferredReleaseManager.h"
#include <imgui.h>

SpriteObject::~SpriteObject() {
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

void SpriteObject::SetSize(float width, float height) {
    width_ = width;
    height_ = height;
    UpdateVertexData(); // サイズが変わったら頂点を再計算
}

void SpriteObject::UpdateVertexData() {
    if (vertexData_ == nullptr) return;

    // 座標の設定 (左上原点の場合)
    vertexData_[0] = { {0.0f, height_, 0.0f, 1.0f}, {0,1}, {0,0,-1} }; // 左下
    vertexData_[1] = { {0.0f, 0.0f, 0.0f, 1.0f}, {0,0}, {0,0,-1} };    // 左上
    vertexData_[2] = { {width_, height_, 0.0f, 1.0f}, {1,1}, {0,0,-1} }; // 右下
    vertexData_[3] = { {width_, 0.0f, 0.0f, 1.0f}, {1,0}, {0,0,-1} };    // 右上
}

void SpriteObject::Initialize(int lightingMode) {
    // Material
    materialResource_ = DxUtils::CreateBufferResource(sEngine->GetDevice(), sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
    materialData_->color = { 1,1,1,1 };
    materialData_->enableLighting = 0;
    materialData_->uvtransform = Math::MakeIdentity();
    materialData_->shininess = 30.0f;

    // WVP
    wvpResource_ = DxUtils::CreateBufferResource(sEngine->GetDevice(), sizeof(TransformationMatrix));
    wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpData_));
    wvpData_->WVP = Math::MakeIdentity();
    wvpData_->world = Math::MakeIdentity();

    // Vertex
    vertexResource_ = DxUtils::CreateBufferResource(sEngine->GetDevice(), sizeof(VertexData) * 4);
    vbView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vbView_.SizeInBytes = sizeof(VertexData) * 4;
    vbView_.StrideInBytes = sizeof(VertexData);

    // Mapしてポインタを保存しておく
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));

    // 初期サイズで頂点を設定
    UpdateVertexData();

    // Index
    indexResource_ = DxUtils::CreateBufferResource(sEngine->GetDevice(), sizeof(uint32_t) * 6);
    ibView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    ibView_.SizeInBytes = sizeof(uint32_t) * 6;
    ibView_.Format = DXGI_FORMAT_R32_UINT;
    uint32_t* idx;
    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&idx));
    idx[0] = 0; idx[1] = 1; idx[2] = 2; idx[3] = 1; idx[4] = 3; idx[5] = 2;

    // ヒエラルキー自動登録
    InitializeGameObject("Sprite");
}

void SpriteObject::Update() {
    Matrix4x4 world = Math::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    Matrix4x4 wvp = Math::Multiply(Math::Multiply(world, CameraResource::GetCameraManager()->GetViewMatrix2D()), CameraResource::GetCameraManager()->GetProjectionMatrix2D());
    wvpData_->WVP = wvp;
    wvpData_->world = world;

    Matrix4x4 uv = Math::MakeScaleMatrix(uvTransform_.scale);
    uv = Math::Multiply(uv, Math::MakeRotateZMatrix(uvTransform_.rotate.z));
    uv = Math::Multiply(uv, Math::MakeTranslateMatrix(uvTransform_.translate));
    materialData_->uvtransform = uv;
}

void SpriteObject::Draw(const std::string& textureKey, bool draw) {
    if (!draw) return;

    // 描画処理は共通描画クラス Object2DDrawer を通して実行
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
void SpriteObject::DrawInspector() {
#ifdef _USEIMGUI
    std::string label = "##" + name_;

    // --- Sprite特有の設定 (サイズ) ---
    if (ImGui::CollapsingHeader(("Sprite Settings" + label).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        float size[2] = { width_, height_ };
        if (ImGui::DragFloat2(("Size" + label).c_str(), size, 1.0f, 0.0f, 0.0f, "%.1f")) {
            SetSize(size[0], size[1]);
        }
    }

    // --- 共通のSRT設定 ---
    if (ImGui::CollapsingHeader(("Transform" + label).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3(("Scale" + label).c_str(), &transform_.scale.x, 0.01f, 0.0f, 0.0f, "%.1f");
        ImGui::DragFloat3(("Rotate" + label).c_str(), &transform_.rotate.x, 0.01f, 0.0f, 0.0f, "%.1f");
        ImGui::DragFloat3(("Translate" + label).c_str(), &transform_.translate.x, 1.0f, 0.0f, 0.0f, "%.1f");
    }

    // --- カラー設定 ---
    if (ImGui::CollapsingHeader(("Color" + label).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit4(("Color" + label).c_str(), &materialData_->color.x, ImGuiColorEditFlags_AlphaBar);
    }

    // --- UV設定 (Sprite特有) ---
    if (ImGui::CollapsingHeader(("UV Transform" + label).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat2(("uvScale" + label).c_str(), &uvTransform_.scale.x, 0.01f, 0.0f, 0.0f, "%.1f");
        ImGui::DragFloat(("uvRotate" + label).c_str(), &uvTransform_.rotate.z, 0.01f, 0.0f, 0.0f, "%.1f");
        ImGui::DragFloat2(("uvTranslate" + label).c_str(), &uvTransform_.translate.x, 0.01f, 0.0f, 0.0f, "%.1f");
    }
#endif
}
