#include "Engine/Graphics/Animation/Animation.h"
#include "Engine/Math/Matrix/Matrix.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <cassert>
#include <filesystem>

namespace AnimationHelper {

Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time) {
	assert(!keyframes.empty());

	constexpr size_t kSingleKeyCount = 1;
	constexpr size_t kFirstIndex = 0;
	if (keyframes.size() == kSingleKeyCount || time <= keyframes[kFirstIndex].time) {
		return keyframes[kFirstIndex].value;
	}

	for (size_t index = 0; index < keyframes.size() - 1; ++index) {
		size_t nextIndex = index + 1;
		if (keyframes[index].time <= time && time <= keyframes[nextIndex].time) {
			float range = keyframes[nextIndex].time - keyframes[index].time;
			constexpr float kEpsilon = 1e-5f;
			float t = (range > kEpsilon) ? ((time - keyframes[index].time) / range) : 0.0f;
			return Math::Lerp(keyframes[index].value, keyframes[nextIndex].value, t);
		}
	}

	return (*keyframes.rbegin()).value;
}

Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time) {
	assert(!keyframes.empty());

	constexpr size_t kSingleKeyCount = 1;
	constexpr size_t kFirstIndex = 0;
	if (keyframes.size() == kSingleKeyCount || time <= keyframes[kFirstIndex].time) {
		return keyframes[kFirstIndex].value;
	}

	for (size_t index = 0; index < keyframes.size() - 1; ++index) {
		size_t nextIndex = index + 1;
		if (keyframes[index].time <= time && time <= keyframes[nextIndex].time) {
			float range = keyframes[nextIndex].time - keyframes[index].time;
			constexpr float kEpsilon = 1e-5f;
			float t = (range > kEpsilon) ? ((time - keyframes[index].time) / range) : 0.0f;
			return Math::Slerp(keyframes[index].value, keyframes[nextIndex].value, t);
		}
	}

	return (*keyframes.rbegin()).value;
}

Animation LoadAnimationFile(const std::string& directoryPath, const std::string& filename) {
	std::string filePath = directoryPath.empty() ? filename : (directoryPath + "/" + filename);
	return LoadAnimationFile(filePath);
}

Animation LoadAnimationFile(const std::string& filePath) {
	Animation animation{};
	Assimp::Importer importer;

	constexpr uint32_t kFlags = 0;
	const aiScene* scene = importer.ReadFile(filePath.c_str(), kFlags);
	assert(scene != nullptr);
	assert(scene->mNumAnimations != 0);

	constexpr uint32_t kPrimaryAnimationIndex = 0;
	aiAnimation* animationAssimp = scene->mAnimations[kPrimaryAnimationIndex];

	// 時間単位を秒に変換 (mTicksPerSecond が 0 の場合はデフォルト 24.0f)
	constexpr double kDefaultTicksPerSecond = 24.0;
	double ticksPerSecond = (animationAssimp->mTicksPerSecond != 0.0)
		? animationAssimp->mTicksPerSecond
		: kDefaultTicksPerSecond;

	animation.duration = static_cast<float>(animationAssimp->mDuration / ticksPerSecond);

	// 各ノード（ボーン・メッシュ）のアニメーションチャンネルを走査
	for (uint32_t channelIndex = 0; channelIndex < animationAssimp->mNumChannels; ++channelIndex) {
		aiNodeAnim* nodeAnimationAssimp = animationAssimp->mChannels[channelIndex];
		NodeAnimation& nodeAnimation = animation.nodeAnimations[nodeAnimationAssimp->mNodeName.C_Str()];

		// 1. 平行移動 (Translate) の抽出 & 右手系->左手系変換 (-x)
		for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumPositionKeys; ++keyIndex) {
			aiVectorKey& keyAssimp = nodeAnimationAssimp->mPositionKeys[keyIndex];
			KeyframeVector3 keyframe{};
			keyframe.time = static_cast<float>(keyAssimp.mTime / ticksPerSecond);
			keyframe.value = { -keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z };
			nodeAnimation.translate.push_back(keyframe);
		}

		// 2. 回転 (Rotate: Quaternion) の抽出 & 右手系->左手系変換 (y, z反転)
		for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumRotationKeys; ++keyIndex) {
			aiQuatKey& keyAssimp = nodeAnimationAssimp->mRotationKeys[keyIndex];
			KeyframeQuaternion keyframe{};
			keyframe.time = static_cast<float>(keyAssimp.mTime / ticksPerSecond);
			keyframe.value = { keyAssimp.mValue.x, -keyAssimp.mValue.y, -keyAssimp.mValue.z, keyAssimp.mValue.w };
			nodeAnimation.rotate.push_back(keyframe);
		}

		// 3. 拡大縮小 (Scale) の抽出 (そのまま)
		for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumScalingKeys; ++keyIndex) {
			aiVectorKey& keyAssimp = nodeAnimationAssimp->mScalingKeys[keyIndex];
			KeyframeVector3 keyframe{};
			keyframe.time = static_cast<float>(keyAssimp.mTime / ticksPerSecond);
			keyframe.value = { keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z };
			nodeAnimation.scale.push_back(keyframe);
		}
	}

	return animation;
}

} // namespace AnimationHelper
