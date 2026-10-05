#include "Engine/Graphics/PostProcess/VignettePass.h"
#include "Engine/Graphics/PSO/RootSignature/RootSignatureBuilder.h"
#include "Engine/Graphics/PSO/BlendState/BlendStateBuilder.h"
#include "Engine/Graphics/PSO/RasterizerState/RasterizerStateBuilder.h"
#include "Engine/Graphics/PSO/DepthStencilState/DepthStencilStateBuilder.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Zuizui.h"
#include <cassert>

#ifdef _USEIMGUI
#include <imgui.h>
#endif

namespace {
    // マジックナンバー排除用の定数
    const float kDefaultVignetteScale = 16.0f;
    const float kDefaultVignetteExponent = 0.8f;
    constexpr UINT kRootParamIndexSRV = 0;
    constexpr UINT kRootParamIndexCBV = 1;
}

void VignettePass::Initialize(ID3D12Device* device) {
    Zuizui* engine = Zuizui::GetInstance();
    DxCommon* dxCommon = engine->GetDxCommon();

    // 1. 定数バッファの作成と初期化
    vignetteResource_ = DxUtils::CreateBufferResource(device, sizeof(VignetteParams));
    assert(vignetteResource_ != nullptr && "Failed to create vignetteResource_ in VignettePass!");

    HRESULT hr = vignetteResource_->Map(0, nullptr, reinterpret_cast<void**>(&vignetteData_));
    assert(SUCCEEDED(hr) && vignetteData_ != nullptr && "Failed to map vignetteResource_ in VignettePass!");

    vignetteData_->scale = kDefaultVignetteScale;
    vignetteData_->exponent = kDefaultVignetteExponent;

    // 2. RootSignature
    RootSignatureBuilder rs;
    rs.AddSRV(0, D3D12_SHADER_VISIBILITY_PIXEL); // t0 (PS)
    rs.AddCBV(0, D3D12_SHADER_VISIBILITY_PIXEL); // b0 (PS)

    D3D12_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    rs.AddSampler(sampler, 0);

    rootSignature_ = rs.Build(device);
    assert(rootSignature_ && "VignettePass RootSignature creation failed!");

    // 3. Shader Compile
    bool vsResult = shaderProgram_.CompileVS(
        L"resources/Shader/Fullscreen/Fullscreen.VS.hlsl",
        dxCommon->GetDxcUtils(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler()
    );
    assert(vsResult && "VignettePass VS Compile Failed!");

    bool psResult = shaderProgram_.CompilePS(
        L"resources/Shader/Fullscreen/Vignette.PS.hlsl",
        dxCommon->GetDxcUtils(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler()
    );
    assert(psResult && "VignettePass PS Compile Failed!");

    // 4. Pipeline State (PSO)
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
    assert(SUCCEEDED(hr) && "Failed to create VignettePass GraphicsPipelineState!");
}

void VignettePass::Draw(ID3D12GraphicsCommandList* commandList, D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) {
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->SetGraphicsRootDescriptorTable(kRootParamIndexSRV, inputSRV);
    commandList->SetGraphicsRootConstantBufferView(kRootParamIndexCBV, vignetteResource_->GetGPUVirtualAddress());

    constexpr UINT kVertexCount = 3;
    constexpr UINT kInstanceCount = 1;
    constexpr UINT kStartVertexLocation = 0;
    constexpr UINT kStartInstanceLocation = 0;
    commandList->DrawInstanced(kVertexCount, kInstanceCount, kStartVertexLocation, kStartInstanceLocation);
}

void VignettePass::ImGuiControl() {
#ifdef _USEIMGUI
    if (ImGui::TreeNode("Vignette")) {
        ImGui::DragFloat("Scale", &vignetteData_->scale, 0.1f, 0.0f, 100.0f, "%.1f");
        ImGui::DragFloat("Exponent", &vignetteData_->exponent, 0.01f, 0.0f, 10.0f, "%.2f");
        ImGui::TreePop();
    }
#endif
}
