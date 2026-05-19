#pragma once

#include "utils/math/MathUtils.h"

class VfxCamera {
public:
    void Initialize(
        const Transform& transform,
        float fovY,
        float aspectRatio,
        float nearZ,
        float farZ);

    void SetTransform(const Transform& transform);
    void SetLens(float fovY, float aspectRatio, float nearZ, float farZ);
    void UpdateMatrices();

    const Transform& GetTransform() const { return transform_; }
    const Vector3& GetPosition() const { return transform_.translate; }
    const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }
    const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }
    const Matrix4x4& GetViewProjectionMatrix() const { return viewProjectionMatrix_; }
    float GetFovY() const { return fovY_; }
    float GetAspectRatio() const { return aspectRatio_; }
    float GetNearZ() const { return nearZ_; }
    float GetFarZ() const { return farZ_; }

private:
    Transform transform_{};
    float fovY_ = 0.25f * 3.14159265358979323846f;
    float aspectRatio_ = 16.0f / 9.0f;
    float nearZ_ = 0.1f;
    float farZ_ = 1000.0f;
    Matrix4x4 viewMatrix_{};
    Matrix4x4 projectionMatrix_{};
    Matrix4x4 viewProjectionMatrix_{};
};
