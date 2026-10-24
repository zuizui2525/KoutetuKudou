#pragma once
#include "Engine/Math/MathStructs.h"
#include <vector>
#include <string>
#include <map>

template <typename tValue>
struct Keyframe {
	float time;
	tValue value;
};

using KeyframeVector3 = Keyframe<Vector3>;
using KeyframeQuaternion = Keyframe<Quaternion>;

struct NodeAnimation {
	std::vector<KeyframeVector3> translate;
	std::vector<KeyframeQuaternion> rotate;
	std::vector<KeyframeVector3> scale;
};

struct Animation {
	float duration = 0.0f; // アニメーション全体の尺 (単位: 秒)
	std::map<std::string, NodeAnimation> nodeAnimations;
};

/// @brief キーフレームアニメーション関連のヘルパー関数群
namespace AnimationHelper {
	/// @brief 任意時刻のVector3キーフレーム補間値を計算
	Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time);

	/// @brief 任意時刻のQuaternionキーフレーム補間値を計算
	Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time);

	/// @brief ディレクトリパスとファイル名を指定してアニメーションファイルを読み込む
	Animation LoadAnimationFile(const std::string& directoryPath, const std::string& filename);

	/// @brief ファイルパスを直接指定してアニメーションファイルを読み込む
	Animation LoadAnimationFile(const std::string& filePath);
}
