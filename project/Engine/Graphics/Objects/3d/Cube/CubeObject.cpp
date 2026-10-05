#include "Engine/Graphics/Objects/3d/Cube/CubeObject.h"
#include "Engine/Graphics/Objects/3d/Drawer/Object3DDrawer.h"
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

    Object3D::Update();
}

void CubeObject::Draw(const std::string& textureKey, const std::string& envMapKey) {
    // 描画処理は共通描画クラス Object3DDrawer を通して実行
    Object3DDrawer::GetInstance()->DrawIndexed(this, vbView_, ibView_, kIndexCount, textureKey, envMapKey);
}

void CubeObject::CreateMesh() {

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
