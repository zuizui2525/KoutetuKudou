#include "Engine/Graphics/Objects/3d/TriangularPyramid/TriangularPyramidObject.h"
#include "Engine/Graphics/Objects/3d/Drawer/Object3DDrawer.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Zuizui.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Light/Directional/DirectionalLight.h"
#include "Engine/Graphics/Objects/Light/Manager/LightManager.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Math/Matrix/Matrix.h"

void TriangularPyramidObject::Initialize(int lightingMode) {
    Object3D::Initialize(lightingMode);
    CreateMesh();
}

void TriangularPyramidObject::Update() {
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

void TriangularPyramidObject::Draw(const std::string& textureKey, const std::string& envMapKey) {
    // 描画処理は共通描画クラス Object3DDrawer を通して実行
    Object3DDrawer::GetInstance()->DrawIndexed(this, vbView_, ibView_, kIndexCount, textureKey, envMapKey);
}

void TriangularPyramidObject::CreateMesh() {
    uint32_t kVertexCount = 12; // 3 faces * 3 vertices + 1 bottom * 3 vertices
    uint32_t kIndexCount = 12;

    vertexResource_ = DxUtils::CreateBufferResource(sEngine->GetDevice(), sizeof(VertexData) * kVertexCount);
    vbView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vbView_.SizeInBytes = sizeof(VertexData) * kVertexCount;
    vbView_.StrideInBytes = sizeof(VertexData);

    VertexData* vtx;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vtx));

    float hh = height_ / 2.0f;
    float r = radius_;
    float sqrt3_2 = std::sqrt(3.0f) / 2.0f;

    // 頂点座標
    Vector3 top = { 0.0f, hh, 0.0f };
    Vector3 vFront = { 0.0f, -hh, -r };                  // 手前
    Vector3 vRightBack = { r * sqrt3_2, -hh, r * 0.5f };  // 右奥
    Vector3 vLeftBack = { -r * sqrt3_2, -hh, r * 0.5f };   // 左奥

    auto CalcNormal = [](const Vector3& p0, const Vector3& p1, const Vector3& p2) {
        Vector3 v1 = { p1.x - p0.x, p1.y - p0.y, p1.z - p0.z };
        Vector3 v2 = { p2.x - p0.x, p2.y - p0.y, p2.z - p0.z };
        Vector3 n = {
            v1.y * v2.z - v1.z * v2.y,
            v1.z * v2.x - v1.x * v2.z,
            v1.x * v2.y - v1.y * v2.x
        };
        float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        if (len > 0.0f) { n.x /= len; n.y /= len; n.z /= len; }
        return n;
    };

    Vector3 nRight = CalcNormal(vFront, top, vRightBack);
    Vector3 nBack  = CalcNormal(vRightBack, top, vLeftBack);
    Vector3 nLeft  = CalcNormal(vLeftBack, top, vFront);
    Vector3 nBottom = { 0.0f, -1.0f, 0.0f };

    uint32_t vIndex = 0;

    // 右側面 (Right)
    vtx[vIndex].position = { top.x, top.y, top.z, 1.0f };
    vtx[vIndex].texcoord = { 0.5f, 0.0f };
    vtx[vIndex++].normal = nRight;

    vtx[vIndex].position = { vRightBack.x, vRightBack.y, vRightBack.z, 1.0f };
    vtx[vIndex].texcoord = { 1.0f, 1.0f };
    vtx[vIndex++].normal = nRight;

    vtx[vIndex].position = { vFront.x, vFront.y, vFront.z, 1.0f };
    vtx[vIndex].texcoord = { 0.0f, 1.0f };
    vtx[vIndex++].normal = nRight;

    // 後面 (Back)
    vtx[vIndex].position = { top.x, top.y, top.z, 1.0f };
    vtx[vIndex].texcoord = { 0.5f, 0.0f };
    vtx[vIndex++].normal = nBack;

    vtx[vIndex].position = { vLeftBack.x, vLeftBack.y, vLeftBack.z, 1.0f };
    vtx[vIndex].texcoord = { 1.0f, 1.0f };
    vtx[vIndex++].normal = nBack;

    vtx[vIndex].position = { vRightBack.x, vRightBack.y, vRightBack.z, 1.0f };
    vtx[vIndex].texcoord = { 0.0f, 1.0f };
    vtx[vIndex++].normal = nBack;

    // 左側面 (Left)
    vtx[vIndex].position = { top.x, top.y, top.z, 1.0f };
    vtx[vIndex].texcoord = { 0.5f, 0.0f };
    vtx[vIndex++].normal = nLeft;

    vtx[vIndex].position = { vFront.x, vFront.y, vFront.z, 1.0f };
    vtx[vIndex].texcoord = { 1.0f, 1.0f };
    vtx[vIndex++].normal = nLeft;

    vtx[vIndex].position = { vLeftBack.x, vLeftBack.y, vLeftBack.z, 1.0f };
    vtx[vIndex].texcoord = { 0.0f, 1.0f };
    vtx[vIndex++].normal = nLeft;

    // 底面 (Bottom)
    uint32_t bottomBase = vIndex;
    vtx[vIndex].position = { vFront.x, vFront.y, vFront.z, 1.0f };
    vtx[vIndex].texcoord = { 0.5f, 1.0f };
    vtx[vIndex++].normal = nBottom;

    vtx[vIndex].position = { vRightBack.x, vRightBack.y, vRightBack.z, 1.0f };
    vtx[vIndex].texcoord = { 1.0f, 0.0f };
    vtx[vIndex++].normal = nBottom;

    vtx[vIndex].position = { vLeftBack.x, vLeftBack.y, vLeftBack.z, 1.0f };
    vtx[vIndex].texcoord = { 0.0f, 0.0f };
    vtx[vIndex++].normal = nBottom;

    vertexResource_->Unmap(0, nullptr);

    // インデックス生成
    indexResource_ = DxUtils::CreateBufferResource(sEngine->GetDevice(), sizeof(uint32_t) * kIndexCount);
    ibView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    ibView_.SizeInBytes = sizeof(uint32_t) * kIndexCount;
    ibView_.Format = DXGI_FORMAT_R32_UINT;

    uint32_t* idxGPU = nullptr;
    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&idxGPU));

    uint32_t iIndex = 0;

    for (uint32_t i = 0; i < 4; ++i) {
        idxGPU[iIndex++] = i * 3 + 0;
        idxGPU[iIndex++] = i * 3 + 1;
        idxGPU[iIndex++] = i * 3 + 2;
    }

    indexResource_->Unmap(0, nullptr);
}
