#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>

/// <summary>
/// DirectX 12 コンピュートシェーダー（Compute Shader）用のパイプライン状態とルートシグネチャを管理するクラス
/// </summary>
class ComputePipeline {
public:
    ComputePipeline() = default;
    ~ComputePipeline() = default;

    /// <summary>
    /// ルートシグネチャとコンピュートシェーダーバイトコードからコンピュートパイプライン状態（Compute PSO）を生成します。
    /// </summary>
    /// <param name="device">DirectX 12デバイス</param>
    /// <param name="rootSignature">パイプラインにバインドするルートシグネチャ</param>
    /// <param name="csBytecode">コンパイル済みのCSバイトコード</param>
    /// <returns>生成成功時 true</returns>
    bool Initialize(
        ID3D12Device* device,
        ID3D12RootSignature* rootSignature,
        D3D12_SHADER_BYTECODE csBytecode);

    /// <summary>
    /// コマンドリストへコンピュートパイプラインとルートシグネチャを設定します。
    /// </summary>
    /// <param name="commandList">コマンドリスト</param>
    void Bind(ID3D12GraphicsCommandList* commandList) const;

    ID3D12PipelineState* GetPSO() const { return pipelineState_.Get(); }
    ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
};
