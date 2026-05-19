#include "camera/VfxCamera.h"

#include <algorithm>

void VfxCamera::Initialize(
    const Transform& transform,
    float fovY,
    float aspectRatio,
    float nearZ,
    float farZ) {
    transform_ = transform;
    SetLens(fovY, aspectRatio, nearZ, farZ);
}

void VfxCamera::SetTransform(const Transform& transform) {
    transform_ = transform;
    UpdateMatrices();
}

void VfxCamera::SetLens(float fovY, float aspectRatio, float nearZ, float farZ) {
    fovY_ = (std::max)(0.01f, fovY);
    aspectRatio_ = (std::max)(0.01f, aspectRatio);
    nearZ_ = (std::max)(0.001f, nearZ);
    farZ_ = (std::max)(nearZ_ + 0.001f, farZ);
    UpdateMatrices();
}

void VfxCamera::UpdateMatrices() {
    Matrix4x4 worldMatrix = MakeAffineMatrix(
        transform_.scale,
        transform_.rotate,
        transform_.translate);
    viewMatrix_ = Inverse(worldMatrix);
    projectionMatrix_ = MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearZ_, farZ_);
    viewProjectionMatrix_ = Multiply(viewMatrix_, projectionMatrix_);
}
