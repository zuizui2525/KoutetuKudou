#include "Engine/Graphics/Objects/2d/Drawer/Object2DDrawer.h"
#include "Engine/Zuizui.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Graphics/Texture/TextureManager.h"

namespace {
    // ルートシグネチャのパラメータインデックス (マジックナンバー排除)
    constexpr UINT kRootParamIndexWVP = 0;
    constexpr UINT kRootParamIndexMaterial = 1;
    constexpr UINT kRootParamIndexTexture = 2;

    // 描画関連定数 (マジックナンバー排除)
    constexpr UINT kDefaultInstanceCount = 1;
    constexpr UINT kStartIndexLocation = 0;
    constexpr INT kBaseVertexLocation = 0;
    constexpr UINT kStartInstanceLocation = 0;
    constexpr UINT kVertexBufferSlot = 0;
    constexpr UINT kNumViews = 1;
}

Object2DDrawer* Object2DDrawer::GetInstance() {
    static Object2DDrawer instance;
    return &instance;
}

void Object2DDrawer::DrawIndexed(
    ID3D12Resource* wvpResource,
    ID3D12Resource* materialResource,
    const D3D12_VERTEX_BUFFER_VIEW& vbView,
    const D3D12_INDEX_BUFFER_VIEW& ibView,
    uint32_t indexCount,
    const std::string& textureKey,
    bool isVisible,
    const std::string& psoKey
) {
    if (!isVisible || !wvpResource || !materialResource) return;

    auto engine = EngineResource::GetEngine();
    if (!engine) return;

    auto dxCommon = engine->GetDxCommon();
    if (!dxCommon) return;

    auto psoMgr = engine->GetPSOManager();
    if (!psoMgr) return;

    auto commandList = dxCommon->GetCommandList();
    if (!commandList) return;

    auto texMgr = TextureResource::GetTextureManager();
    if (!texMgr) return;

    // パイプライン・ルートシグネチャ設定
    commandList->SetGraphicsRootSignature(psoMgr->GetRootSignature(psoKey));
    commandList->SetPipelineState(psoMgr->GetPSO(psoKey));

    // 頂点・インデックスバッファ設定
    commandList->IASetVertexBuffers(kVertexBufferSlot, kNumViews, &vbView);
    commandList->IASetIndexBuffer(&ibView);

    // 定数バッファ設定 (WVP, Material)
    commandList->SetGraphicsRootConstantBufferView(kRootParamIndexWVP, wvpResource->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(kRootParamIndexMaterial, materialResource->GetGPUVirtualAddress());

    // テクスチャ設定
    commandList->SetGraphicsRootDescriptorTable(kRootParamIndexTexture, texMgr->GetGpuHandle(textureKey));

    // 描画コール
    commandList->DrawIndexedInstanced(indexCount, kDefaultInstanceCount, kStartIndexLocation, kBaseVertexLocation, kStartInstanceLocation);
}
