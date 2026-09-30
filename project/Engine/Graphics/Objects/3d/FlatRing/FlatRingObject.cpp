#include "Engine/Graphics/Objects/3d/FlatRing/FlatRingObject.h"
#include "Engine/Graphics/Objects/3d/Drawer/Object3DDrawer.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Zuizui.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Light/Directional/DirectionalLight.h"
#include "Engine/Graphics/Objects/Light/Manager/LightManager.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Math/Matrix/Matrix.h"
#include <numbers>
#include <cmath>

FlatRingObject::FlatRingObject(uint32_t divide, float outerRadius, float innerRadius)
    : divide_(divide), outerRadius_(outerRadius), innerRadius_(innerRadius) {
}

void FlatRingObject::Initialize(int lightingMode) {
    Object3D::Initialize(lightingMode);
    CreateMesh();
}

void FlatRingObject::Update() {
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

void FlatRingObject::Draw(const std::string& textureKey, const std::string& envMapKey) {
    // 描画処理は共通描画クラス Object3DDrawer を通して実行
    uint32_t indexCount = divide_ * kIndicesPerQuad;
    Object3DDrawer::GetInstance()->DrawIndexed(this, vbView_, ibView_, indexCount, textureKey, envMapKey);
}

void FlatRingObject::CreateMesh() {
    uint32_t kVertexCount = (divide_ + 1) * 2;
    uint32_t kIndexCount = divide_ * 6;

    vertexResource_ = DxUtils::CreateBufferResource(sEngine->GetDevice(), sizeof(VertexData) * kVertexCount);
    vbView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vbView_.SizeInBytes = sizeof(VertexData) * kVertexCount;
    vbView_.StrideInBytes = sizeof(VertexData);

    VertexData* vtx;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vtx));

    float radianPerDivide = static_cast<float>(3.14159265f * 2.0f / divide_);

    uint32_t vIndex = 0;
    // index=0からdivide_までループし、端点でUVのUを0.0～1.0にする
    for (uint32_t i = 0; i <= divide_; ++i) {
        float theta = i * radianPerDivide;
        float s = std::sin(theta);
        float c = std::cos(theta);
        float u = static_cast<float>(i) / divide_;

        // 頂点法線（Z奥向き）
        Vector3 normal = { 0.0f, 0.0f, -1.0f };

        // 外側の頂点 (V = 0.0f)
        vtx[vIndex].position = { -s * outerRadius_, c * outerRadius_, 0.0f, 1.0f };
        vtx[vIndex].normal = normal;
        vtx[vIndex].texcoord = { u, 0.0f };
        vIndex++;

        // 内側の頂点 (V = 1.0f)
        vtx[vIndex].position = { -s * innerRadius_, c * innerRadius_, 0.0f, 1.0f };
        vtx[vIndex].normal = normal;
        vtx[vIndex].texcoord = { u, 1.0f };
        vIndex++;
    }
    vertexResource_->Unmap(0, nullptr);

    indexResource_ = DxUtils::CreateBufferResource(sEngine->GetDevice(), sizeof(uint32_t) * kIndexCount);
    ibView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    ibView_.SizeInBytes = sizeof(uint32_t) * kIndexCount;
    ibView_.Format = DXGI_FORMAT_R32_UINT;

    uint32_t* idxGPU = nullptr;
    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&idxGPU));

    uint32_t iIndex = 0;
    for (uint32_t i = 0; i < divide_; ++i) {
        uint32_t vOuter = i * 2;
        uint32_t vInner = i * 2 + 1;
        uint32_t vNextOuter = (i + 1) * 2;
        uint32_t vNextInner = (i + 1) * 2 + 1;

        // 三角形1 (Outer, Inner, NextOuter)
        idxGPU[iIndex++] = vOuter;
        idxGPU[iIndex++] = vInner;
        idxGPU[iIndex++] = vNextOuter;

        // 三角形2 (NextOuter, Inner, NextInner)
        idxGPU[iIndex++] = vNextOuter;
        idxGPU[iIndex++] = vInner;
        idxGPU[iIndex++] = vNextInner;
    }
    indexResource_->Unmap(0, nullptr);
}
