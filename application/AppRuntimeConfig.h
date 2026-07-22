#pragma once

struct AppVfxRuntimeState;
struct AppRuntimeState;

enum class AppStartupScene {
    RailShooter,
    VfxPreview,
    MultiMaterialShowcase,
};

AppStartupScene ParseAppStartupSceneArguments(
    int argumentCount,
    const wchar_t* const* arguments);
AppStartupScene ResolveAppStartupSceneFromCommandLine();

void ResetMultiMaterialShowcaseHumanoidPose(AppRuntimeState& runtimeState);
float ResolveHumanoidMovementYaw(float moveX, float moveY) noexcept;
bool DidHumanoidMovementStart(
    float previousMoveMagnitude,
    float currentMoveMagnitude) noexcept;
float AdvanceHumanoidMovementYaw(
    float currentYaw,
    float targetYaw,
    float maxDelta) noexcept;
void ApplyMultiMaterialShowcasePresentationDefaults(AppRuntimeState& runtimeState);
bool BeginSkinnedAnimationBlend(
    AppRuntimeState& runtimeState,
    unsigned int targetModelIndex,
    float durationSeconds = 0.25f) noexcept;
float AdvanceSkinnedAnimationBlend(
    AppRuntimeState& runtimeState,
    float deltaTime) noexcept;
void CompleteSkinnedAnimationBlend(AppRuntimeState& runtimeState) noexcept;
void CancelSkinnedAnimationBlend(AppRuntimeState& runtimeState) noexcept;
bool ShouldAdvancePreviewRuntime(
    bool editorRuntimeAdvance,
    bool submissionShowcaseEnabled) noexcept;

void ApplyEnvironmentRuntimeConfig(AppRuntimeState& runtimeState);
void ApplyEnvironmentRuntimeConfig(AppVfxRuntimeState& runtimeState);
