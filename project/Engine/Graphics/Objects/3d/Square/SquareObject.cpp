#include "Engine/Graphics/Objects/3d/Square/SquareObject.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Zuizui.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Light/Directional/DirectionalLight.h"
#include "Engine/Graphics/Objects/Light/Manager/LightManager.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Math/Matrix/Matrix.h"

void SquareObject::Initialize(int lightingMode) {
    // 基底クラスの初期化
    Object3D::Initialize(lightingMode);
    
    // 初回のメッシュ生成
    CreateMesh();
}

void SquareObject::Update() {
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

void SquareObject::Draw(const std::string& textureKey, const std::string& envMapKey, const std::string& psoKey) {
    if (!isVisible_) return;
    // コマンドリスト
    auto commandList = EngineResource::GetEngine()->GetDxCommon()->GetCommandList();
    // パイプラインの選択
    commandList->SetGraphicsRootSignature(EngineResource::GetEngine()->GetPSOManager()->GetRootSignature(psoKey));
    commandList->SetPipelineState(EngineResource::GetEngine()->GetPSOManager()->GetPSO(psoKey));
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
    const uint32_t indexCount = 6;
    const uint32_t instanceCount = 1;
    commandList->DrawIndexedInstanced(indexCount, instanceCount, 0, 0, 0);
}

void SquareObject::CreateMesh() {
    const uint32_t kVertexCount = 4;
    const uint32_t kIndexCount = 6;

    // Vertex Resource 作成
    vertexResource_ = DxUtils::CreateBufferResource(sEngine->GetDevice(), sizeof(VertexData) * kVertexCount);
    vbView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vbView_.SizeInBytes = sizeof(VertexData) * kVertexCount;
    vbView_.StrideInBytes = sizeof(VertexData);

    VertexData* vtx;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vtx));

    const float halfWidth = size_.x / 2.0f;
    const float halfHeight = size_.y / 2.0f;

    const float left = -halfWidth;
    const float right = halfWidth;
    const float bottom = -halfHeight;
    const float top = halfHeight;
    const float zIndex = 0.0f;
    const float wIndex = 1.0f;

    const float uvLeft = 0.0f;
    const float uvRight = 1.0f;
    const float uvTop = 0.0f;
    const float uvBottom = 1.0f;

    const Vector3 normalFront = { 0.0f, 0.0f, -1.0f };

    // 左下
    vtx[0].position = { left, bottom, zIndex, wIndex };
    vtx[0].texcoord = { uvLeft, uvBottom };
    vtx[0].normal = normalFront;

    // 左上
    vtx[1].position = { left, top, zIndex, wIndex };
    vtx[1].texcoord = { uvLeft, uvTop };
    vtx[1].normal = normalFront;

    // 右下
    vtx[2].position = { right, bottom, zIndex, wIndex };
    vtx[2].texcoord = { uvRight, uvBottom };
    vtx[2].normal = normalFront;

    // 右上
    vtx[3].position = { right, top, zIndex, wIndex };
    vtx[3].texcoord = { uvRight, uvTop };
    vtx[3].normal = normalFront;

    vertexResource_->Unmap(0, nullptr);

    // Index Resource 作成
    indexResource_ = DxUtils::CreateBufferResource(sEngine->GetDevice(), sizeof(uint32_t) * kIndexCount);
    ibView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    ibView_.SizeInBytes = sizeof(uint32_t) * kIndexCount;
    ibView_.Format = DXGI_FORMAT_R32_UINT;

    uint32_t* idxGPU = nullptr;
    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&idxGPU));

    idxGPU[0] = 0;
    idxGPU[1] = 1;
    idxGPU[2] = 2;
    idxGPU[3] = 2;
    idxGPU[4] = 1;
    idxGPU[5] = 3;

    indexResource_->Unmap(0, nullptr);
}
