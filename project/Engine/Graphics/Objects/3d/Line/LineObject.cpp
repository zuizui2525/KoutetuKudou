#include "Engine/Graphics/Objects/3d/Line/LineObject.h"
#include "Engine/Graphics/Objects/3d/Drawer/Object3DDrawer.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Zuizui.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Light/Directional/DirectionalLight.h"
#include "Engine/Graphics/Objects/Light/Manager/LightManager.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Math/Matrix/Matrix.h"
#include <cmath>

void LineObject::Initialize(int lightingMode) {
    Object3D::Initialize(lightingMode);
    CreateMesh();
    UpdateVertex();
}

void LineObject::Update() {
    // カメラに合わせて常に太さを計算し直すため、毎フレーム更新する
    UpdateVertex();

    // startPoint/endPointのワールド座標を直接利用するため、基本はWorld行列を単位行列にする
    Matrix4x4 world = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    
    // フラグがtrueのときはTransformの影響を受けて移動・回転・拡縮する
    if (useTransform_) {
        world = Math::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    }
    
    Matrix4x4 wvp = Math::Multiply(Math::Multiply(world, CameraResource::GetCameraManager()->GetViewMatrix3D()), CameraResource::GetCameraManager()->GetProjectionMatrix3D());

    // ライティング用の行列
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

void LineObject::Draw(const std::string& textureKey) {
    // 描画処理は共通描画クラス Object3DDrawer を通して実行
    Object3DDrawer::GetInstance()->DrawIndexed(this, vbView_, ibView_, kIndexCount, textureKey);
}

void LineObject::CreateMesh() {
    vertexResource_ = DxUtils::CreateBufferResource(sEngine->GetDevice(), sizeof(VertexData) * 4);
    vbView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vbView_.SizeInBytes = sizeof(VertexData) * 4;
    vbView_.StrideInBytes = sizeof(VertexData);

    indexResource_ = DxUtils::CreateBufferResource(sEngine->GetDevice(), sizeof(uint32_t) * 6);
    ibView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    ibView_.SizeInBytes = sizeof(uint32_t) * 6;
    ibView_.Format = DXGI_FORMAT_R32_UINT;

    uint32_t* idxGPU = nullptr;
    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&idxGPU));
    idxGPU[0] = 0; // 左下
    idxGPU[1] = 1; // 左上
    idxGPU[2] = 2; // 右下
    idxGPU[3] = 2; // 右下
    idxGPU[4] = 1; // 左上
    idxGPU[5] = 3; // 右上
    indexResource_->Unmap(0, nullptr);
}

void LineObject::UpdateVertex() {
    // カメラのワールド座標を取得（ビュー行列の逆行列の平行移動成分）
    Matrix4x4 viewMat = CameraResource::GetCameraManager()->GetViewMatrix3D();
    Matrix4x4 viewInv = Math::Inverse(viewMat);
    Vector3 cameraPos = { viewInv.m[3][0], viewInv.m[3][1], viewInv.m[3][2] };

    // 線の方向ベクトル (end - start)
    Vector3 lineVec = {
        endPoint_.x - startPoint_.x,
        endPoint_.y - startPoint_.y,
        endPoint_.z - startPoint_.z
    };
    
    // カメラから線の中点への視線ベクトル
    Vector3 centerPos = {
        startPoint_.x + lineVec.x * 0.5f,
        startPoint_.y + lineVec.y * 0.5f,
        startPoint_.z + lineVec.z * 0.5f
    };
    Vector3 viewVec = {
        centerPos.x - cameraPos.x,
        centerPos.y - cameraPos.y,
        centerPos.z - cameraPos.z
    };

    // 外積で「線」と「視線」の両方に垂直なベクトル（押し出し方向）を計算
    Vector3 normal = {
        lineVec.y * viewVec.z - lineVec.z * viewVec.y,
        lineVec.z * viewVec.x - lineVec.x * viewVec.z,
        lineVec.x * viewVec.y - lineVec.y * viewVec.x
    };

    // 押し出し方向の正規化
    float length = sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    if (length != 0.0f) {
        normal.x /= length;
        normal.y /= length;
        normal.z /= length;
    }

    // 太さの半分
    float halfThickness = thickness_ * 0.5f;
    Vector3 offset = {
        normal.x * halfThickness,
        normal.y * halfThickness,
        normal.z * halfThickness
    };

    // 頂点バッファの書き換え
    if (!vertexResource_) return;
    VertexData* vtx = nullptr;
    HRESULT hr = vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vtx));
    if (FAILED(hr) || !vtx) return;

    // 頂点0: 始点 - オフセット
    vtx[0].position = { startPoint_.x - offset.x, startPoint_.y - offset.y, startPoint_.z - offset.z, 1.0f };
    vtx[0].normal = {0.0f, 1.0f, 0.0f}; // 線なのでライティングは無効を想定
    vtx[0].texcoord = { 0.0f, 1.0f };

    // 頂点1: 終点 - オフセット
    vtx[1].position = { endPoint_.x - offset.x, endPoint_.y - offset.y, endPoint_.z - offset.z, 1.0f };
    vtx[1].normal = {0.0f, 1.0f, 0.0f};
    vtx[1].texcoord = { 0.0f, 0.0f };

    // 頂点2: 始点 + オフセット
    vtx[2].position = { startPoint_.x + offset.x, startPoint_.y + offset.y, startPoint_.z + offset.z, 1.0f };
    vtx[2].normal = {0.0f, 1.0f, 0.0f};
    vtx[2].texcoord = { 1.0f, 1.0f };

    // 頂点3: 終点 + オフセット
    vtx[3].position = { endPoint_.x + offset.x, endPoint_.y + offset.y, endPoint_.z + offset.z, 1.0f };
    vtx[3].normal = {0.0f, 1.0f, 0.0f};
    vtx[3].texcoord = { 1.0f, 0.0f };

    vertexResource_->Unmap(0, nullptr);
}
