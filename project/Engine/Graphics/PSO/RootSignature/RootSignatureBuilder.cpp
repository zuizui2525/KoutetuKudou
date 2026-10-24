#include "Engine/Graphics/PSO/RootSignature/RootSignatureBuilder.h"

RootSignatureBuilder::RootSignatureBuilder() {
    rootParams_.clear();
    samplers_.clear();
    descriptors_.clear();
    ranges_.clear();
}

void RootSignatureBuilder::AddCBV(UINT shaderRegister, D3D12_SHADER_VISIBILITY visibility) {
    // CBVは "Root Constant Buffer View" (アドレス直渡し) でOK
    descriptors_.push_back({});
    descriptors_.back().ShaderRegister = shaderRegister;
    descriptors_.back().RegisterSpace = 0;

    D3D12_ROOT_PARAMETER param{};
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    param.ShaderVisibility = visibility;
    param.Descriptor = descriptors_.back();

    rootParams_.push_back(param);
}

void RootSignatureBuilder::AddSRV(UINT shaderRegister, D3D12_SHADER_VISIBILITY visibility) {
    // ★修正: テクスチャを使う場合、TYPE_SRV ではなく TYPE_DESCRIPTOR_TABLE が必須！

    // 1. レンジ（範囲）を作成
    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
    range.NumDescriptors = 1; // 1つだけ
    range.BaseShaderRegister = shaderRegister; // t0, t1...
    range.RegisterSpace = 0;
    range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // ベクターに保存 (Build時にポインタを使用するため)
    ranges_.push_back(range);

    // 2. ルートパラメータをテーブルとして設定
    D3D12_ROOT_PARAMETER param{};
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ← ここが重要
    param.ShaderVisibility = visibility;
    param.DescriptorTable.NumDescriptorRanges = 1;
    // pDescriptorRanges の設定は Build() で行う (vectorの再確保でアドレスが変わるのを防ぐため)
    param.DescriptorTable.pDescriptorRanges = nullptr;

    rootParams_.push_back(param);
}

void RootSignatureBuilder::AddUAV(UINT shaderRegister, D3D12_SHADER_VISIBILITY visibility) {
    // 1. レンジ（範囲）を作成
    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; // UAVを使う
    range.NumDescriptors = 1;
    range.BaseShaderRegister = shaderRegister; // u0, u1...
    range.RegisterSpace = 0;
    range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // ベクターに保存 (Build時にポインタを使用するため)
    ranges_.push_back(range);

    // 2. ルートパラメータをテーブルとして設定
    D3D12_ROOT_PARAMETER param{};
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    param.ShaderVisibility = visibility;
    param.DescriptorTable.NumDescriptorRanges = 1;
    param.DescriptorTable.pDescriptorRanges = nullptr;

    rootParams_.push_back(param);
}

void RootSignatureBuilder::AddSampler(const D3D12_SAMPLER_DESC& samplerDesc, UINT shaderRegister) {
    D3D12_STATIC_SAMPLER_DESC s{};
    s.Filter = samplerDesc.Filter;
    s.AddressU = samplerDesc.AddressU;
    s.AddressV = samplerDesc.AddressV;
    s.AddressW = samplerDesc.AddressW;
    s.MipLODBias = samplerDesc.MipLODBias;
    s.MaxAnisotropy = samplerDesc.MaxAnisotropy;
    s.ComparisonFunc = samplerDesc.ComparisonFunc;
    s.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    s.MinLOD = samplerDesc.MinLOD;
    s.MaxLOD = samplerDesc.MaxLOD;
    s.ShaderRegister = shaderRegister;
    s.RegisterSpace = 0;
    s.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    samplers_.push_back(s);
}

Microsoft::WRL::ComPtr<ID3D12RootSignature> RootSignatureBuilder::Build(
    ID3D12Device* device,
    D3D12_ROOT_SIGNATURE_FLAGS flags) {
    // ★重要: Descriptor Table のポインタ設定
    // ranges_ vector にデータが溜まった状態で、各パラメータに正しいアドレスをセットする
    int rangeIndex = 0;
    for (auto& param : rootParams_) {
        if (param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
            param.DescriptorTable.pDescriptorRanges = &ranges_[rangeIndex];
            rangeIndex++;
        }
    }

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.NumParameters = static_cast<UINT>(rootParams_.size());
    desc.pParameters = rootParams_.data();
    desc.NumStaticSamplers = static_cast<UINT>(samplers_.size());
    desc.pStaticSamplers = samplers_.data();
    desc.Flags = flags;

    Microsoft::WRL::ComPtr<ID3DBlob> signature;
    Microsoft::WRL::ComPtr<ID3DBlob> error;

    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
    if (FAILED(hr)) {
        if (error) {
            OutputDebugStringA(reinterpret_cast<char*>(error->GetBufferPointer()));
        }
        return nullptr;
    }

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    hr = device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));

    if (FAILED(hr)) {
        return nullptr;
    }

    return rootSignature;
}
