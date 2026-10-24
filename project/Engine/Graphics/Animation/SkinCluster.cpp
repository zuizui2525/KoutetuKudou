#include "Engine/Graphics/Animation/SkinCluster.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Zuizui.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Math/Matrix/Matrix.h"
#include <cstring>
#include <cassert>

namespace {
	constexpr float kZeroWeight = 0.0f;
	constexpr float kFullWeight = 1.0f;
	constexpr int32_t kDefaultJointIndex = 0;
	constexpr UINT kFirstElement = 0;
}

SkinCluster CreateSkinCluster(
	ID3D12Device* device,
	const Skeleton& skeleton,
	const ModelData& modelData
) {
	SkinCluster skinCluster;

	if (skeleton.joints.empty() || modelData.vertices.empty()) {
		return skinCluster;
	}

	// 1. 各ジョイントの逆バインドポーズ行列の領域確保 & 単位行列で初期化
	skinCluster.inverseBindPoseMatrices.resize(skeleton.joints.size(), Math::MakeIdentity());

	// 2. 頂点インフルエンス（Slot 1 VBV）用のGPUリソース作成
	size_t influenceBufferSize = sizeof(VertexInfluence) * modelData.vertices.size();
	skinCluster.influenceResource = DxUtils::CreateBufferResource(device, influenceBufferSize);
	if (!skinCluster.influenceResource) {
		return skinCluster;
	}

	skinCluster.influenceBufferView.BufferLocation = skinCluster.influenceResource->GetGPUVirtualAddress();
	skinCluster.influenceBufferView.SizeInBytes = static_cast<UINT>(influenceBufferSize);
	skinCluster.influenceBufferView.StrideInBytes = sizeof(VertexInfluence);

	VertexInfluence* mappedInfluence = nullptr;
	skinCluster.influenceResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedInfluence));
	std::memset(mappedInfluence, 0, influenceBufferSize);
	skinCluster.mappedInfluence = { mappedInfluence, modelData.vertices.size() };

	// 3. マトリックスパレット（StructuredBuffer）用のGPUリソース作成
	size_t paletteBufferSize = sizeof(WellForGPU) * skeleton.joints.size();
	skinCluster.paletteResource = DxUtils::CreateBufferResource(device, paletteBufferSize);
	if (!skinCluster.paletteResource) {
		return skinCluster;
	}

	WellForGPU* mappedPalette = nullptr;
	skinCluster.paletteResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedPalette));
	skinCluster.mappedPalette = { mappedPalette, skeleton.joints.size() };

	// 4. マトリックスパレット用のSRVデスクリプタ作成
	auto texMgr = TextureResource::GetTextureManager();
	auto engine = EngineResource::GetEngine();
	if (texMgr && engine && engine->GetDxCommon()) {
		skinCluster.paletteDescriptorIndex = texMgr->AllocateDescriptorIndex();
		ID3D12DescriptorHeap* srvHeap = engine->GetDxCommon()->GetSrvHeap();
		UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		skinCluster.paletteSrvHandleCPU = DxUtils::GetCPUDescriptorHandle(srvHeap, descriptorSize, skinCluster.paletteDescriptorIndex);
		skinCluster.paletteSrvHandleGPU = DxUtils::GetGPUDescriptorHandle(srvHeap, descriptorSize, skinCluster.paletteDescriptorIndex);

		D3D12_SHADER_RESOURCE_VIEW_DESC paletteSrvDesc{};
		paletteSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
		paletteSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		paletteSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		paletteSrvDesc.Buffer.FirstElement = kFirstElement;
		paletteSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		paletteSrvDesc.Buffer.NumElements = static_cast<UINT>(skeleton.joints.size());
		paletteSrvDesc.Buffer.StructureByteStride = sizeof(WellForGPU);

		device->CreateShaderResourceView(skinCluster.paletteResource.Get(), &paletteSrvDesc, skinCluster.paletteSrvHandleCPU);
	}

	// 5. ボーン情報・頂点ウェイトの割り当て
	for (const auto& [jointName, jointWeightData] : modelData.skinClusterData) {
		auto it = skeleton.jointMap.find(jointName);
		if (it == skeleton.jointMap.end()) {
			continue;
		}
		uint32_t jointIndex = it->second;
		skinCluster.inverseBindPoseMatrices[jointIndex] = jointWeightData.inverseBindPoseMatrix;

		for (const auto& vertexWeight : jointWeightData.vertexWeights) {
			if (vertexWeight.vertexIndex >= modelData.vertices.size()) {
				continue;
			}
			auto& influence = skinCluster.mappedInfluence[vertexWeight.vertexIndex];
			for (uint32_t index = 0; index < kNumMaxInfluence; ++index) {
				if (influence.weights[index] == kZeroWeight) {
					influence.weights[index] = vertexWeight.weight;
					influence.jointIndices[index] = static_cast<int32_t>(jointIndex);
					break;
				}
			}
		}
	}

	// 6. ウェイトの正規化 (4つの合計が 1.0f になるように調整)
	for (auto& influence : skinCluster.mappedInfluence) {
		float totalWeight = 0.0f;
		for (uint32_t i = 0; i < kNumMaxInfluence; ++i) {
			totalWeight += influence.weights[i];
		}

		if (totalWeight > kZeroWeight) {
			for (uint32_t i = 0; i < kNumMaxInfluence; ++i) {
				influence.weights[i] /= totalWeight;
			}
		} else {
			// ウェイトのない頂点はルートジョイントに追従させてクラッシュを防止
			influence.weights[0] = kFullWeight;
			influence.jointIndices[0] = kDefaultJointIndex;
		}
	}

	skinCluster.isValid = true;
	return skinCluster;
}

void UpdateSkinCluster(SkinCluster& skinCluster, const Skeleton& skeleton) {
	if (!skinCluster.isValid || skinCluster.mappedPalette.empty()) {
		return;
	}

	for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex) {
		assert(jointIndex < skinCluster.inverseBindPoseMatrices.size());
		assert(jointIndex < skinCluster.mappedPalette.size());

		// 合成行列 = 逆バインドポーズ行列 * 現在のスケルトン空間行列
		Matrix4x4 skeletonSpaceMatrix = Math::Multiply(
			skinCluster.inverseBindPoseMatrices[jointIndex],
			skeleton.joints[jointIndex].skeletonSpaceMatrix
		);

		skinCluster.mappedPalette[jointIndex].skeletonSpaceMatrix = skeletonSpaceMatrix;
		skinCluster.mappedPalette[jointIndex].skeletonSpaceInverseTransposeMatrix =
			Math::Transpose(Math::Inverse(skeletonSpaceMatrix));
	}
}
