#include "Engine/Graphics/Objects/3d/Skydome/Skydome.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Zuizui.h"
#include "Engine/Math/Matrix/Matrix.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Graphics/Objects/3d/Model/ModelManager.h"
#include "Engine/Graphics/PSO/Manager/PSOManager.h"
#include "Engine/Base/DeferredRelease/DeferredReleaseManager.h"
#ifdef _USEIMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
    // 定数定義 (マジックナンバー排除)
    constexpr size_t kConstantBufferAlignment = 0xff;
    constexpr UINT kRootParamIndexWVP = 0;
    constexpr UINT kRootParamIndexTexture = 1;
    constexpr UINT kVertexBufferSlot = 0;
    constexpr UINT kNumVertexBuffers = 1;
    constexpr UINT kInstanceCount = 1;
    constexpr UINT kStartInstanceLocation = 0;
    constexpr UINT kStartIndexLocation = 0;
    constexpr INT kBaseVertexLocation = 0;

    const std::string kPsoKeySkydome = "Skydome";
    const std::string kDefaultSkydomeKey = "skydome";
}

Skydome::~Skydome() {
    auto deferredMgr = DeferredReleaseManager::GetInstance();
    if (deferredMgr) {
        if (wvpResource_) {
            deferredMgr->Enqueue(std::move(wvpResource_));
        }
    }
}

void Skydome::Initialize(const std::string& modelKey) {
    modelKey_ = modelKey.empty() ? kDefaultSkydomeKey : modelKey;
    auto device = EngineResource::GetEngine()->GetDevice();

    // WVP用の定数バッファ作成 (DirectX12の256バイトアライメントに準拠)
    constexpr size_t kConstantBufferSize = (sizeof(WVPData) + kConstantBufferAlignment) & ~kConstantBufferAlignment;
    wvpResource_ = DxUtils::CreateBufferResource(device, kConstantBufferSize);
    wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpData_));
    wvpData_->WVP = Math::MakeIdentity();
    wvpData_->World = Math::MakeIdentity();
    wvpData_->color = color_;
}

void Skydome::SetColor(const Vector4& color) {
    color_ = color;
    if (wvpData_) {
        wvpData_->color = color_;
    }
}

void Skydome::Update() {
    if (!isActive_) return;

    // 1. 自転更新 (Y軸まわりの回転)
    transform_.rotate.y += rotationSpeed_;

    // 2. カメラ追従 (カメラのワールド位置を取得して天球の中心に同期)
    auto camera = CameraResource::GetCameraManager();
    if (camera) {
        Matrix4x4 view = camera->GetViewMatrix3D();
        Matrix4x4 viewInv = Math::Inverse(view);
        transform_.translate = { viewInv.m[3][0], viewInv.m[3][1], viewInv.m[3][2] };

        // 3. 行列計算
        Matrix4x4 world = Math::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
        Matrix4x4 projection = camera->GetProjectionMatrix3D();
        Matrix4x4 wvp = Math::Multiply(Math::Multiply(world, view), projection);

        if (wvpData_) {
            wvpData_->WVP = wvp;
            wvpData_->World = world;
            wvpData_->color = color_;
        }
    }
}

void Skydome::Draw(const std::string& textureKey) {
    if (!isActive_) return;

    auto modelMgr = ModelResource::GetModelManager();
    if (!modelMgr) return;

    auto modelData = modelMgr->GetModelData(modelKey_);
    if (!modelData || modelData->vertices.empty()) return;

    auto engine = EngineResource::GetEngine();
    if (!engine) return;

    auto dxCommon = engine->GetDxCommon();
    if (!dxCommon) return;

    auto commandList = dxCommon->GetCommandList();
    auto psoMgr = engine->GetPSOManager();
    if (!commandList || !psoMgr) return;

    // PSOとRootSignatureの設定
    commandList->SetGraphicsRootSignature(psoMgr->GetRootSignature(kPsoKeySkydome));
    commandList->SetPipelineState(psoMgr->GetPSO(kPsoKeySkydome));

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(kVertexBufferSlot, kNumVertexBuffers, &modelData->vbv);

    // 定数バッファ (b0) の設定
    commandList->SetGraphicsRootConstantBufferView(kRootParamIndexWVP, wvpResource_->GetGPUVirtualAddress());

    // テクスチャ (t0) の設定
    std::string validTextureKey = textureKey.empty() ? modelKey_ : textureKey;
    auto texMgr = TextureResource::GetTextureManager();
    if (texMgr) {
        auto handle = texMgr->GetGpuHandle(validTextureKey);
        if (handle.ptr != 0) {
            commandList->SetGraphicsRootDescriptorTable(kRootParamIndexTexture, handle);
        }
    }

    // インデックス描画または頂点描画
    if (!modelData->indices.empty() && modelData->indexResource) {
        commandList->IASetIndexBuffer(&modelData->ibv);
        commandList->DrawIndexedInstanced(
            static_cast<UINT>(modelData->indices.size()),
            kInstanceCount,
            kStartIndexLocation,
            kBaseVertexLocation,
            kStartInstanceLocation
        );
    } else {
        commandList->DrawInstanced(
            static_cast<UINT>(modelData->vertices.size()),
            kInstanceCount,
            0,
            kStartInstanceLocation
        );
    }
}

void Skydome::DrawInspector() {
#ifdef _USEIMGUI
    if (ImGui::TreeNode("天球設定###SkydomeSettings")) {
        ImGui::Checkbox("有効###Active", &isActive_);
        ImGui::ColorEdit4("天球カラー###Color", &color_.x);
        SetColor(color_);
        ImGui::DragFloat("自転速度###RotSpeed", &rotationSpeed_, 0.0001f, -0.1f, 0.1f, "%.5f");
        ImGui::DragFloat3("スケール###Scale", &transform_.scale.x, 10.0f, 10.0f, 10000.0f);
        ImGui::TreePop();
    }
#endif
}
