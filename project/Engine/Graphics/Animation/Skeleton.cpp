#include "Engine/Graphics/Animation/Skeleton.h"
#include "Engine/Math/Matrix/Matrix.h"

int32_t CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints) {
    Joint joint;
    joint.name = node.name;
    joint.localMatrix = node.localMatrix;
    joint.skeletonSpaceMatrix = Math::MakeIdentity();
    joint.transform = node.transform;
    joint.index = static_cast<int32_t>(joints.size());
    joint.parent = parent;

    joints.push_back(joint);

    for (const Node& child : node.children) {
        int32_t childIndex = CreateJoint(child, joint.index, joints);
        joints[joint.index].children.push_back(childIndex);
    }

    return joint.index;
}

Skeleton CreateSkeleton(const Node& rootNode) {
    Skeleton skeleton;
    skeleton.root = CreateJoint(rootNode, std::nullopt, skeleton.joints);

    // 名前とindexのマッピングを行いアクセスしやすくする
    for (const Joint& joint : skeleton.joints) {
        skeleton.jointMap.emplace(joint.name, joint.index);
    }

    // 初期ポーズで全Jointの行列を更新しておく
    Update(skeleton);

    return skeleton;
}

void Update(Skeleton& skeleton) {
    // すべてのJointを更新。親が若いので通常ループで処理可能
    for (Joint& joint : skeleton.joints) {
        joint.localMatrix = Math::MakeAffineMatrix(joint.transform.scale, joint.transform.rotate, joint.transform.translate);
        if (joint.parent) {
            joint.skeletonSpaceMatrix = joint.localMatrix * skeleton.joints[*joint.parent].skeletonSpaceMatrix;
        } else {
            joint.skeletonSpaceMatrix = joint.localMatrix;
        }
    }
}

void ApplyAnimation(Skeleton& skeleton, const Animation& animation, float animationTime) {
    for (Joint& joint : skeleton.joints) {
        if (auto it = animation.nodeAnimations.find(joint.name); it != animation.nodeAnimations.end()) {
            const NodeAnimation& nodeAnimation = it->second;
            joint.transform.translate = AnimationHelper::CalculateValue(nodeAnimation.translate, animationTime);
            joint.transform.rotate = AnimationHelper::CalculateValue(nodeAnimation.rotate, animationTime);
            joint.transform.scale = AnimationHelper::CalculateValue(nodeAnimation.scale, animationTime);
        }
    }
}
