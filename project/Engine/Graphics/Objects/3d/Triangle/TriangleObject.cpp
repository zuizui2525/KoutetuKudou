#include "Engine/Graphics/Objects/3d/Triangle/TriangleObject.h"
#include "Engine/Graphics/Objects/3d/Drawer/Object3DDrawer.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Zuizui.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"

void TriangleObject::Initialize(int lightingMode) {
    // 基底クラスの初期化
    Object3D::Initialize(lightingMode);
    
    // Vertex (三角形3頂点)
    vertexResource_ = DxUtils::CreateBufferResource(sEngine->GetDevice(), sizeof(VertexData) * kVertexCount);
    vbView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vbView_.SizeInBytes = sizeof(VertexData) * kVertexCount;
    vbView_.StrideInBytes = sizeof(VertexData);

    VertexData* vtx;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vtx));
    vtx[0].position = { -0.5f, -0.5f, 0.0f, 1.0f }; // 左下
    vtx[0].texcoord = { 0.0f, 1.0f };
    vtx[0].normal = { 0.0f, 0.0f, -1.0f };

    vtx[1].position = { 0.0f, 0.5f, 0.0f, 1.0f }; // 上
    vtx[1].texcoord = { 0.5f, 0.0f };
    vtx[1].normal = { 0.0f, 0.0f, -1.0f };

    vtx[2].position = { 0.5f, -0.5f, 0.0f, 1.0f }; // 右下
    vtx[2].texcoord = { 1.0f, 1.0f };
    vtx[2].normal = { 0.0f, 0.0f, -1.0f };

    vertexResource_->Unmap(0, nullptr);
}

void TriangleObject::Update() {
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

void TriangleObject::Draw(const std::string& textureKey, const std::string& envMapKey) {
    // 描画処理は共通描画クラス Object3DDrawer を通して実行
    Object3DDrawer::GetInstance()->Draw(this, vbView_, kVertexCount, textureKey, envMapKey);
}

