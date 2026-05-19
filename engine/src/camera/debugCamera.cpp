#include "camera/DebugCamera.h"

#include <Windows.h>
#include <numbers>

void DebugCamera::Initialize() {
    Transform transform{};
    transform.scale = {1.0f, 1.0f, 1.0f};
    transform.rotate = {0.0f, 0.0f, 0.0f};
    transform.translate = {0.0f, 0.0f, -8.0f};
    rotation_ = transform.rotate;
    translation_ = transform.translate;
    camera_.Initialize(
        transform,
        0.25f * std::numbers::pi_v<float>,
        16.0f / 9.0f,
        0.1f,
        1000.0f);
}

void DebugCamera::SetTransform(const Transform& transform) {
    rotation_ = transform.rotate;
    translation_ = transform.translate;
    camera_.SetTransform(transform);
}

void DebugCamera::SetLens(float fovY, float aspectRatio, float nearZ, float farZ) {
    camera_.SetLens(fovY, aspectRatio, nearZ, farZ);
}

void DebugCamera::Update() {
    Transform transform = camera_.GetTransform();
    if (inputEnabled_) {
        const float moveSpeed = 0.1f;
        const float rotateSpeed = 0.02f;

        if (GetAsyncKeyState('W') & 0x8000) {
            transform.translate.z += moveSpeed;
        }
        if (GetAsyncKeyState('S') & 0x8000) {
            transform.translate.z -= moveSpeed;
        }
        if (GetAsyncKeyState('D') & 0x8000) {
            transform.translate.x += moveSpeed;
        }
        if (GetAsyncKeyState('A') & 0x8000) {
            transform.translate.x -= moveSpeed;
        }
        if (GetAsyncKeyState(VK_UP) & 0x8000) {
            transform.rotate.x += rotateSpeed;
        }
        if (GetAsyncKeyState(VK_DOWN) & 0x8000) {
            transform.rotate.x -= rotateSpeed;
        }
        if (GetAsyncKeyState(VK_RIGHT) & 0x8000) {
            transform.rotate.y += rotateSpeed;
        }
        if (GetAsyncKeyState(VK_LEFT) & 0x8000) {
            transform.rotate.y -= rotateSpeed;
        }
    }

    transform.scale = {1.0f, 1.0f, 1.0f};
    SetTransform(transform);
}
