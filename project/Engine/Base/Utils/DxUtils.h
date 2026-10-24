#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <cstdint>
#include "Engine/Math/MathStructs.h"

namespace DxUtils {
    // バッファリソースを作成する関数
    [[nodiscard]]
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(ID3D12Device* device, size_t sizeInBytes);

    // ディスクリプタヒープの生成
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

    // DepthStencilTextureの生成
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(ID3D12Device* device, int32_t width, int32_t height);

    // RenderTexture用テクスチャリソースの生成
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateRenderTextureResource(
        ID3D12Device* device,
        uint32_t width,
        uint32_t height,
        DXGI_FORMAT format,
        const Vector4& clearColor);

    // ディスクリプタヒープの先頭から指定したインデックスのCPUディスクリプタハンドルを取得する
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index);

    // ディスクリプタヒープの先頭から指定したインデックスのGPUディスクリプタハンドルを取得する
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index);

    // Compute Shader等で使用するUAV（Unordered Access View）対応のDefaultバッファリソースを生成
    [[nodiscard]]
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateUAVBufferResource(
        ID3D12Device* device,
        size_t sizeInBytes,
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON);

    // 構造化バッファ（StructuredBuffer）に対するUAV（Unordered Access View）を作成
    void CreateBufferUAV(
        ID3D12Device* device,
        ID3D12Resource* resource,
        uint32_t numElements,
        uint32_t structureByteStride,
        D3D12_CPU_DESCRIPTOR_HANDLE destHandle);
}
