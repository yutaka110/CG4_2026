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
void ApplyMultiMaterialShowcasePresentationDefaults(AppRuntimeState& runtimeState);
bool ShouldAdvancePreviewRuntime(
    bool editorRuntimeAdvance,
    bool submissionShowcaseEnabled) noexcept;

void ApplyEnvironmentRuntimeConfig(AppRuntimeState& runtimeState);
void ApplyEnvironmentRuntimeConfig(AppVfxRuntimeState& runtimeState);
