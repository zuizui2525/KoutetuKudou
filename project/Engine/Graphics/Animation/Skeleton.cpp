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

void ApplyAnimationBlend(
    Skeleton& skeleton,
    const Animation& animA, float timeA,
    const Animation& animB, float timeB,
    float blendFactor
) {
    constexpr float kMinFactor = 0.0f;
    constexpr float kMaxFactor = 1.0f;
    float clampedFactor = (std::max)(kMinFactor, (std::min)(blendFactor, kMaxFactor));

    for (Joint& joint : skeleton.joints) {
        auto itA = animA.nodeAnimations.find(joint.name);
        auto itB = animB.nodeAnimations.find(joint.name);

        bool hasA = (itA != animA.nodeAnimations.end());
        bool hasB = (itB != animB.nodeAnimations.end());

        if (hasA && hasB) {
            Vector3 transA = AnimationHelper::CalculateValue(itA->second.translate, timeA);
            Quaternion rotA = AnimationHelper::CalculateValue(itA->second.rotate, timeA);
            Vector3 sclA = AnimationHelper::CalculateValue(itA->second.scale, timeA);

            Vector3 transB = AnimationHelper::CalculateValue(itB->second.translate, timeB);
            Quaternion rotB = AnimationHelper::CalculateValue(itB->second.rotate, timeB);
            Vector3 sclB = AnimationHelper::CalculateValue(itB->second.scale, timeB);

            joint.transform.translate = Math::Lerp(transA, transB, clampedFactor);
            joint.transform.rotate = Math::Slerp(rotA, rotB, clampedFactor);
            joint.transform.scale = Math::Lerp(sclA, sclB, clampedFactor);
        } else if (hasA) {
            joint.transform.translate = AnimationHelper::CalculateValue(itA->second.translate, timeA);
            joint.transform.rotate = AnimationHelper::CalculateValue(itA->second.rotate, timeA);
            joint.transform.scale = AnimationHelper::CalculateValue(itA->second.scale, timeA);
        } else if (hasB) {
            joint.transform.translate = AnimationHelper::CalculateValue(itB->second.translate, timeB);
            joint.transform.rotate = AnimationHelper::CalculateValue(itB->second.rotate, timeB);
            joint.transform.scale = AnimationHelper::CalculateValue(itB->second.scale, timeB);
        }
    }
}

std::optional<Matrix4x4> GetJointSkeletonMatrix(const Skeleton& skeleton, const std::string& jointName) {
    auto it = skeleton.jointMap.find(jointName);
    if (it != skeleton.jointMap.end()) {
        int32_t index = it->second;
        if (index >= 0 && index < static_cast<int32_t>(skeleton.joints.size())) {
            return skeleton.joints[index].skeletonSpaceMatrix;
        }
    }
    return std::nullopt;
}
