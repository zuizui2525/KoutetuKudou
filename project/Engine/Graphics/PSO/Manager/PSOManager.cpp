#include "Engine/Graphics/PSO/Manager/PSOManager.h"
#include "Engine/Base/Log/Log.h"
#include "Engine/Base/Utils/StringUtility.h"
#include <cassert>
#include <format>

PSOManager::PSOManager(ID3D12Device* device)
    : device_(device) {
}

HRESULT PSOManager::Initialize(
    IDxcUtils* dxcUtils,
    IDxcCompiler3* dxcCompiler,
    IDxcIncludeHandler* includeHandler) {

    // Object3D用プリセット作成
    PSOPreset object3DPreset = PSOPreset::CreateObject3DPreset(
        device_, dxcUtils, dxcCompiler, includeHandler);

    // 登録処理
    RegisterPreset("Object3D", object3DPreset);

    // Object3D_Alpha用プリセット作成
    PSOPreset object3DAlphaPreset = PSOPreset::CreateObject3DAlphaPreset(
        device_, dxcUtils, dxcCompiler, includeHandler);
    RegisterPreset("Object3D_Alpha", object3DAlphaPreset);

    // Particle用プリセット作成
    PSOPreset particlePreset = PSOPreset::CreateParticlePreset(
        device_, dxcUtils, dxcCompiler, includeHandler);

    // 登録処理
    RegisterPreset("Particle", particlePreset);

    // Skybox用プリセット作成
    PSOPreset skyboxPreset = PSOPreset::CreateSkyboxPreset(
        device_, dxcUtils, dxcCompiler, includeHandler);

    // 登録処理
    RegisterPreset("Skybox", skyboxPreset);

    // Object2D用プリセット作成
    PSOPreset object2DPreset = PSOPreset::CreateObject2DPreset(
        device_, dxcUtils, dxcCompiler, includeHandler);

    // 登録処理
    RegisterPreset("Object2D", object2DPreset);

    return S_OK;
}

void PSOManager::RegisterPreset(const std::string& name, const PSOPreset& preset) {
    assert(device_);

    // -------------------------------------------------------
    // ★修正ポイント: マップへの参照を使って直接操作する
    // -------------------------------------------------------
    // ローカル変数で作ってから代入すると、コピー発生時にポインタがずれるため、
    // 最初からマップ内のメモリ領域を使用します。
    PSOData& data = psoMap_[name];

    // 1. データのコピー (ここで vector<string> などのメモリ位置が確定する)
    data.originalPreset = preset;
    data.rootSignature = preset.rootSignature;

    // 2. ★確定したメモリ位置にある Builder を使って InputLayout を再構築する★
    // これにより、InputLayoutDesc.pInputElementDescs が指す文字列ポインタが
    // 「data.originalPreset 内部の文字列」を正しく指すようになります。
    data.originalPreset.inputLayoutDesc = data.originalPreset.ilBuilder_.Build();

    // -------------------------------------------------------
    // PSO設定 (dataの内容を使用)
    // -------------------------------------------------------
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};

    desc.pRootSignature = data.rootSignature.Get();
    desc.InputLayout = data.originalPreset.inputLayoutDesc; // ← 再構築済みの正しいDesc

    // シェーダー取得とチェック
    desc.VS = data.originalPreset.shaderProgram.GetVS();
    desc.PS = data.originalPreset.shaderProgram.GetPS();

    // もしコンパイル失敗などで中身が空ならアサート
    if (!desc.VS.pShaderBytecode || desc.VS.BytecodeLength == 0) {
        assert(false && "Invalid VS Bytecode. CreateGraphicsPipelineState will crash.");
    }
    if (!desc.PS.pShaderBytecode || desc.PS.BytecodeLength == 0) {
        assert(false && "Invalid PS Bytecode. CreateGraphicsPipelineState will crash.");
    }

    desc.BlendState = data.originalPreset.blendDesc;
    desc.RasterizerState = data.originalPreset.rasterizerDesc;
    desc.DepthStencilState = data.originalPreset.depthStencilDesc;

    // 固定設定
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.SampleDesc.Count = 1;

    // PSO生成
    HRESULT hr = device_->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&data.pso));

    if (FAILED(hr)) {
        assert(false && "Failed to create GraphicsPipelineState. Check Output Window for D3D12 Errors.");
    }

    Log::Write(std::format(L" ├─ 【PSO生成完了】 プリセット名:「{}」 のグラフィックスパイプラインおよびルートシグネチャを生成しました。", ConvertString(name)));
}

HRESULT PSOManager::UpdateBlendMode(const std::string& name, BlendMode mode) {
    // マップに存在するかチェック
    auto it = psoMap_.find(name);
    if (it == psoMap_.end()) {
        return E_FAIL;
    }

    PSOData& data = it->second;

    // ブレンドステートのみ作り直す
    BlendStateBuilder blendBuilder;
    blendBuilder.SetBlendMode(mode);
    D3D12_BLEND_DESC newBlendDesc = blendBuilder.Build();

    // 新しい設定を作成
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = data.rootSignature.Get();
    desc.InputLayout = data.originalPreset.inputLayoutDesc; // 既存の正しいLayoutを使用
    desc.VS = data.originalPreset.shaderProgram.GetVS();
    desc.PS = data.originalPreset.shaderProgram.GetPS();

    desc.BlendState = newBlendDesc; // ★ここだけ更新

    desc.RasterizerState = data.originalPreset.rasterizerDesc;
    desc.DepthStencilState = data.originalPreset.depthStencilDesc;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.SampleDesc.Count = 1;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> newPso;
    HRESULT hr = device_->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&newPso));

    if (SUCCEEDED(hr)) {
        // 成功したら差し替え
        data.pso = newPso;
        data.originalPreset.blendDesc = newBlendDesc; // プリセット情報も更新しておく
        Log::Write(std::format(L" ├─ 【PSOブレンド更新】 プリセット:「{}」 のブレンドモードを「{}」に変更しました。", ConvertString(name), (int)mode));
        return S_OK;
    }

    return hr;
}

ID3D12PipelineState* PSOManager::GetPSO(const std::string& name) const {
    auto it = psoMap_.find(name);
    if (it != psoMap_.end()) {
        return it->second.pso.Get();
    }
    return nullptr;
}

ID3D12RootSignature* PSOManager::GetRootSignature(const std::string& name) const {
    auto it = psoMap_.find(name);
    if (it != psoMap_.end()) {
        return it->second.rootSignature.Get();
    }
    return nullptr;
}
