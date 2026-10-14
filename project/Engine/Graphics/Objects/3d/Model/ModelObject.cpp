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
    // 基底クラスObject3DのUpdateを実行（親行列parentMatrix_やTransformの乗算を確実に行う）
    Object3D::Update();
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
