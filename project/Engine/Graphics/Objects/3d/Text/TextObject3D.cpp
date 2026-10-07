#include "Engine/Graphics/Objects/3d/Text/TextObject3D.h"
#include "Engine/Graphics/Objects/3d/Drawer/Object3DDrawer.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Zuizui.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Camera/Base/BaseCamera.h"
#include "Engine/Base/DeferredRelease/DeferredReleaseManager.h"
#include "Engine/Math/Matrix/Matrix.h"

namespace {
    // 描画・頂点定数 (マジックナンバー排除)
    constexpr float kHalfFactor = 0.5f;
    constexpr float kDepthZero = 0.0f;
    constexpr float kWComponent = 1.0f;
    constexpr float kUVLeft = 0.0f;
    constexpr float kUVRight = 1.0f;
    constexpr float kUVTop = 0.0f;
    constexpr float kUVBottom = 1.0f;
    constexpr Vector3 kNormalFront = { 0.0f, 0.0f, -1.0f };
}

TextObject3D::TextObject3D() = default;

TextObject3D::~TextObject3D() {
    auto deferredMgr = DeferredReleaseManager::GetInstance();
    if (vertexResource_) {
        deferredMgr->Enqueue(std::move(vertexResource_));
    }
    if (indexResource_) {
        deferredMgr->Enqueue(std::move(indexResource_));
    }
}

void TextObject3D::SetSize(const Vector2& size) {
    if (size_.x != size.x || size_.y != size.y) {
        size_ = size;
        needsMeshUpdate_ = true;
    }
}

void TextObject3D::Initialize(int lightingMode) {
    Object3D::Initialize(lightingMode);
    CreateMesh();
    InitializeGameObject("Text3D");
}

void TextObject3D::CreateMesh() {
    auto engine = EngineResource::GetEngine();
    assert(engine != nullptr);
    auto device = engine->GetDevice();

    // 以前のリソースがあれば安全に遅延解放キューへ送る
    if (vertexResource_) {
        DeferredReleaseManager::GetInstance()->Enqueue(std::move(vertexResource_));
    }
    if (indexResource_) {
        DeferredReleaseManager::GetInstance()->Enqueue(std::move(indexResource_));
    }

    // 頂点バッファ生成
    vertexResource_ = DxUtils::CreateBufferResource(device, sizeof(VertexData) * kVertexCount);
    vbView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vbView_.SizeInBytes = sizeof(VertexData) * kVertexCount;
    vbView_.StrideInBytes = sizeof(VertexData);

    VertexData* vtx = nullptr;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vtx));

    const float halfWidth = size_.x * kHalfFactor;
    const float halfHeight = size_.y * kHalfFactor;

    const float left = -halfWidth;
    const float right = halfWidth;
    const float bottom = -halfHeight;
    const float top = halfHeight;

    // 左下
    vtx[0].position = { left, bottom, kDepthZero, kWComponent };
    vtx[0].texcoord = { kUVLeft, kUVBottom };
    vtx[0].normal = kNormalFront;

    // 左上
    vtx[1].position = { left, top, kDepthZero, kWComponent };
    vtx[1].texcoord = { kUVLeft, kUVTop };
    vtx[1].normal = kNormalFront;

    // 右下
    vtx[2].position = { right, bottom, kDepthZero, kWComponent };
    vtx[2].texcoord = { kUVRight, kUVBottom };
    vtx[2].normal = kNormalFront;

    // 右上
    vtx[3].position = { right, top, kDepthZero, kWComponent };
    vtx[3].texcoord = { kUVRight, kUVTop };
    vtx[3].normal = kNormalFront;

    vertexResource_->Unmap(0, nullptr);

    // インデックスバッファ生成
    indexResource_ = DxUtils::CreateBufferResource(device, sizeof(uint32_t) * kIndexCount);
    ibView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    ibView_.SizeInBytes = sizeof(uint32_t) * kIndexCount;
    ibView_.Format = DXGI_FORMAT_R32_UINT;

    uint32_t* idx = nullptr;
    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&idx));
    idx[0] = 0; idx[1] = 1; idx[2] = 2;
    idx[3] = 2; idx[4] = 1; idx[5] = 3;
    indexResource_->Unmap(0, nullptr);
}

void TextObject3D::Update() {
    if (needsMeshUpdate_) {
        CreateMesh();
        needsMeshUpdate_ = false;
    }

    auto cameraMgr = CameraResource::GetCameraManager();
    Matrix4x4 world = Math::MakeIdentity();

    Matrix4x4 scaleMat = Math::MakeScaleMatrix(transform_.scale);
    Matrix4x4 transMat = Math::MakeTranslateMatrix(transform_.translate);

    switch (billboardMode_) {
    case BillboardMode::None:
        // 通常の3D回転
        world = Math::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
        break;

    case BillboardMode::AllAxis:
        // 全軸カメラ正対 (常にカメラ視線と並行)
        if (cameraMgr) {
            Matrix4x4 billBoardMatrix = Math::Inverse(cameraMgr->GetViewMatrix3D());
            billBoardMatrix.m[3][0] = 0.0f;
            billBoardMatrix.m[3][1] = 0.0f;
            billBoardMatrix.m[3][2] = 0.0f;
            world = Math::Multiply(Math::Multiply(scaleMat, billBoardMatrix), transMat);
        } else {
            world = Math::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
        }
        break;

    case BillboardMode::YAxisOnly:
        // 垂直軸 (Y軸) のみカメラを向く (上下の傾きは維持)
        if (cameraMgr && cameraMgr->GetActiveCamera()) {
            Vector3 camPos = cameraMgr->GetActiveCamera()->GetPosition();
            Vector3 toCam = { camPos.x - transform_.translate.x, 0.0f, camPos.z - transform_.translate.z };
            float angleY = std::atan2(toCam.x, toCam.z);
            Matrix4x4 rotY = Math::MakeRotateYMatrix(angleY);
            world = Math::Multiply(Math::Multiply(scaleMat, rotY), transMat);
        } else {
            world = Math::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
        }
        break;
    }

    if (cameraMgr) {
        Matrix4x4 wvp = Math::Multiply(
            Math::Multiply(world, cameraMgr->GetViewMatrix3D()),
            cameraMgr->GetProjectionMatrix3D()
        );

        Matrix4x4 worldForNormal = world;
        worldForNormal.m[3][0] = 0.0f;
        worldForNormal.m[3][1] = 0.0f;
        worldForNormal.m[3][2] = 0.0f;
        worldForNormal.m[3][3] = 1.0f;

        wvpData_->WVP = wvp;
        wvpData_->world = world;
        wvpData_->WorldInverseTranspose = Math::Transpose(Math::Inverse(worldForNormal));
    }

    Matrix4x4 uv = Math::MakeScaleMatrix(uvTransform_.scale);
    uv = Math::Multiply(uv, Math::MakeRotateZMatrix(uvTransform_.rotate.z));
    uv = Math::Multiply(uv, Math::MakeTranslateMatrix(uvTransform_.translate));
    materialData_->uvtransform = uv;
}

void TextObject3D::Draw(
    D3D12_GPU_DESCRIPTOR_HANDLE textureHandle,
    const std::string& envMapKey,
    const std::string& psoKey
) {
    if (!isVisible_ || textureHandle.ptr == 0) return;

    Object3DDrawer::GetInstance()->DrawIndexedHandle(
        this,
        vbView_,
        ibView_,
        kIndexCount,
        textureHandle,
        envMapKey,
        psoKey
    );
}
