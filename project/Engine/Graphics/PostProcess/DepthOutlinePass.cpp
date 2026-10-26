#include "Engine/Graphics/PostProcess/DepthOutlinePass.h"
#include "Engine/Graphics/PSO/RootSignature/RootSignatureBuilder.h"
#include "Engine/Graphics/PSO/BlendState/BlendStateBuilder.h"
#include "Engine/Graphics/PSO/RasterizerState/RasterizerStateBuilder.h"
#include "Engine/Graphics/PSO/DepthStencilState/DepthStencilStateBuilder.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Zuizui.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include <cassert>

#ifdef _USEIMGUI
#include <imgui.h>
#endif

namespace {
    // マジックナンバー排除のための定数定義
    constexpr UINT kReservedSrvIndexDepth = TextureManager::kReservedDepthOutlineIndex; // 深度用SRVのヒープ予約インデックス
    
    // デフォルトパラメータ
    const Vector3 kDefaultEdgeColor = { 0.0f, 0.0f, 0.0f }; // 黒
    constexpr float kDefaultEdgeWidth = 1.0f;
    constexpr float kDefaultThreshold = 0.01f;
    constexpr float kDefaultScale = 6.0f;

    // ルートパラメータインデックス
    constexpr UINT kRootParamIndexSRV = 0;       // t0: カラーテクスチャ
    constexpr UINT kRootParamIndexDepthSRV = 1;  // t1: 深度テクスチャ
    constexpr UINT kRootParamIndexCBV = 2;       // b0: 定数バッファ
}

void DepthOutlinePass::Initialize(ID3D12Device* device) {
    Zuizui* engine = Zuizui::GetInstance();
    DxCommon* dxCommon = engine->GetDxCommon();

    // 1. 定数バッファの作成と初期化
    paramsResource_ = DxUtils::CreateBufferResource(device, sizeof(DepthOutlineParams));
    assert(paramsResource_ != nullptr && "Failed to create paramsResource in DepthOutlinePass!");

    HRESULT hr = paramsResource_->Map(0, nullptr, reinterpret_cast<void**>(&paramsData_));
    assert(SUCCEEDED(hr) && paramsData_ != nullptr && "Failed to map paramsResource in DepthOutlinePass!");

    // デフォルト値をセット
    paramsData_->projectionInverse = Math::MakeIdentity();
    paramsData_->edgeColor = kDefaultEdgeColor;
    paramsData_->edgeWidth = kDefaultEdgeWidth;
    paramsData_->threshold = kDefaultThreshold;
    paramsData_->scale = kDefaultScale;

    // 2. 深度用SRVの作成
    ID3D12DescriptorHeap* srvHeap = dxCommon->GetSrvHeap();
    UINT srvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    depthSrvCpu_ = srvHeap->GetCPUDescriptorHandleForHeapStart();
    depthSrvGpu_ = srvHeap->GetGPUDescriptorHandleForHeapStart();

    depthSrvCpu_.ptr += kReservedSrvIndexDepth * srvDescriptorSize;
    depthSrvGpu_.ptr += kReservedSrvIndexDepth * srvDescriptorSize;

    D3D12_SHADER_RESOURCE_VIEW_DESC depthTextureSrvDesc{};
    depthTextureSrvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    depthTextureSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    depthTextureSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    depthTextureSrvDesc.Texture2D.MipLevels = 1;

    device->CreateShaderResourceView(
        dxCommon->GetDepthStencilResource(),
        &depthTextureSrvDesc,
        depthSrvCpu_
    );

    lastDepthResource_ = dxCommon->GetDepthStencilResource();

    // 3. RootSignature (カラーSRV: t0, 深度SRV: t1, 定数バッファ: b0)
    RootSignatureBuilder rs;
    rs.AddSRV(0, D3D12_SHADER_VISIBILITY_PIXEL); // t0: カラー
    rs.AddSRV(1, D3D12_SHADER_VISIBILITY_PIXEL); // t1: 深度
    rs.AddCBV(0, D3D12_SHADER_VISIBILITY_PIXEL); // b0: 定数バッファ

    // s0: gSampler (Linear, Clamp)
    D3D12_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    rs.AddSampler(sampler, 0);

    // s1: gSamplerPoint (Point, Clamp)
    D3D12_SAMPLER_DESC samplerPoint{};
    samplerPoint.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    samplerPoint.AddressU = samplerPoint.AddressV = samplerPoint.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    rs.AddSampler(samplerPoint, 1);

    rootSignature_ = rs.Build(device);
    assert(rootSignature_ && "DepthOutlinePass RootSignature creation failed!");

    // 4. シェーダーコンパイル
    bool vsResult = shaderProgram_.CompileVS(
        L"resources/Shader/Fullscreen/Fullscreen.VS.hlsl",
        dxCommon->GetDxcUtils(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler()
    );
    assert(vsResult && "DepthOutlinePass VS Compile Failed!");

    bool psResult = shaderProgram_.CompilePS(
        L"resources/Shader/Fullscreen/DepthOutline.PS.hlsl",
        dxCommon->GetDxcUtils(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler()
    );
    assert(psResult && "DepthOutlinePass PS Compile Failed!");

    // 5. Pipeline State (PSO)
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = rootSignature_.Get();
    desc.InputLayout.pInputElementDescs = nullptr;
    desc.InputLayout.NumElements = 0;
    desc.VS = shaderProgram_.GetVS();
    desc.PS = shaderProgram_.GetPS();

    BlendStateBuilder blendBuilder;
    blendBuilder.SetBlendMode(kBlendModeNone);
    desc.BlendState = blendBuilder.Build();

    RasterizerStateBuilder rsb;
    rsb.SetCullMode(CullMode::None);
    desc.RasterizerState = rsb.Build();

    DepthStencilStateBuilder dsb;
    dsb.SetDepthEnable(false);
    desc.DepthStencilState = dsb.GetDesc();

    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.SampleDesc.Count = 1;

    hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr) && "Failed to create DepthOutlinePass GraphicsPipelineState!");
}

void DepthOutlinePass::Draw(ID3D12GraphicsCommandList* commandList, D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) {
    Zuizui* engine = Zuizui::GetInstance();
    DxCommon* dxCommon = engine->GetDxCommon();
    ID3D12Resource* depthResource = dxCommon->GetDepthStencilResource();

    // 深度リソースが再生成（リサイズ）された場合はSRVを更新
    if (depthResource != lastDepthResource_) {
        D3D12_SHADER_RESOURCE_VIEW_DESC depthTextureSrvDesc{};
        depthTextureSrvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        depthTextureSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        depthTextureSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        depthTextureSrvDesc.Texture2D.MipLevels = 1;

        engine->GetDevice()->CreateShaderResourceView(
            depthResource,
            &depthTextureSrvDesc,
            depthSrvCpu_
        );
        lastDepthResource_ = depthResource;
    }

    // 1. 深度リソースを読み取り可能な状態に遷移 (DEPTH_WRITE -> PIXEL_SHADER_RESOURCE)
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = depthResource;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    commandList->ResourceBarrier(1, &barrier);

    // 2. カメラのプロジェクション逆行列を更新して定数バッファへコピー
    CameraManager* cameraMgr = CameraResource::GetCameraManager();
    if (cameraMgr && cameraMgr->GetActiveCamera()) {
        Matrix4x4 proj = cameraMgr->GetActiveCamera()->GetProjectionMatrix();
        paramsData_->projectionInverse = Math::Inverse(proj);
    } else {
        paramsData_->projectionInverse = Math::MakeIdentity();
    }

    // 3. パイプライン・ルートシグネチャ・リソースのバインド
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    
    // t0: カラーテクスチャ
    commandList->SetGraphicsRootDescriptorTable(kRootParamIndexSRV, inputSRV);
    // t1: 深度テクスチャ
    commandList->SetGraphicsRootDescriptorTable(kRootParamIndexDepthSRV, depthSrvGpu_);
    // b0: パラメータ定数バッファ
    commandList->SetGraphicsRootConstantBufferView(kRootParamIndexCBV, paramsResource_->GetGPUVirtualAddress());

    // 4. 全画面描画
    constexpr UINT kVertexCount = 3;
    constexpr UINT kInstanceCount = 1;
    constexpr UINT kStartVertexLocation = 0;
    constexpr UINT kStartInstanceLocation = 0;
    commandList->DrawInstanced(kVertexCount, kInstanceCount, kStartVertexLocation, kStartInstanceLocation);

    // 5. 深度リソースの状態を元に戻す (PIXEL_SHADER_RESOURCE -> DEPTH_WRITE)
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    commandList->ResourceBarrier(1, &barrier);
}

void DepthOutlinePass::ImGuiControl() {
#ifdef _USEIMGUI
    if (ImGui::TreeNode("DepthOutline")) {
        ImGui::DragFloat("Edge Width", &paramsData_->edgeWidth, 0.05f, 0.1f, 10.0f, "%.2f");
        ImGui::DragFloat("Threshold", &paramsData_->threshold, 0.005f, 0.0f, 1.0f, "%.3f");
        ImGui::DragFloat("Scale (Sensitivity)", &paramsData_->scale, 0.1f, 0.1f, 50.0f, "%.1f");

        float color[3] = { paramsData_->edgeColor.x, paramsData_->edgeColor.y, paramsData_->edgeColor.z };
        if (ImGui::ColorEdit3("Edge Color", color)) {
            paramsData_->edgeColor = { color[0], color[1], color[2] };
        }

        ImGui::TreePop();
    }
#endif
}
