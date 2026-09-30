#include "Engine/Graphics/Objects/3d/Ring/RingObject.h"
#include "Engine/Graphics/Objects/3d/Drawer/Object3DDrawer.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Zuizui.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Light/Directional/DirectionalLight.h"
#include "Engine/Graphics/Objects/Light/Manager/LightManager.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Math/Matrix/Matrix.h"

void RingObject::Initialize(int lightingMode) {
    Object3D::Initialize(lightingMode);
    CreateMesh();
}

void RingObject::Update() {
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

void RingObject::Draw(const std::string& textureKey, const std::string& envMapKey) {
    // 描画処理は共通描画クラス Object3DDrawer を通して実行
    uint32_t indexCount = mainSubdivision_ * tubeSubdivision_ * kIndicesPerSegment;
    Object3DDrawer::GetInstance()->DrawIndexed(this, vbView_, ibView_, indexCount, textureKey, envMapKey);
}

void RingObject::CreateMesh() {
    uint32_t kVertexCount = (mainSubdivision_ + 1) * (tubeSubdivision_ + 1);
    uint32_t kIndexCount = mainSubdivision_ * tubeSubdivision_ * 6;

    vertexResource_ = DxUtils::CreateBufferResource(sEngine->GetDevice(), sizeof(VertexData) * kVertexCount);
    vbView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vbView_.SizeInBytes = sizeof(VertexData) * kVertexCount;
    vbView_.StrideInBytes = sizeof(VertexData);

    VertexData* vtx;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vtx));

    float mainStep = static_cast<float>(M_PI * 2.0f / mainSubdivision_);
    float tubeStep = static_cast<float>(M_PI * 2.0f / tubeSubdivision_);

    uint32_t vIndex = 0;
    for (uint32_t i = 0; i <= mainSubdivision_; ++i) {
        float mainTheta = i * mainStep;
        float cosMain = cosf(mainTheta);
        float sinMain = sinf(mainTheta);

        for (uint32_t j = 0; j <= tubeSubdivision_; ++j) {
            float tubeTheta = j * tubeStep;
            float cosTube = cosf(tubeTheta);
            float sinTube = sinf(tubeTheta);

            float r_plus_rcos = mainRadius_ + tubeRadius_ * cosTube;
            float x = r_plus_rcos * cosMain;
            float y = tubeRadius_ * sinTube;
            float z = r_plus_rcos * sinMain;

            float nx = cosTube * cosMain;
            float ny = sinTube;
            float nz = cosTube * sinMain;

            float u = static_cast<float>(i) / mainSubdivision_;
            float v = static_cast<float>(j) / tubeSubdivision_;

            vtx[vIndex].position = { x, y, z, 1.0f };
            vtx[vIndex].normal = { nx, ny, nz };
            vtx[vIndex].texcoord = { u, v };
            vIndex++;
        }
    }
    vertexResource_->Unmap(0, nullptr);

    indexResource_ = DxUtils::CreateBufferResource(sEngine->GetDevice(), sizeof(uint32_t) * kIndexCount);
    ibView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    ibView_.SizeInBytes = sizeof(uint32_t) * kIndexCount;
    ibView_.Format = DXGI_FORMAT_R32_UINT;

    uint32_t* idxGPU = nullptr;
    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&idxGPU));

    uint32_t iIndex = 0;
    for (uint32_t i = 0; i < mainSubdivision_; ++i) {
        for (uint32_t j = 0; j < tubeSubdivision_; ++j) {
            uint32_t current = i * (tubeSubdivision_ + 1) + j;
            uint32_t nextMain = (i + 1) * (tubeSubdivision_ + 1) + j;

            uint32_t v0 = current;
            uint32_t v1 = current + 1;
            uint32_t v2 = nextMain;
            uint32_t v3 = nextMain + 1;

            idxGPU[iIndex++] = v0;
            idxGPU[iIndex++] = v1;
            idxGPU[iIndex++] = v2;

            idxGPU[iIndex++] = v1;
            idxGPU[iIndex++] = v3;
            idxGPU[iIndex++] = v2;
        }
    }
    indexResource_->Unmap(0, nullptr);
}
