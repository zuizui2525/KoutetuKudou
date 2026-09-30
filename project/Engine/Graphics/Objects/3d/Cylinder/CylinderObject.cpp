#include "Engine/Graphics/Objects/3d/Cylinder/CylinderObject.h"
#include "Engine/Graphics/Objects/3d/Drawer/Object3DDrawer.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Zuizui.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Light/Directional/DirectionalLight.h"
#include "Engine/Graphics/Objects/Light/Manager/LightManager.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Math/Matrix/Matrix.h"

void CylinderObject::Initialize(int lightingMode) {
    Object3D::Initialize(lightingMode);
    CreateMesh();
}

void CylinderObject::Update() {
    if (needsUpdate_) {
        CreateMesh();
        needsUpdate_ = false;
    }

    Matrix4x4 world = Math::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    Matrix4x4 wvp = Math::Multiply(Math::Multiply(world, CameraResource::GetCameraManager()->GetViewMatrix3D()), CameraResource::GetCameraManager()->GetProjectionMatrix3D());

    Matrix4x4 worldForNormal = world;
    worldForNormal.m[3][0] = 0.0f;
    worldForNormal.m[3][1] = 0.0f;
    worldForNormal.m[3][2] = 0.0f;
    worldForNormal.m[3][3] = 1.0f;

    wvpData_->WVP = wvp;
    wvpData_->world = world;
    wvpData_->WorldInverseTranspose = Math::Transpose(Math::Inverse(worldForNormal));

    Matrix4x4 uv = Math::MakeScaleMatrix(uvTransform_.scale);
    uv = Math::Multiply(uv, Math::MakeRotateZMatrix(uvTransform_.rotate.z));
    uv = Math::Multiply(uv, Math::MakeTranslateMatrix(uvTransform_.translate));
    materialData_->uvtransform = uv;
}

void CylinderObject::Draw(const std::string& textureKey, const std::string& envMapKey) {
    // 描画処理は共通描画クラス Object3DDrawer を通して実行
    uint32_t indexCount = subdivision_ * kIndicesPerSegment;
    Object3DDrawer::GetInstance()->DrawIndexed(this, vbView_, ibView_, indexCount, textureKey, envMapKey);
}

void CylinderObject::CreateMesh() {
    uint32_t numSideVertices = (subdivision_ + 1) * 2;
    uint32_t numTopVertices = subdivision_ + 2;
    uint32_t numBottomVertices = subdivision_ + 2;

    uint32_t kVertexCount = numSideVertices + numTopVertices + numBottomVertices;
    uint32_t kIndexCount = subdivision_ * 12;

    vertexResource_ = DxUtils::CreateBufferResource(sEngine->GetDevice(), sizeof(VertexData) * kVertexCount);
    vbView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vbView_.SizeInBytes = sizeof(VertexData) * kVertexCount;
    vbView_.StrideInBytes = sizeof(VertexData);

    VertexData* vtx;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vtx));

    float halfHeight = height_ / 2.0f;
    float thetaStep = static_cast<float>(M_PI * 2.0f / subdivision_);

    uint32_t vIndex = 0;

    // --- 側面 (Side) ---
    uint32_t sideBaseIndex = vIndex;
    for (uint32_t i = 0; i <= subdivision_; ++i) {
        float theta = i * thetaStep;
        float c = cosf(theta);
        float s = sinf(theta);
        float x = c * radius_;
        float z = s * radius_;
        float u = static_cast<float>(i) / subdivision_;

        // 下側の頂点
        vtx[vIndex].position = { x, -halfHeight, z, 1.0f };
        vtx[vIndex].normal = { c, 0.0f, s };
        vtx[vIndex].texcoord = { u, 1.0f };
        vIndex++;

        // 上側の頂点
        vtx[vIndex].position = { x, halfHeight, z, 1.0f };
        vtx[vIndex].normal = { c, 0.0f, s };
        vtx[vIndex].texcoord = { u, 0.0f };
        vIndex++;
    }

    // --- 上面 (Top) ---
    uint32_t topBaseIndex = vIndex;
    // 中心点
    vtx[vIndex].position = { 0.0f, halfHeight, 0.0f, 1.0f };
    vtx[vIndex].normal = { 0.0f, 1.0f, 0.0f };
    vtx[vIndex].texcoord = { 0.5f, 0.5f };
    vIndex++;
    // 円周上の点
    for (uint32_t i = 0; i <= subdivision_; ++i) {
        float theta = i * thetaStep;
        float c = cosf(theta);
        float s = sinf(theta);
        vtx[vIndex].position = { c * radius_, halfHeight, s * radius_, 1.0f };
        vtx[vIndex].normal = { 0.0f, 1.0f, 0.0f };
        vtx[vIndex].texcoord = { (c + 1.0f) * 0.5f, (1.0f - s) * 0.5f };
        vIndex++;
    }

    // --- 下面 (Bottom) ---
    uint32_t bottomBaseIndex = vIndex;
    // 中心点
    vtx[vIndex].position = { 0.0f, -halfHeight, 0.0f, 1.0f };
    vtx[vIndex].normal = { 0.0f, -1.0f, 0.0f };
    vtx[vIndex].texcoord = { 0.5f, 0.5f };
    vIndex++;
    // 円周上の点
    for (uint32_t i = 0; i <= subdivision_; ++i) {
        float theta = i * thetaStep;
        float c = cosf(theta);
        float s = sinf(theta);
        vtx[vIndex].position = { c * radius_, -halfHeight, s * radius_, 1.0f };
        vtx[vIndex].normal = { 0.0f, -1.0f, 0.0f };
        vtx[vIndex].texcoord = { (c + 1.0f) * 0.5f, (s + 1.0f) * 0.5f }; // 下面なのでvの向きを調整
        vIndex++;
    }

    vertexResource_->Unmap(0, nullptr);

    // --- インデックス生成 ---
    indexResource_ = DxUtils::CreateBufferResource(sEngine->GetDevice(), sizeof(uint32_t) * kIndexCount);
    ibView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    ibView_.SizeInBytes = sizeof(uint32_t) * kIndexCount;
    ibView_.Format = DXGI_FORMAT_R32_UINT;

    uint32_t* idxGPU = nullptr;
    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&idxGPU));

    uint32_t iIndex = 0;

    // 側面のインデックス
    for (uint32_t i = 0; i < subdivision_; ++i) {
        uint32_t current = sideBaseIndex + i * 2;
        uint32_t next = sideBaseIndex + (i + 1) * 2;

        // 三角形1 (下、上、次の下)
        idxGPU[iIndex++] = current;
        idxGPU[iIndex++] = current + 1;
        idxGPU[iIndex++] = next;

        // 三角形2 (次の下、上、次の上)
        idxGPU[iIndex++] = next;
        idxGPU[iIndex++] = current + 1;
        idxGPU[iIndex++] = next + 1;
    }

    // 上面のインデックス
    uint32_t topCenter = topBaseIndex;
    for (uint32_t i = 0; i < subdivision_; ++i) {
        idxGPU[iIndex++] = topCenter;
        idxGPU[iIndex++] = topCenter + 1 + i + 1;
        idxGPU[iIndex++] = topCenter + 1 + i;
    }

    // 下面のインデックス
    uint32_t bottomCenter = bottomBaseIndex;
    for (uint32_t i = 0; i < subdivision_; ++i) {
        idxGPU[iIndex++] = bottomCenter;
        idxGPU[iIndex++] = bottomCenter + 1 + i;
        idxGPU[iIndex++] = bottomCenter + 1 + i + 1;
    }

    indexResource_->Unmap(0, nullptr);
}
