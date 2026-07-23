#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "AnimationClip.h"
#include "utils/math/MathUtils.h"

struct Joint {
    QuaternionTransform transform{};
    QuaternionTransform bindTransform{};
    Matrix4x4 localMatrix{};
    Matrix4x4 skeletonSpaceMatrix{};
    std::string name;
    std::vector<int32_t> children;
    int32_t index = 0;
    std::optional<int32_t> parent;
};

struct Skeleton {
    int32_t root = 0;
    std::map<std::string, int32_t> jointMap;
    std::vector<Joint> joints;
};

Skeleton CreateSkeleton(const Node& rootNode);
void ApplyAnimation(Skeleton& skeleton, const AnimationClip& animation, float animationTime);
void ApplyAnimationBlend(Skeleton& skeleton,
    const AnimationClip& fromAnimation, float fromTime,
    const AnimationClip& toAnimation, float toTime, float blendAlpha);
void UpdateSkeleton(Skeleton& skeleton);

// Matrices in this renderer use row-vector convention. This is the row-vector
// equivalent of Root^-1 * JointGlobal * InverseBind used by column-vector
// renderers: InverseBind * JointGlobal * RootInverse.
Matrix4x4 BuildMeshSpaceSkinningMatrix(
    const Matrix4x4& inverseBindPoseMatrix,
    const Matrix4x4& jointGlobalMatrix,
    const Matrix4x4& meshRootInverseMatrix);

// Assimp bone offsets transform mesh-local positions into bind-pose joint
// space. Therefore InverseBind * BindJointGlobal resolves the actual mesh
// root basis, which is not necessarily the skeleton hierarchy root.
bool TryBuildMeshRootBindMatrix(
    const Skeleton& skeleton,
    const ModelData& model,
    Matrix4x4& outMeshRootBindMatrix,
    float* outMaximumDeviation = nullptr);
