#include "Engine/Graphics/Objects/3d/Frustum/CameraFrustumObject.h"
#include "Engine/Graphics/Objects/3d/Drawer/Object3DDrawer.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Base/DeferredRelease/DeferredReleaseManager.h"
#include "Engine/Zuizui.h"
#include <cmath>
#include <algorithm>

namespace {
    // 頂点数およびインデックス数 (四角錐台の外枠: 8頂点, 12エッジ = 24インデックス)
    constexpr uint32_t kVertexCount = 8;
    constexpr uint32_t kIndexCount = 24;

    // ライン描画用PSOキー
    const std::string kLinePsoKey = "Object3D_Line";
}

CameraFrustumObject::~CameraFrustumObject() {
    auto deferredMgr = DeferredReleaseManager::GetInstance();
    if (vertexResource_) {
        deferredMgr->Enqueue(std::move(vertexResource_));
    }
    if (indexResource_) {
        deferredMgr->Enqueue(std::move(indexResource_));
    }
}

void CameraFrustumObject::Initialize(int lightingMode) {
    Object3D::Initialize(lightingMode);
    CreateMesh();
}

void CameraFrustumObject::Update() {
    if (needsUpdate_) {
        CreateMesh();
        needsUpdate_ = false;
    }
    Object3D::Update();
}

void CameraFrustumObject::Draw(const std::string& textureKey, const std::string& envMapKey) {
    Object3DDrawer::GetInstance()->DrawIndexed(
        this, vbView_, ibView_, kIndexCount, textureKey, envMapKey, kLinePsoKey
    );
}

void CameraFrustumObject::SetParameters(float fov, float aspectRatio, float nearZ, float farZ) {
    if (std::abs(fov_ - fov) > 0.001f ||
        std::abs(aspectRatio_ - aspectRatio) > 0.001f ||
        std::abs(nearZ_ - nearZ) > 0.001f ||
        std::abs(farZ_ - farZ) > 0.001f) {
        fov_ = fov;
        aspectRatio_ = aspectRatio;
        nearZ_ = nearZ;
        farZ_ = farZ;
        needsUpdate_ = true;
    }
}

void CameraFrustumObject::CreateMesh() {
    float tanHalfFov = std::tan(fov_ * 0.5f);

    float nearH = nearZ_ * tanHalfFov;
    float nearW = nearH * aspectRatio_;

    float farH = farZ_ * tanHalfFov;
    float farW = farH * aspectRatio_;

    // 8頂点の座標算出 (Z正方向がカメラの前方)
    VertexData vertices[kVertexCount] = {
        // Near 平面 (0: TopLeft, 1: TopRight, 2: BottomRight, 3: BottomLeft)
        { { -nearW,  nearH, nearZ_, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
        { {  nearW,  nearH, nearZ_, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
        { {  nearW, -nearH, nearZ_, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
        { { -nearW, -nearH, nearZ_, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },

        // Far 平面 (4: TopLeft, 5: TopRight, 6: BottomRight, 7: BottomLeft)
        { { -farW,   farH,  farZ_,  1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f,  1.0f } },
        { {  farW,   farH,  farZ_,  1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f,  1.0f } },
        { {  farW,  -farH,  farZ_,  1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f,  1.0f } },
        { { -farW,  -farH,  farZ_,  1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f,  1.0f } },
    };

    // 12本のエッジ（枠線）のインデックス（対角線なし）
    uint32_t indices[kIndexCount] = {
        // Near 4辺
        0, 1,  1, 2,  2, 3,  3, 0,
        // Far 4辺
        4, 5,  5, 6,  6, 7,  7, 4,
        // 四隅を結ぶ稜線 4辺
        0, 4,  1, 5,  2, 6,  3, 7
    };

    // Vertex Resource 作成
    vertexResource_ = DxUtils::CreateBufferResource(sEngine->GetDevice(), sizeof(VertexData) * kVertexCount);
    vbView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vbView_.SizeInBytes = sizeof(VertexData) * kVertexCount;
    vbView_.StrideInBytes = sizeof(VertexData);

    VertexData* vertData = nullptr;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertData));
    std::copy(std::begin(vertices), std::end(vertices), vertData);
    vertexResource_->Unmap(0, nullptr);

    // Index Resource 作成
    indexResource_ = DxUtils::CreateBufferResource(sEngine->GetDevice(), sizeof(uint32_t) * kIndexCount);
    ibView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    ibView_.SizeInBytes = sizeof(uint32_t) * kIndexCount;
    ibView_.Format = DXGI_FORMAT_R32_UINT;

    uint32_t* idxData = nullptr;
    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&idxData));
    std::copy(std::begin(indices), std::end(indices), idxData);
    indexResource_->Unmap(0, nullptr);
}
