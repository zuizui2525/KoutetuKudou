#include "Engine/Graphics/Objects/3d/Model/ModelObject.h"
#include "Engine/Graphics/Objects/3d/Drawer/Object3DDrawer.h"
#include "Engine/Zuizui.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Light/Manager/LightManager.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Graphics/Objects/3d/Model/ModelManager.h"
#include "Engine/Math/Matrix/Matrix.h"
#include <cassert>

void ModelObject::Initialize(int lightingMode) {
    Object3D::Initialize(lightingMode);
}

void ModelObject::Update() {
    Matrix4x4 worldMatrix = Math::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    Matrix4x4 viewMatrix = CameraResource::GetCameraManager()->GetViewMatrix3D();
    Matrix4x4 projectionMatrix = CameraResource::GetCameraManager()->GetProjectionMatrix3D();
    Matrix4x4 wvpMatrix = Math::Multiply(worldMatrix, Math::Multiply(viewMatrix, projectionMatrix));

    // 法線用行列（逆転置）
    Matrix4x4 worldForNormal = worldMatrix;
    worldForNormal.m[3][0] = 0.0f; worldForNormal.m[3][1] = 0.0f; worldForNormal.m[3][2] = 0.0f;

    wvpData_->WVP = wvpMatrix;
    wvpData_->world = worldMatrix;
    wvpData_->WorldInverseTranspose = Math::Transpose(Math::Inverse(worldForNormal));
}

void ModelObject::Draw(const std::string& modelKey, const std::string& textureKey, const std::string& envMapKey) {
    if (!isVisible_ || modelKey.empty()) return;

    auto modelData = sModelMgr->GetModelData(modelKey);
    if (!modelData) return;

    std::string finalTextureKey = textureKey;
    if (finalTextureKey.empty()) {
        finalTextureKey = modelKey;
    }

    // 描画処理は共通描画クラス Object3DDrawer を通して実行
    uint32_t vertexCount = static_cast<uint32_t>(modelData->vertices.size());
    Object3DDrawer::GetInstance()->Draw(this, modelData->vbv, vertexCount, finalTextureKey, envMapKey);
}
