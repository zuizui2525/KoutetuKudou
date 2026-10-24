#include "Engine/Graphics/PSO/Compute/ComputePipeline.h"
#include <cassert>

bool ComputePipeline::Initialize(
    ID3D12Device* device,
    ID3D12RootSignature* rootSignature,
    D3D12_SHADER_BYTECODE csBytecode) {
    assert(device != nullptr);
    assert(rootSignature != nullptr);
    assert(csBytecode.pShaderBytecode != nullptr && csBytecode.BytecodeLength > 0);

    rootSignature_ = rootSignature;

    D3D12_COMPUTE_PIPELINE_STATE_DESC computeDesc{};
    computeDesc.pRootSignature = rootSignature_.Get();
    computeDesc.CS = csBytecode;
    computeDesc.NodeMask = 0;
    computeDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

    HRESULT hr = device->CreateComputePipelineState(&computeDesc, IID_PPV_ARGS(&pipelineState_));
    if (FAILED(hr)) {
        assert(false && "Failed to create Compute Pipeline State!");
        return false;
    }

    return true;
}

void ComputePipeline::Bind(ID3D12GraphicsCommandList* commandList) const {
    assert(commandList != nullptr);
    assert(pipelineState_ != nullptr);
    assert(rootSignature_ != nullptr);

    commandList->SetComputeRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
}
