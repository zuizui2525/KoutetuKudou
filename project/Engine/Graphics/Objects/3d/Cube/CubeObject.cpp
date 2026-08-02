#include "Engine/Graphics/Objects/3d/Cube/CubeObject.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Base/DeferredRelease/DeferredReleaseManager.h"
#include "Engine/Zuizui.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Light/Directional/DirectionalLight.h"
#include "Engine/Graphics/Objects/Light/Manager/LightManager.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Math/Matrix/Matrix.h"

CubeObject::~CubeObject() {
    // GPUリソースを遅延解放キューに退避（GPUが使い終わるまで保持される）
    auto deferredMgr = DeferredReleaseManager::GetInstance();
    if (vertexResource_) {
        deferredMgr->Enqueue(std::move(vertexResource_));
    }
    if (indexResource_) {
        deferredMgr->Enqueue(std::move(indexResource_));
    }
}

void CubeObject::Initialize(int lightingMode) {
    // 基底クラスの初期化
    Object3D::Initialize(lightingMode);
    
    // 初回のメッシュ生成
    CreateMesh();
}

void CubeObject::Update() {
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

void CubeObject::Draw(const std::string& textureKey, const std::string& envMapKey) {
    if (!isVisible_) return;
    // コマンドリスト
    auto commandList = EngineResource::GetEngine()->GetDxCommon()->GetCommandList();
    // パイプラインの選択
    commandList->SetGraphicsRootSignature(EngineResource::GetEngine()->GetPSOManager()->GetRootSignature("Object3D"));
    commandList->SetPipelineState(EngineResource::GetEngine()->GetPSOManager()->GetPSO("Object3D"));
    // VBV, IBV設定
    commandList->IASetVertexBuffers(0, 1, &vbView_);
    commandList->IASetIndexBuffer(&ibView_);
    // 定数バッファ設定
    commandList->SetGraphicsRootConstantBufferView(0, wvpResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(1, materialResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(2, CameraResource::GetCameraManager()->GetGPUVirtualAddress());
    auto lightMgr = LightResource::GetLightManager();
    if (lightMgr) {
        commandList->SetGraphicsRootConstantBufferView(3, lightMgr->GetDirectionalLightGroupAddress());
        commandList->SetGraphicsRootConstantBufferView(4, lightMgr->GetPointLightGroupAddress());
        commandList->SetGraphicsRootConstantBufferView(5, lightMgr->GetSpotLightGroupAddress());
    }
    // 指定されたキーでテクスチャ取得
    commandList->SetGraphicsRootDescriptorTable(6, sTexMgr->GetGpuHandle(textureKey));
    
    // 環境マップテクスチャ
    if (!envMapKey.empty()) {
        if (materialData_->environmentCoefficient == 0.0f) {
            materialData_->environmentCoefficient = 1.0f;
        }
        commandList->SetGraphicsRootDescriptorTable(7, sTexMgr->GetGpuHandle(envMapKey));
    } else {
        materialData_->environmentCoefficient = 0.0f;
        // TextureCube以外のテクスチャを渡すとエラーになるため、空のときはskyboxTexをダミーとして渡す
        commandList->SetGraphicsRootDescriptorTable(7, sTexMgr->GetGpuHandle("skyboxTex")); 
    }
    
    // DrawIndexedInstanced
    const uint32_t indexCount = 36;
    const uint32_t instanceCount = 1;
    commandList->DrawIndexedInstanced(indexCount, instanceCount, 0, 0, 0);
}

void CubeObject::CreateMesh() {
    const uint32_t kVertexCount = 24;
    const uint32_t kIndexCount = 36;

    // 既存のGPUリソースがあれば遅延解放キューに退避してからリソースを新規作成
    auto deferredMgr = DeferredReleaseManager::GetInstance();
    if (vertexResource_) {
        deferredMgr->Enqueue(std::move(vertexResource_));
    }
    if (indexResource_) {
        deferredMgr->Enqueue(std::move(indexResource_));
    }

    // Vertex Resource 作成
    vertexResource_ = DxUtils::CreateBufferResource(sEngine->GetDevice(), sizeof(VertexData) * kVertexCount);
    vbView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vbView_.SizeInBytes = sizeof(VertexData) * kVertexCount;
    vbView_.StrideInBytes = sizeof(VertexData);

    VertexData* vtx;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vtx));

    const float halfWidth  = size_.x / 2.0f;
    const float halfHeight = size_.y / 2.0f;
    const float halfDepth  = size_.z / 2.0f;

    const float left   = -halfWidth;
    const float right  =  halfWidth;
    const float bottom = -halfHeight;
    const float top    =  halfHeight;
    const float front  = -halfDepth;
    const float back   =  halfDepth;
    const float wIndex =  1.0f;

    const float uvLeft   = 0.0f;
    const float uvRight  = 1.0f;
    const float uvTop    = 0.0f;
    const float uvBottom = 1.0f;

    // 各面の法線
    const Vector3 normalFront  = {  0.0f,  0.0f, -1.0f };
    const Vector3 normalBack   = {  0.0f,  0.0f,  1.0f };
    const Vector3 normalLeft   = { -1.0f,  0.0f,  0.0f };
    const Vector3 normalRight  = {  1.0f,  0.0f,  0.0f };
    const Vector3 normalTop    = {  0.0f,  1.0f,  0.0f };
    const Vector3 normalBottom = {  0.0f, -1.0f,  0.0f };

    uint32_t vIndex = 0;

    // 前面 (Front)
    vtx[vIndex++] = { { left,  bottom, front, wIndex }, { uvLeft,  uvBottom }, normalFront }; // 左下 0
    vtx[vIndex++] = { { left,  top,    front, wIndex }, { uvLeft,  uvTop    }, normalFront }; // 左上 1
    vtx[vIndex++] = { { right, bottom, front, wIndex }, { uvRight, uvBottom }, normalFront }; // 右下 2
    vtx[vIndex++] = { { right, top,    front, wIndex }, { uvRight, uvTop    }, normalFront }; // 右上 3

    // 背面 (Back) - 後ろから見たときの左下、左上、右下、右上
    vtx[vIndex++] = { { right, bottom, back, wIndex }, { uvLeft,  uvBottom }, normalBack }; // 左下 4
    vtx[vIndex++] = { { right, top,    back, wIndex }, { uvLeft,  uvTop    }, normalBack }; // 左上 5
    vtx[vIndex++] = { { left,  bottom, back, wIndex }, { uvRight, uvBottom }, normalBack }; // 右下 6
    vtx[vIndex++] = { { left,  top,    back, wIndex }, { uvRight, uvTop    }, normalBack }; // 右上 7

    // 左面 (Left) - 左から見たときの左下、左上、右下、右上
    vtx[vIndex++] = { { left, bottom, back,  wIndex }, { uvLeft,  uvBottom }, normalLeft }; // 左下 8
    vtx[vIndex++] = { { left, top,    back,  wIndex }, { uvLeft,  uvTop    }, normalLeft }; // 左上 9
    vtx[vIndex++] = { { left, bottom, front, wIndex }, { uvRight, uvBottom }, normalLeft }; // 右下 10
    vtx[vIndex++] = { { left, top,    front, wIndex }, { uvRight, uvTop    }, normalLeft }; // 右上 11

    // 右面 (Right) - 右から見たときの左下、左上、右下、右上
    vtx[vIndex++] = { { right, bottom, front, wIndex }, { uvLeft,  uvBottom }, normalRight }; // 左下 12
    vtx[vIndex++] = { { right, top,    front, wIndex }, { uvLeft,  uvTop    }, normalRight }; // 左上 13
    vtx[vIndex++] = { { right, bottom, back,  wIndex }, { uvRight, uvBottom }, normalRight }; // 右下 14
    vtx[vIndex++] = { { right, top,    back,  wIndex }, { uvRight, uvTop    }, normalRight }; // 右上 15

    // 上面 (Top) - 上から見たときの左下、左上、右下、右上
    vtx[vIndex++] = { { left,  top, front, wIndex }, { uvLeft,  uvBottom }, normalTop }; // 左下 16
    vtx[vIndex++] = { { left,  top, back,  wIndex }, { uvLeft,  uvTop    }, normalTop }; // 左上 17
    vtx[vIndex++] = { { right, top, front, wIndex }, { uvRight, uvBottom }, normalTop }; // 右下 18
    vtx[vIndex++] = { { right, top, back,  wIndex }, { uvRight, uvTop    }, normalTop }; // 右上 19

    // 下面 (Bottom) - 下から見たときの左下、左上、右下、右上
    vtx[vIndex++] = { { left,  bottom, back,  wIndex }, { uvLeft,  uvBottom }, normalBottom }; // 左下 20
    vtx[vIndex++] = { { left,  bottom, front, wIndex }, { uvLeft,  uvTop    }, normalBottom }; // 左上 21
    vtx[vIndex++] = { { right, bottom, back,  wIndex }, { uvRight, uvBottom }, normalBottom }; // 右下 22
    vtx[vIndex++] = { { right, bottom, front, wIndex }, { uvRight, uvTop    }, normalBottom }; // 右上 23

    vertexResource_->Unmap(0, nullptr);

    // Index Resource 作成
    indexResource_ = DxUtils::CreateBufferResource(sEngine->GetDevice(), sizeof(uint32_t) * kIndexCount);
    ibView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    ibView_.SizeInBytes = sizeof(uint32_t) * kIndexCount;
    ibView_.Format = DXGI_FORMAT_R32_UINT;

    uint32_t* idxGPU = nullptr;
    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&idxGPU));

    uint32_t iIndex = 0;
    for (uint32_t i = 0; i < 6; ++i) {
        uint32_t offset = i * 4;
        idxGPU[iIndex++] = offset + 0;
        idxGPU[iIndex++] = offset + 1;
        idxGPU[iIndex++] = offset + 2;
        idxGPU[iIndex++] = offset + 2;
        idxGPU[iIndex++] = offset + 1;
        idxGPU[iIndex++] = offset + 3;
    }

    indexResource_->Unmap(0, nullptr);
}
