#include "Engine/Graphics/Objects/3d/Drawer/Object3DDrawer.h"
#include "Engine/Graphics/Objects/3d/Object3D.h"
#include "Engine/Zuizui.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Light/Manager/LightManager.h"
#include "Engine/Graphics/Texture/TextureManager.h"

namespace {
    // ルートシグネチャの各パラメータインデックス (マジックナンバー排除)
    constexpr UINT kRootParamIndexWVP = 0;
    constexpr UINT kRootParamIndexMaterial = 1;
    constexpr UINT kRootParamIndexCamera = 2;
    constexpr UINT kRootParamIndexDirLight = 3;
    constexpr UINT kRootParamIndexPointLight = 4;
    constexpr UINT kRootParamIndexSpotLight = 5;
    constexpr UINT kRootParamIndexTexture = 6;
    constexpr UINT kRootParamIndexEnvMap = 7;

    // 描画関連の定数 (マジックナンバー排除)
    constexpr UINT kDefaultInstanceCount = 1;
    constexpr UINT kStartInstanceLocation = 0;
    constexpr UINT kStartVertexLocation = 0;
    constexpr INT kBaseVertexLocation = 0;
    constexpr UINT kStartIndexLocation = 0;
    constexpr UINT kVertexBufferSlot = 0;
    constexpr UINT kNumViews = 1;

    // 環境マップ関連の定数
    const std::string kDefaultSkyboxTexKey = "skyboxTex";
    constexpr float kDefaultEnvCoefficient = 1.0f;
    constexpr float kZeroEnvCoefficient = 0.0f;

    // PSOキー関連の定数
    const std::string kPsoKeyLine = "Object3D_Line";
}

Object3DDrawer* Object3DDrawer::GetInstance() {
    static Object3DDrawer instance;
    return &instance;
}

bool Object3DDrawer::PrepareDraw(
    Object3D* object,
    const D3D12_VERTEX_BUFFER_VIEW& vbView,
    const std::string& textureKey,
    const std::string& envMapKey,
    const std::string& psoKey,
    ID3D12GraphicsCommandList*& outCommandList
) {
    if (!object || !object->GetIsVisible()) {
        return false;
    }

    auto engine = EngineResource::GetEngine();
    if (!engine) return false;

    auto dxCommon = engine->GetDxCommon();
    if (!dxCommon) return false;

    auto psoMgr = engine->GetPSOManager();
    if (!psoMgr) return false;

    outCommandList = dxCommon->GetCommandList();
    if (!outCommandList) return false;

    // パイプライン・ルートシグネチャの設定
    outCommandList->SetGraphicsRootSignature(psoMgr->GetRootSignature(psoKey));
    outCommandList->SetPipelineState(psoMgr->GetPSO(psoKey));

    // トポロジー設定 (Object3D_Line の場合は LINELIST、それ以外は TRIANGLELIST)
    if (psoKey == kPsoKeyLine) {
        outCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    } else {
        outCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }

    // 頂点バッファ設定
    outCommandList->IASetVertexBuffers(kVertexBufferSlot, kNumViews, &vbView);

    // 定数バッファ設定 (WVP, Material, Camera)
    outCommandList->SetGraphicsRootConstantBufferView(kRootParamIndexWVP, object->GetWVPResource()->GetGPUVirtualAddress());
    outCommandList->SetGraphicsRootConstantBufferView(kRootParamIndexMaterial, object->GetMaterialResource()->GetGPUVirtualAddress());

    auto cameraMgr = CameraResource::GetCameraManager();
    if (cameraMgr) {
        outCommandList->SetGraphicsRootConstantBufferView(kRootParamIndexCamera, cameraMgr->GetGPUVirtualAddress());
    }

    // ライティング定数バッファ設定
    auto lightMgr = LightResource::GetLightManager();
    if (lightMgr) {
        outCommandList->SetGraphicsRootConstantBufferView(kRootParamIndexDirLight, lightMgr->GetDirectionalLightGroupAddress());
        outCommandList->SetGraphicsRootConstantBufferView(kRootParamIndexPointLight, lightMgr->GetPointLightGroupAddress());
        outCommandList->SetGraphicsRootConstantBufferView(kRootParamIndexSpotLight, lightMgr->GetSpotLightGroupAddress());
    }

    // テクスチャ設定
    auto texMgr = TextureResource::GetTextureManager();
    if (texMgr) {
        constexpr const char* kFallbackWhiteKey = "white";
        std::string validTextureKey = textureKey.empty() ? kFallbackWhiteKey : textureKey;
        auto texHandle = texMgr->GetGpuHandle(validTextureKey);

        // 万一ハンドルが無効な場合はクラッシュ防止のため描画を安全に中断
        if (texHandle.ptr == 0) {
            texHandle = texMgr->GetGpuHandle(kFallbackWhiteKey);
            if (texHandle.ptr == 0) {
                return false;
            }
        }
        outCommandList->SetGraphicsRootDescriptorTable(kRootParamIndexTexture, texHandle);

        // 環境マップテクスチャ設定
        auto materialData = object->GetMaterialData();
        auto defaultEnvHandle = texMgr->GetGpuHandle(kDefaultSkyboxTexKey);

        if (!envMapKey.empty()) {
            auto envHandle = texMgr->GetGpuHandle(envMapKey);
            // 存在しないキーの場合は安全にskyboxTexにフォールバック
            if (envHandle.ptr == 0) {
                envHandle = defaultEnvHandle;
            }

            if (envHandle.ptr != 0) {
                if (materialData && materialData->environmentCoefficient == kZeroEnvCoefficient) {
                    materialData->environmentCoefficient = kDefaultEnvCoefficient;
                }
                outCommandList->SetGraphicsRootDescriptorTable(kRootParamIndexEnvMap, envHandle);
            }
        } else {
            if (materialData) {
                materialData->environmentCoefficient = kZeroEnvCoefficient;
            }
            if (defaultEnvHandle.ptr != 0) {
                outCommandList->SetGraphicsRootDescriptorTable(kRootParamIndexEnvMap, defaultEnvHandle);
            }
        }
    }

    return true;
}

void Object3DDrawer::Draw(
    Object3D* object,
    const D3D12_VERTEX_BUFFER_VIEW& vbView,
    uint32_t vertexCount,
    const std::string& textureKey,
    const std::string& envMapKey,
    const std::string& psoKey
) {
    ID3D12GraphicsCommandList* commandList = nullptr;
    if (!PrepareDraw(object, vbView, textureKey, envMapKey, psoKey, commandList)) {
        return;
    }

    commandList->DrawInstanced(vertexCount, kDefaultInstanceCount, kStartVertexLocation, kStartInstanceLocation);

    // ライン描画を行った場合は、後続の描画（ポストプロセス等）への影響を防ぐため標準の TRIANGLELIST に復元
    if (psoKey == kPsoKeyLine) {
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }
}

void Object3DDrawer::DrawIndexed(
    Object3D* object,
    const D3D12_VERTEX_BUFFER_VIEW& vbView,
    const D3D12_INDEX_BUFFER_VIEW& ibView,
    uint32_t indexCount,
    const std::string& textureKey,
    const std::string& envMapKey,
    const std::string& psoKey
) {
    ID3D12GraphicsCommandList* commandList = nullptr;
    if (!PrepareDraw(object, vbView, textureKey, envMapKey, psoKey, commandList)) {
        return;
    }

    commandList->IASetIndexBuffer(&ibView);
    commandList->DrawIndexedInstanced(indexCount, kDefaultInstanceCount, kStartIndexLocation, kBaseVertexLocation, kStartInstanceLocation);

    // ライン描画を行った場合は、後続の描画（ポストプロセス等）への影響を防ぐため標準の TRIANGLELIST に復元
    if (psoKey == kPsoKeyLine) {
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }
}
