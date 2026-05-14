#include "Skeleton.h"

namespace {

int32_t CreateJoint(
    const Node& node,
    std::optional<int32_t> parent,
    std::vector<Joint>& joints) {
    Joint joint{};
    joint.transform = node.transform;
    joint.bindTransform = node.transform;
    joint.localMatrix = node.localMatrix;
    joint.skeletonSpaceMatrix = MakeIdentity4x4();
    joint.name = node.name;
    joint.index = static_cast<int32_t>(joints.size());
    joint.parent = parent;
    joints.push_back(joint);

    for (const Node& child : node.children) {
        const int32_t childIndex = CreateJoint(child, joint.index, joints);
        joints[static_cast<size_t>(joint.index)].children.push_back(childIndex);
    }

    return joint.index;
}

Matrix4x4 MakeLocalMatrix(const QuaternionTransform& transform) {
    return MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
}

} // namespace

Skeleton CreateSkeleton(const Node& rootNode) {
    Skeleton skeleton{};
    skeleton.root = CreateJoint(rootNode, std::nullopt, skeleton.joints);
    for (const Joint& joint : skeleton.joints) {
        skeleton.jointMap.emplace(joint.name, joint.index);
    }
    return skeleton;
}

void ApplyAnimation(Skeleton& skeleton, const AnimationClip& animation, float animationTime) {
    for (Joint& joint : skeleton.joints) {
        joint.transform = joint.bindTransform;

        const auto found = animation.nodeAnimations.find(joint.name);
        if (found != animation.nodeAnimations.end()) {
            const NodeAnimation& nodeAnimation = found->second;
            joint.transform.translate =
                CalculateValue(nodeAnimation.translate, animationTime, joint.bindTransform.translate);
            joint.transform.rotate =
                CalculateValue(nodeAnimation.rotate, animationTime, joint.bindTransform.rotate);
            joint.transform.scale =
                CalculateValue(nodeAnimation.scale, animationTime, joint.bindTransform.scale);
        }

        joint.localMatrix = MakeLocalMatrix(joint.transform);
    }
}

void UpdateSkeleton(Skeleton& skeleton) {
    for (Joint& joint : skeleton.joints) {
        if (joint.parent.has_value()) {
            const Joint& parent = skeleton.joints[static_cast<size_t>(*joint.parent)];
            joint.skeletonSpaceMatrix = Multiply(joint.localMatrix, parent.skeletonSpaceMatrix);
        } else {
            joint.skeletonSpaceMatrix = joint.localMatrix;
        }
    }
}
