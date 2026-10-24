#pragma once
#include "Engine/Math/MathStructs.h"
#include "Engine/Graphics/Animation/Skeleton.h"
#include "Engine/Graphics/RenderStructs.h"
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <array>
#include <span>
#include <cstdint>

// 頂点1つあたりの最大影響ボーン数 (マジックナンバー排除)
constexpr uint32_t kNumMaxInfluence = 4;

// 頂点ごとのボーン影響データ (InputLayout Slot 1 用)
struct VertexInfluence {
    std::array<float, kNumMaxInfluence> weights{};
    std::array<int32_t, kNumMaxInfluence> jointIndices{};
};

// マトリックスパレットの1要素 (GPU StructuredBuffer 用)
struct WellForGPU {
    Matrix4x4 skeletonSpaceMatrix;
    Matrix4x4 skeletonSpaceInverseTransposeMatrix;
};

// スキンクラスタ構造体
struct SkinCluster {
    std::vector<Matrix4x4> inverseBindPoseMatrices;
    Microsoft::WRL::ComPtr<ID3D12Resource> influenceResource;
    D3D12_VERTEX_BUFFER_VIEW influenceBufferView{};
    std::span<VertexInfluence> mappedInfluence;
    Microsoft::WRL::ComPtr<ID3D12Resource> paletteResource;
    std::span<WellForGPU> mappedPalette;
    uint32_t paletteDescriptorIndex = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE paletteSrvHandleCPU{};
    D3D12_GPU_DESCRIPTOR_HANDLE paletteSrvHandleGPU{};
    bool isValid = false;
};

// スキンクラスタの生成
SkinCluster CreateSkinCluster(
    ID3D12Device* device,
    const Skeleton& skeleton,
    const ModelData& modelData
);

// 毎フレームのスキンクラスタ更新 (マトリックスパレットの更新)
void UpdateSkinCluster(SkinCluster& skinCluster, const Skeleton& skeleton);
