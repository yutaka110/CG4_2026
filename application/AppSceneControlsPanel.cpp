#include "AppSceneControlsPanel.h"

#include "AppRuntimeState.h"
#include "EffectRuntime.h"

#include "../../externals/imgui/imgui.h"

void DrawSceneLightingControlsPanel(
    AppRuntimeState& runtimeState) {
    ImGui::ColorEdit3("Light Color",
        reinterpret_cast<float*>(&runtimeState.directionalLightData.color));

    ImGui::SliderFloat3(
        "Light Direction",
        reinterpret_cast<float*>(&runtimeState.directionalLightData.direction),
        -1.0f,
        1.0f);

    ImGui::SliderFloat(
        "Intensity",
        &runtimeState.directionalLightData.intensity,
        0.0f,
        10.0f);
}

void DrawMaterialSettingsPanel(
    AppRuntimeState& runtimeState,
    const std::function<void()>& onAddParticle) {
    ImGui::Begin("Material Settings");
    DrawMaterialSettingsControlsPanel(runtimeState, onAddParticle);
    ImGui::End();
}

void DrawMaterialSettingsControlsPanel(
    AppRuntimeState& runtimeState,
    const std::function<void()>& onAddParticle) {
    ImGui::ColorEdit4("Material Color",
        reinterpret_cast<float*>(&runtimeState.materialData.color));
    ImGui::Checkbox("Enable Lighting", reinterpret_cast<bool*>(&runtimeState.materialData.enableLighting));
    ImGui::SliderFloat("Shininess", &runtimeState.materialData.shininess, 1.0f, 64.0f);

    ImGui::Text("Recommended: shininess 8-16");

    ImGui::Text("Scale");
    ImGui::DragFloat3("Scale", reinterpret_cast<float*>(&runtimeState.transform.scale),
        0.01f, 0.01f, 10.0f);

    ImGui::Text("Rotate");
    ImGui::DragFloat3("Rotate", reinterpret_cast<float*>(&runtimeState.transform.rotate),
        0.01f, -3.14f, 3.14f);

    ImGui::Text("Translate");
    ImGui::DragFloat3("Translate",
        reinterpret_cast<float*>(&runtimeState.transform.translate), 0.01f,
        -100.0f, 100.0f);

    ImGui::DragFloat2("UVTranslate", &runtimeState.uvTransformSprite.translate.x, 0.01f,
        -10.0f, 10.0f);
    ImGui::DragFloat2("UVScale", &runtimeState.uvTransformSprite.scale.x, 0.01f, -10.0f,
        10.0f);
    ImGui::SliderAngle("UVRotate", &runtimeState.uvTransformSprite.rotate.z);

    ImGui::DragFloat3(
        "EmitterTranslate",
        &runtimeState.emitter.transform.translate.x,
        0.01f,
        -100.0f,
        100.0f);

    ImGui::DragFloat3("Field Accel", &runtimeState.accelerationField.acceleration.x, 0.1f);
    ImGui::DragFloat3("Field Min", &runtimeState.accelerationField.area.min.x, 0.1f);
    ImGui::DragFloat3("Field Max", &runtimeState.accelerationField.area.max.x, 0.1f);

    if (ImGui::Button("Add Particle (Emitter)") && onAddParticle) {
        onAddParticle();
    }

    ImGui::DragFloat3("Point Pos", &runtimeState.pointLightData.position.x, 0.05f);
    ImGui::DragFloat("Point Intensity", &runtimeState.pointLightData.intensity, 0.05f, 0.0f, 50.0f);
    ImGui::DragFloat("Point Radius", &runtimeState.pointLightData.radius, 0.1f, 0.1f, 100.0f);
    ImGui::DragFloat("Point Decay", &runtimeState.pointLightData.decay, 0.05f, 0.1f, 8.0f);
}

void DrawVfxRuntimeControlsPanel(
    const VfxRuntimeControlsPanelInput& input) {
    if (input.runtimeState == nullptr || input.effectRuntime == nullptr ||
        input.trailMeshStreamStartupTelemetryFrames == nullptr) {
        return;
    }

    AppRuntimeState& runtimeState = *input.runtimeState;
    EffectRuntime& effectRuntime = *input.effectRuntime;
    uint32_t& trailMeshStreamStartupTelemetryFrames = *input.trailMeshStreamStartupTelemetryFrames;

    bool runtimePaused = effectRuntime.IsPaused();
    if (ImGui::Checkbox("Pause Effect Runtime", &runtimePaused)) {
        effectRuntime.SetPaused(runtimePaused);
    }
    float runtimeSpeed = effectRuntime.SpeedMultiplier();
    if (ImGui::SliderFloat("Effect Runtime Speed", &runtimeSpeed, 0.0f, 4.0f)) {
        effectRuntime.SetSpeedMultiplier(runtimeSpeed);
    }
    ImGui::Checkbox("Auto Play VFX Demo", &runtimeState.vfx.autoPlayVfxDemo);
    ImGui::Checkbox("Trail Mesh Stream", &runtimeState.vfx.enableTrailMeshStream);
    ImGui::Checkbox("Trail Mesh Stream Safety Fallback", &runtimeState.vfx.enableTrailMeshStreamAutoFallback);
    if (ImGui::Checkbox(
            "Trail Mesh Stream Startup Telemetry Log",
            &runtimeState.vfx.enableTrailMeshStreamStartupTelemetry)) {
        trailMeshStreamStartupTelemetryFrames = 0;
    }
    ImGui::Text(
        "trailMeshStreamStartupTelemetry=%s frames=%u/300",
        runtimeState.vfx.enableTrailMeshStreamStartupTelemetry ? "on" : "off",
        trailMeshStreamStartupTelemetryFrames);
    ImGui::SliderFloat("Demo Spawn Interval", &runtimeState.vfx.autoPlayVfxInterval, 0.1f, 2.0f, "%.2f");
    ImGui::SliderFloat("Demo Spawn Radius", &runtimeState.vfx.autoPlayVfxRadius, 0.0f, 8.0f, "%.2f");
    if (ImGui::Button("Play warp_core")) {
        effectRuntime.PlayEffectWithParams(
            "warp_core",
            runtimeState.emitter.transform.translate,
            {1.0f, 0.75f, 0.35f, 1.0f},
            {1.0f, 1.0f, 1.0f});
    }
    if (ImGui::Button("Play authoring_metadata_demo")) {
        effectRuntime.PlayEffectWithParams(
            "authoring_metadata_demo",
            runtimeState.emitter.transform.translate,
            {0.55f, 0.9f, 1.0f, 1.0f},
            {1.0f, 1.0f, 1.0f});
    }
    if (ImGui::Button("Play authoring_registry_only_demo")) {
        effectRuntime.PlayEffectWithParams(
            "authoring_registry_only_demo",
            runtimeState.emitter.transform.translate,
            {0.9f, 0.65f, 1.0f, 1.0f},
            {1.0f, 1.0f, 1.0f});
    }
    if (ImGui::Button("Play unknown_technique_demo")) {
        effectRuntime.PlayEffectWithParams(
            "authoring_unknown_technique_demo",
            runtimeState.emitter.transform.translate,
            {1.0f, 0.45f, 0.25f, 1.0f},
            {1.0f, 1.0f, 1.0f});
    }
    if (ImGui::Button("Clear Effects")) {
        effectRuntime.ClearInstances();
    }
}
