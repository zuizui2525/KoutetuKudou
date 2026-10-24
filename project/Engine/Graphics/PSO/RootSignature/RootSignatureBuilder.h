#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>

class RootSignatureBuilder {
public:
    RootSignatureBuilder();

    // ルートパラメータ追加系
    // CBVは通常 SetGraphicsRootConstantBufferView で渡すのでそのままでOK
    void AddCBV(UINT shaderRegister, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);

    // ★修正: テクスチャ用に Descriptor Table として設定するように変更
    void AddSRV(UINT shaderRegister, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);

    // Compute Shader等で使用するUAV（Unordered Access View）を Descriptor Table として設定
    void AddUAV(UINT shaderRegister, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);

    void AddSampler(const D3D12_SAMPLER_DESC& samplerDesc, UINT shaderRegister = 0);

    // ルートシグネチャ生成（Compute用等でフラグを変更可能）
    Microsoft::WRL::ComPtr<ID3D12RootSignature> Build(
        ID3D12Device* device,
        D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

private:
    std::vector<D3D12_ROOT_PARAMETER> rootParams_;
    std::vector<D3D12_STATIC_SAMPLER_DESC> samplers_;

    // メモリ保持用
    std::vector<D3D12_ROOT_DESCRIPTOR> descriptors_;

    // ★追加: Descriptor Table を使うために範囲指定データを保持する必要がある
    std::vector<D3D12_DESCRIPTOR_RANGE> ranges_;
};
