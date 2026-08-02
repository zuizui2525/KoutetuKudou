#include "Engine/Graphics/Objects/3d/Pyramid/PyramidObject.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Base/DeferredRelease/DeferredReleaseManager.h"
#include "Engine/Zuizui.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Light/Directional/DirectionalLight.h"
#include "Engine/Graphics/Objects/Light/Manager/LightManager.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Math/Matrix/Matrix.h"

PyramidObject::~PyramidObject() {
    // GPUリソースを遅延解放キューに退避（GPUが使い終わるまで保持される）
    auto deferredMgr = DeferredReleaseManager::GetInstance();
    if (vertexResource_) {
        deferredMgr->Enqueue(std::move(vertexResource_));
    }
    if (indexResource_) {
        deferredMgr->Enqueue(std::move(indexResource_));
    }
}

void PyramidObject::Initialize(int lightingMode) {
    Object3D::Initialize(lightingMode);
    CreateMesh();
}

void PyramidObject::Update() {
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

void PyramidObject::Draw(const std::string& textureKey, const std::string& envMapKey) {
    if (!isVisible_) return;
    auto commandList = EngineResource::GetEngine()->GetDxCommon()->GetCommandList();
    commandList->SetGraphicsRootSignature(EngineResource::GetEngine()->GetPSOManager()->GetRootSignature("Object3D"));
    commandList->SetPipelineState(EngineResource::GetEngine()->GetPSOManager()->GetPSO("Object3D"));
    commandList->IASetVertexBuffers(0, 1, &vbView_);
    commandList->IASetIndexBuffer(&ibView_);
    commandList->SetGraphicsRootConstantBufferView(0, wvpResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(1, materialResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(2, CameraResource::GetCameraManager()->GetGPUVirtualAddress());
    
    auto lightMgr = LightResource::GetLightManager();
    if (lightMgr) {
        commandList->SetGraphicsRootConstantBufferView(3, lightMgr->GetDirectionalLightGroupAddress());
        commandList->SetGraphicsRootConstantBufferView(4, lightMgr->GetPointLightGroupAddress());
        commandList->SetGraphicsRootConstantBufferView(5, lightMgr->GetSpotLightGroupAddress());
    }
    
    commandList->SetGraphicsRootDescriptorTable(6, sTexMgr->GetGpuHandle(textureKey));
    
    if (!envMapKey.empty()) {
        if (materialData_->environmentCoefficient == 0.0f) {
            materialData_->environmentCoefficient = 1.0f;
        }
        commandList->SetGraphicsRootDescriptorTable(7, sTexMgr->GetGpuHandle(envMapKey));
    } else {
        materialData_->environmentCoefficient = 0.0f;
        commandList->SetGraphicsRootDescriptorTable(7, sTexMgr->GetGpuHandle("skyboxTex")); 
    }
    
    uint32_t indexCount = 18;
    commandList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
}

void PyramidObject::CreateMesh() {
    uint32_t kVertexCount = 16; // 4 faces * 3 vertices + 4 vertices (bottom)
    uint32_t kIndexCount = 18;  // 4 faces * 3 indices + 2 triangles * 3 indices

    // 既存のGPUリソースがあれば遅延解放キューに退避してからリソースを新規作成
    auto deferredMgr = DeferredReleaseManager::GetInstance();
    if (vertexResource_) {
        deferredMgr->Enqueue(std::move(vertexResource_));
    }
    if (indexResource_) {
        deferredMgr->Enqueue(std::move(indexResource_));
    }

    vertexResource_ = DxUtils::CreateBufferResource(sEngine->GetDevice(), sizeof(VertexData) * kVertexCount);
    vbView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vbView_.SizeInBytes = sizeof(VertexData) * kVertexCount;
    vbView_.StrideInBytes = sizeof(VertexData);

    VertexData* vtx;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vtx));

    float hw = size_.x / 2.0f;
    float hh = size_.y / 2.0f;
    float hd = size_.z / 2.0f;

    // 頂点座標
    Vector3 top = { 0.0f, hh, 0.0f };
    Vector3 bl = { -hw, -hh, -hd }; // 手前左
    Vector3 br = {  hw, -hh, -hd }; // 手前右
    Vector3 tr = {  hw, -hh,  hd }; // 奥右
    Vector3 tl = { -hw, -hh,  hd }; // 奥左

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

    Vector3 nFront = CalcNormal(bl, top, br);
    Vector3 nRight = CalcNormal(br, top, tr);
    Vector3 nBack  = CalcNormal(tr, top, tl);
    Vector3 nLeft  = CalcNormal(tl, top, bl);
    Vector3 nBottom = { 0.0f, -1.0f, 0.0f };

    uint32_t vIndex = 0;

    // 前面 (Front)
    vtx[vIndex].position = { top.x, top.y, top.z, 1.0f };
    vtx[vIndex].texcoord = { 0.5f, 0.0f };
    vtx[vIndex++].normal = nFront;

    vtx[vIndex].position = { br.x, br.y, br.z, 1.0f };
    vtx[vIndex].texcoord = { 1.0f, 1.0f };
    vtx[vIndex++].normal = nFront;

    vtx[vIndex].position = { bl.x, bl.y, bl.z, 1.0f };
    vtx[vIndex].texcoord = { 0.0f, 1.0f };
    vtx[vIndex++].normal = nFront;

    // 右面 (Right)
    vtx[vIndex].position = { top.x, top.y, top.z, 1.0f };
    vtx[vIndex].texcoord = { 0.5f, 0.0f };
    vtx[vIndex++].normal = nRight;

    vtx[vIndex].position = { tr.x, tr.y, tr.z, 1.0f };
    vtx[vIndex].texcoord = { 1.0f, 1.0f };
    vtx[vIndex++].normal = nRight;

    vtx[vIndex].position = { br.x, br.y, br.z, 1.0f };
    vtx[vIndex].texcoord = { 0.0f, 1.0f };
    vtx[vIndex++].normal = nRight;

    // 背面 (Back)
    vtx[vIndex].position = { top.x, top.y, top.z, 1.0f };
    vtx[vIndex].texcoord = { 0.5f, 0.0f };
    vtx[vIndex++].normal = nBack;

    vtx[vIndex].position = { tl.x, tl.y, tl.z, 1.0f };
    vtx[vIndex].texcoord = { 1.0f, 1.0f };
    vtx[vIndex++].normal = nBack;

    vtx[vIndex].position = { tr.x, tr.y, tr.z, 1.0f };
    vtx[vIndex].texcoord = { 0.0f, 1.0f };
    vtx[vIndex++].normal = nBack;

    // 左面 (Left)
    vtx[vIndex].position = { top.x, top.y, top.z, 1.0f };
    vtx[vIndex].texcoord = { 0.5f, 0.0f };
    vtx[vIndex++].normal = nLeft;

    vtx[vIndex].position = { bl.x, bl.y, bl.z, 1.0f };
    vtx[vIndex].texcoord = { 1.0f, 1.0f };
    vtx[vIndex++].normal = nLeft;

    vtx[vIndex].position = { tl.x, tl.y, tl.z, 1.0f };
    vtx[vIndex].texcoord = { 0.0f, 1.0f };
    vtx[vIndex++].normal = nLeft;

    // 底面 (Bottom)
    uint32_t bottomBase = vIndex;
    vtx[vIndex].position = { bl.x, bl.y, bl.z, 1.0f };
    vtx[vIndex].texcoord = { 0.0f, 1.0f };
    vtx[vIndex++].normal = nBottom;

    vtx[vIndex].position = { br.x, br.y, br.z, 1.0f };
    vtx[vIndex].texcoord = { 1.0f, 1.0f };
    vtx[vIndex++].normal = nBottom;

    vtx[vIndex].position = { tr.x, tr.y, tr.z, 1.0f };
    vtx[vIndex].texcoord = { 1.0f, 0.0f };
    vtx[vIndex++].normal = nBottom;

    vtx[vIndex].position = { tl.x, tl.y, tl.z, 1.0f };
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

    idxGPU[iIndex++] = bottomBase + 0;
    idxGPU[iIndex++] = bottomBase + 1;
    idxGPU[iIndex++] = bottomBase + 2;

    idxGPU[iIndex++] = bottomBase + 0;
    idxGPU[iIndex++] = bottomBase + 2;
    idxGPU[iIndex++] = bottomBase + 3;

    indexResource_->Unmap(0, nullptr);
}
