#pragma once
#include "Engine/Math/MathStructs.h"
#include "Engine/Graphics/RenderStructs.h"
#include "Engine/Graphics/Animation/Animation.h"
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <cstdint>

/// <summary>
/// 骨格を構成する個々の関節（ボーン）情報
/// </summary>
struct Joint {
    QuaternionTransform transform;       // Transform情報
    Matrix4x4 localMatrix;              // localMatrix
    Matrix4x4 skeletonSpaceMatrix;      // skeletonSpaceでの変換行列
    std::string name;                   // 名前
    std::vector<int32_t> children;      // 子JointのIndexリスト
    int32_t index = 0;                  // 自身のIndex
    std::optional<int32_t> parent;      // 親JointのIndex
};

/// <summary>
/// 全関節の階層構造を保持・管理する骨格構造体
/// </summary>
struct Skeleton {
    int32_t root = 0;                         // RootJointのIndex
    std::map<std::string, int32_t> jointMap;  // Joint名とIndexとの辞書
    std::vector<Joint> joints;                // 所属している全Jointの配列
};

/// <summary>
/// Node階層からSkeletonを生成する
/// </summary>
Skeleton CreateSkeleton(const Node& rootNode);

/// <summary>
/// 深さ優先探索でNodeからJointを生成する再帰関数
/// </summary>
int32_t CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints);

/// <summary>
/// 全JointのlocalMatrixおよびskeletonSpaceMatrixを更新する
/// </summary>
void Update(Skeleton& skeleton);

/// <summary>
/// アニメーションデータをSkeletonの各Jointに反映する
/// </summary>
void ApplyAnimation(Skeleton& skeleton, const Animation& animation, float animationTime);
