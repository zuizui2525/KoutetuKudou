#include "Engine/Graphics/Objects/3d/LightGizmo/LightGizmoObject.h"
#include "Engine/Graphics/Objects/3d/Drawer/Object3DDrawer.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Base/DeferredRelease/DeferredReleaseManager.h"
#include "Engine/Zuizui.h"
#include "Engine/Math/Matrix/Matrix.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include <cmath>
#include <vector>
#include <numbers>

namespace {
    // 中央の円（発光球）の分割数
    constexpr uint32_t kCircleSegments = 16;
    // 放射光線（レイ）の本数
    constexpr uint32_t kRayCount = 8;
    // 中心の十字のライン数 (2本)
    constexpr uint32_t kCrossLineCount = 2;

    // 頂点数: 円(16) + レイ(8 * 2 = 16) + 十字(2 * 2 = 4) = 36
    constexpr uint32_t kTotalVertices = kCircleSegments + (kRayCount * 2) + (kCrossLineCount * 2);
    // インデックス数: 円(16 * 2 = 32) + レイ(8 * 2 = 16) + 十字(2 * 2 = 4) = 52
    constexpr uint32_t kTotalIndices = (kCircleSegments * 2) + (kRayCount * 2) + (kCrossLineCount * 2);

    // ライン描画用PSOキー
    const std::string kLinePsoKey = "Object3D_Line";

    constexpr float kTwoPi = 6.283185307179586f;
}

LightGizmoObject::~LightGizmoObject() {
    auto deferredMgr = DeferredReleaseManager::GetInstance();
    if (vertexResource_) {
        deferredMgr->Enqueue(std::move(vertexResource_));
    }
    if (indexResource_) {
        deferredMgr->Enqueue(std::move(indexResource_));
    }
}

void LightGizmoObject::Initialize(int lightingMode) {
    Object3D::Initialize(lightingMode);
    CreateMesh();
}

void LightGizmoObject::Update() {
    if (needsUpdate_) {
        CreateMesh();
        needsUpdate_ = false;
    }

    // カメラのビュー行列からビルボード回転（カメラの正面を向く）を生成
    auto cameraMgr = CameraResource::GetCameraManager();
    Matrix4x4 billBoardMatrix = Math::MakeIdentity();
    if (cameraMgr) {
        billBoardMatrix = Math::Inverse(cameraMgr->GetViewMatrix3D());
        billBoardMatrix.m[3][0] = 0.0f;
        billBoardMatrix.m[3][1] = 0.0f;
        billBoardMatrix.m[3][2] = 0.0f;
    }

    // スケール -> ビルボード回転 -> 平行移動（親オブジェクトがあれば合成）
    Matrix4x4 scaleMat = Math::MakeScaleMatrix(transform_.scale);
    Matrix4x4 transMat = Math::MakeTranslateMatrix(transform_.translate);
    Matrix4x4 world = Math::Multiply(Math::Multiply(scaleMat, billBoardMatrix), transMat);

    if (parent_) {
        world = Math::Multiply(world, parent_->GetWorldMatrix());
    }
    matWorld_ = world;

    // WVP 行列の計算
    if (cameraMgr && wvpData_) {
        Matrix4x4 wvp = Math::Multiply(Math::Multiply(world, cameraMgr->GetViewMatrix3D()), cameraMgr->GetProjectionMatrix3D());
        wvpData_->WVP = wvp;
        wvpData_->world = world;
        wvpData_->WorldInverseTranspose = Math::Transpose(Math::Inverse(world));
    }
}

void LightGizmoObject::Draw(const std::string& textureKey, const std::string& envMapKey) {
    Object3DDrawer::GetInstance()->DrawIndexed(
        this, vbView_, ibView_, kTotalIndices, textureKey, envMapKey, kLinePsoKey
    );
}

void LightGizmoObject::SetRadius(float radius) {
    if (std::abs(radius_ - radius) > 0.001f) {
        radius_ = radius;
        needsUpdate_ = true;
    }
}

void LightGizmoObject::CreateMesh() {
    VertexData vertices[kTotalVertices]{};
    uint32_t indices[kTotalIndices]{};

    uint32_t vtxOffset = 0;
    uint32_t idxOffset = 0;

    // 1. 中央の円（発光球）
    const float circleRadius = radius_ * 0.40f;
    for (uint32_t i = 0; i < kCircleSegments; ++i) {
        float angle = (static_cast<float>(i) / static_cast<float>(kCircleSegments)) * kTwoPi;
        float x = std::cos(angle) * circleRadius;
        float y = std::sin(angle) * circleRadius;
        vertices[vtxOffset + i].position = { x, y, 0.0f, 1.0f };
        vertices[vtxOffset + i].texcoord = { 0.0f, 0.0f };
        vertices[vtxOffset + i].normal = { 0.0f, 0.0f, 1.0f };

        uint32_t nextI = (i + 1) % kCircleSegments;
        indices[idxOffset++] = vtxOffset + i;
        indices[idxOffset++] = vtxOffset + nextI;
    }
    vtxOffset += kCircleSegments;

    // 2. 周囲の放射光線（8方向のレイ）
    const float rayInnerRadius = radius_ * 0.55f;
    const float rayOuterRadius = radius_ * 0.85f;
    for (uint32_t i = 0; i < kRayCount; ++i) {
        float angle = (static_cast<float>(i) / static_cast<float>(kRayCount)) * kTwoPi;
        float cosA = std::cos(angle);
        float sinA = std::sin(angle);

        // 始点
        vertices[vtxOffset].position = { cosA * rayInnerRadius, sinA * rayInnerRadius, 0.0f, 1.0f };
        vertices[vtxOffset].texcoord = { 0.0f, 0.0f };
        vertices[vtxOffset].normal = { 0.0f, 0.0f, 1.0f };

        // 終点
        vertices[vtxOffset + 1].position = { cosA * rayOuterRadius, sinA * rayOuterRadius, 0.0f, 1.0f };
        vertices[vtxOffset + 1].texcoord = { 0.0f, 0.0f };
        vertices[vtxOffset + 1].normal = { 0.0f, 0.0f, 1.0f };

        indices[idxOffset++] = vtxOffset;
        indices[idxOffset++] = vtxOffset + 1;
        vtxOffset += 2;
    }

    // 3. 中心の小さな十字（+印）
    const float crossRadius = radius_ * 0.15f;
    // 水平線
    vertices[vtxOffset].position = { -crossRadius, 0.0f, 0.0f, 1.0f };
    vertices[vtxOffset].normal = { 0.0f, 0.0f, 1.0f };
    vertices[vtxOffset + 1].position = { crossRadius, 0.0f, 0.0f, 1.0f };
    vertices[vtxOffset + 1].normal = { 0.0f, 0.0f, 1.0f };
    indices[idxOffset++] = vtxOffset;
    indices[idxOffset++] = vtxOffset + 1;
    vtxOffset += 2;

    // 垂直線
    vertices[vtxOffset].position = { 0.0f, -crossRadius, 0.0f, 1.0f };
    vertices[vtxOffset].normal = { 0.0f, 0.0f, 1.0f };
    vertices[vtxOffset + 1].position = { 0.0f, crossRadius, 0.0f, 1.0f };
    vertices[vtxOffset + 1].normal = { 0.0f, 0.0f, 1.0f };
    indices[idxOffset++] = vtxOffset;
    indices[idxOffset++] = vtxOffset + 1;
    vtxOffset += 2;

    // Vertex Resource 作成
    vertexResource_ = DxUtils::CreateBufferResource(sEngine->GetDevice(), sizeof(VertexData) * kTotalVertices);
    vbView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vbView_.SizeInBytes = sizeof(VertexData) * kTotalVertices;
    vbView_.StrideInBytes = sizeof(VertexData);

    VertexData* vertData = nullptr;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertData));
    std::copy(std::begin(vertices), std::end(vertices), vertData);
    vertexResource_->Unmap(0, nullptr);

    // Index Resource 作成
    indexResource_ = DxUtils::CreateBufferResource(sEngine->GetDevice(), sizeof(uint32_t) * kTotalIndices);
    ibView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    ibView_.SizeInBytes = sizeof(uint32_t) * kTotalIndices;
    ibView_.Format = DXGI_FORMAT_R32_UINT;

    uint32_t* idxData = nullptr;
    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&idxData));
    std::copy(std::begin(indices), std::end(indices), idxData);
    indexResource_->Unmap(0, nullptr);
}
