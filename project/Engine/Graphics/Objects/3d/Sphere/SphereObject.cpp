#include "Engine/Graphics/Objects/3d/Sphere/SphereObject.h"
#include "Engine/Graphics/Objects/3d/Drawer/Object3DDrawer.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Zuizui.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Light/Directional/DirectionalLight.h"
#include "Engine/Graphics/Objects/Light/Manager/LightManager.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Math/Matrix/Matrix.h"

void SphereObject::Initialize(int lightingMode) {
    // 基底クラスの初期化
    Object3D::Initialize(lightingMode);
    
    // 初回のメッシュ生成
    CreateMesh();
}

void SphereObject::Update() {
    // パラメータに変更があればメッシュを再生成
    if (needsUpdate_) {
        CreateMesh();
        needsUpdate_ = false;
    }

    // 行列更新
    Matrix4x4 world = Math::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    if (parent_) {
        world = Math::Multiply(world, parent_->GetWorldMatrix());
    }
    matWorld_ = world;

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

void SphereObject::Draw(const std::string& textureKey, const std::string& envMapKey) {
    // 描画処理は共通描画クラス Object3DDrawer を通して実行
    constexpr uint32_t kIndicesPerQuad = 6;
    uint32_t indexCount = subdivision_ * subdivision_ * kIndicesPerQuad;
    Object3DDrawer::GetInstance()->DrawIndexed(this, vbView_, ibView_, indexCount, textureKey, envMapKey);
}

void SphereObject::CreateMesh() {
    // 頂点数とインデックス数を計算
    uint32_t kVertexCount = (subdivision_ + 1) * (subdivision_ + 1);
    uint32_t kIndexCount = subdivision_ * subdivision_ * 6;
    float kLonEvery = static_cast<float>(M_PI * 2.0f / subdivision_);
    float kLatEvery = static_cast<float>(M_PI / subdivision_);

    // Vertex Resource 作成 (以前のリソースはComPtrの代入により自動解放される)
    vertexResource_ = DxUtils::CreateBufferResource(sEngine->GetDevice(), sizeof(VertexData) * kVertexCount);
    vbView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vbView_.SizeInBytes = sizeof(VertexData) * kVertexCount;
    vbView_.StrideInBytes = sizeof(VertexData);

    VertexData* vtx;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vtx));

    for (uint32_t latIndex = 0; latIndex <= subdivision_; ++latIndex) {
        float lat = static_cast<float>(-M_PI / 2.0f + kLatEvery * latIndex);
        for (uint32_t lonIndex = 0; lonIndex <= subdivision_; ++lonIndex) {
            float lon = kLonEvery * lonIndex;

            float x = cosf(lat) * cosf(lon) * radius_;
            float y = sinf(lat) * radius_;
            float z = cosf(lat) * sinf(lon) * radius_;

            uint32_t index = latIndex * (subdivision_ + 1) + lonIndex;
            vtx[index].position = { x, y, z, 1.0f };
            vtx[index].texcoord = { (float)lonIndex / subdivision_, 1.0f - (float)latIndex / subdivision_ };
            vtx[index].normal = { x, y, z };
        }
    }
    vertexResource_->Unmap(0, nullptr);

    // Index Resource 作成
    indexResource_ = DxUtils::CreateBufferResource(sEngine->GetDevice(), sizeof(uint32_t) * kIndexCount);
    ibView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    ibView_.SizeInBytes = sizeof(uint32_t) * kIndexCount;
    ibView_.Format = DXGI_FORMAT_R32_UINT;

    uint32_t* idxGPU = nullptr;
    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&idxGPU));

    uint32_t idx = 0;
    for (uint32_t latIndex = 0; latIndex < subdivision_; ++latIndex) {
        for (uint32_t lonIndex = 0; lonIndex < subdivision_; ++lonIndex) {
            uint32_t current = latIndex * (subdivision_ + 1) + lonIndex;
            uint32_t next = current + subdivision_ + 1;

            idxGPU[idx++] = current;
            idxGPU[idx++] = next;
            idxGPU[idx++] = current + 1;

            idxGPU[idx++] = current + 1;
            idxGPU[idx++] = next;
            idxGPU[idx++] = next + 1;
        }
    }
    indexResource_->Unmap(0, nullptr);
}
