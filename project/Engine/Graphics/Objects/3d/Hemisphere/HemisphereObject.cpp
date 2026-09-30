#include "Engine/Graphics/Objects/3d/Hemisphere/HemisphereObject.h"
#include "Engine/Graphics/Objects/3d/Drawer/Object3DDrawer.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Zuizui.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Light/Directional/DirectionalLight.h"
#include "Engine/Graphics/Objects/Light/Manager/LightManager.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Math/Matrix/Matrix.h"

void HemisphereObject::Initialize(int lightingMode) {
    // 基底クラスの初期化
    Object3D::Initialize(lightingMode);
    
    // 初回のメッシュ生成
    CreateMesh();
}

void HemisphereObject::Update() {
    // パラメータに変更があればメッシュを再生成
    if (needsUpdate_) {
        CreateMesh();
        needsUpdate_ = false;
    }

    // 行列更新
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

void HemisphereObject::Draw(const std::string& textureKey, const std::string& envMapKey) {
    // 描画処理は共通描画クラス Object3DDrawer を通して実行
    Object3DDrawer::GetInstance()->DrawIndexed(this, vbView_, ibView_, indexCount_, textureKey, envMapKey);
}

void HemisphereObject::CreateMesh() {
    // 緯度方向の分割数（半球なので subdivision_ の半分とする）
    uint32_t latSubdivision = (std::max)(1u, subdivision_ / 2);
    
    uint32_t kDomeVertexCount = (latSubdivision + 1) * (subdivision_ + 1);
    uint32_t kBaseVertexCount = 1 + (subdivision_ + 1); // 中心 + 周囲
    uint32_t kVertexCount = kDomeVertexCount + kBaseVertexCount;

    uint32_t kDomeIndexCount = latSubdivision * subdivision_ * 6;
    uint32_t kBaseIndexCount = subdivision_ * 3;
    indexCount_ = kDomeIndexCount + kBaseIndexCount;

    float kLonEvery = static_cast<float>(M_PI * 2.0f / subdivision_);
    float kLatEvery = static_cast<float>((M_PI / 2.0f) / latSubdivision);

    // Vertex Resource 作成 (以前のリソースはComPtrの代入により自動解放される)
    vertexResource_ = DxUtils::CreateBufferResource(sEngine->GetDevice(), sizeof(VertexData) * kVertexCount);
    vbView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vbView_.SizeInBytes = sizeof(VertexData) * kVertexCount;
    vbView_.StrideInBytes = sizeof(VertexData);

    VertexData* vtx;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vtx));

    uint32_t vIdx = 0;

    // ドーム部分 (lat: 0=赤道 〜 M_PI/2=北極)
    for (uint32_t latIndex = 0; latIndex <= latSubdivision; ++latIndex) {
        float lat = kLatEvery * latIndex;
        for (uint32_t lonIndex = 0; lonIndex <= subdivision_; ++lonIndex) {
            float lon = kLonEvery * lonIndex;

            float x = cosf(lat) * cosf(lon) * radius_;
            float y = sinf(lat) * radius_;
            float z = cosf(lat) * sinf(lon) * radius_;

            vtx[vIdx].position = { x, y, z, 1.0f };
            // 球体の上半分に相当するUV (v値: 0.5 〜 0.0)
            vtx[vIdx].texcoord = { (float)lonIndex / subdivision_, 0.5f - 0.5f * ((float)latIndex / latSubdivision) };
            vtx[vIdx].normal = { x, y, z };
            vIdx++;
        }
    }

    // 底面部分
    uint32_t baseCenterVIdx = vIdx;
    vtx[vIdx].position = { 0.0f, 0.0f, 0.0f, 1.0f };
    vtx[vIdx].texcoord = { 0.5f, 0.5f };
    vtx[vIdx].normal = { 0.0f, -1.0f, 0.0f };
    vIdx++;

    uint32_t baseEdgeStartVIdx = vIdx;
    for (uint32_t lonIndex = 0; lonIndex <= subdivision_; ++lonIndex) {
        float lon = kLonEvery * lonIndex;
        float x = cosf(lon) * radius_;
        float z = sinf(lon) * radius_;

        vtx[vIdx].position = { x, 0.0f, z, 1.0f };
        // 底面UVは円形に投影
        vtx[vIdx].texcoord = { x / radius_ * 0.5f + 0.5f, z / radius_ * 0.5f + 0.5f };
        vtx[vIdx].normal = { 0.0f, -1.0f, 0.0f };
        vIdx++;
    }

    vertexResource_->Unmap(0, nullptr);

    // Index Resource 作成
    indexResource_ = DxUtils::CreateBufferResource(sEngine->GetDevice(), sizeof(uint32_t) * indexCount_);
    ibView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    ibView_.SizeInBytes = sizeof(uint32_t) * indexCount_;
    ibView_.Format = DXGI_FORMAT_R32_UINT;

    uint32_t* idxGPU = nullptr;
    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&idxGPU));

    uint32_t iIdx = 0;

    // ドーム部分のインデックス
    for (uint32_t latIndex = 0; latIndex < latSubdivision; ++latIndex) {
        for (uint32_t lonIndex = 0; lonIndex < subdivision_; ++lonIndex) {
            uint32_t current = latIndex * (subdivision_ + 1) + lonIndex;
            uint32_t next = current + subdivision_ + 1;

            idxGPU[iIdx++] = current;
            idxGPU[iIdx++] = next;
            idxGPU[iIdx++] = current + 1;

            idxGPU[iIdx++] = current + 1;
            idxGPU[iIdx++] = next;
            idxGPU[iIdx++] = next + 1;
        }
    }

    // 底面部分のインデックス
    for (uint32_t lonIndex = 0; lonIndex < subdivision_; ++lonIndex) {
        uint32_t current = baseEdgeStartVIdx + lonIndex;
        uint32_t next = baseEdgeStartVIdx + lonIndex + 1;

        idxGPU[iIdx++] = baseCenterVIdx;
        idxGPU[iIdx++] = current;
        idxGPU[iIdx++] = next;
    }

    indexResource_->Unmap(0, nullptr);
}
