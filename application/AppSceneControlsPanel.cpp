#include "AppSceneControlsPanel.h"

#include "AppRuntimeState.h"
#include "EffectRuntime.h"

#include "../../externals/imgui/imgui.h"

#include <algorithm>

namespace {
Vector3 FrontHitEffectPosition(const AppRuntimeState& runtimeState) {
    const float frontRadius = runtimeState.transform.scale.z;
    return {
        runtimeState.transform.translate.x,
        runtimeState.transform.translate.y,
        runtimeState.transform.translate.z - frontRadius - 0.15f,
    };
}

void SyncHeldEffect(
    EffectRuntime& effectRuntime,
    bool enabled,
    uint32_t& instanceId,
    const char* effectName,
    const Vector3& position,
    const Vector4& color,
    const Vector3& scale) {
    if (!enabled) {
        if (instanceId != 0) {
            effectRuntime.SetEffectPreviewLoop(instanceId, false);
            effectRuntime.StopEffect(instanceId);
            instanceId = 0;
        }
        return;
    }

    EffectInstance* instance = effectRuntime.FindInstance(instanceId);
    if (instance == nullptr) {
        instanceId = effectRuntime.PlayEffectWithParams(effectName, position, color, scale);
        effectRuntime.SetEffectPreviewLoop(instanceId, true);
        instance = effectRuntime.FindInstance(instanceId);
        if (instance == nullptr) {
            return;
        }
    } else {
        effectRuntime.SetEffectPreviewLoop(instanceId, true);
    }

    if (instance->assetName != effectName) {
        effectRuntime.SetEffectPreviewLoop(instanceId, false);
        effectRuntime.StopEffect(instanceId);
        instanceId = effectRuntime.PlayEffectWithParams(effectName, position, color, scale);
        effectRuntime.SetEffectPreviewLoop(instanceId, true);
        return;
    }

    instance->transform.translate = position;
    instance->previousPosition = position;
}

void DisableHeldHitEffects(AppRuntimeState& runtimeState, EffectRuntime& effectRuntime) {
    runtimeState.vfx.holdHitPlaneBurst = false;
    runtimeState.vfx.holdHitRing = false;
    runtimeState.vfx.holdHitCylinder = false;
    runtimeState.vfx.holdHitCylinderCombo = false;
    SyncHeldEffect(
        effectRuntime,
        false,
        runtimeState.vfx.holdHitPlaneBurstInstanceId,
        "hit_plane_burst",
        {},
        {},
        {});
    SyncHeldEffect(
        effectRuntime,
        false,
        runtimeState.vfx.holdHitRingInstanceId,
        "hit_ring",
        {},
        {},
        {});
    SyncHeldEffect(
        effectRuntime,
        false,
        runtimeState.vfx.holdHitCylinderInstanceId,
        "hit_cylinder",
        {},
        {},
        {});
    SyncHeldEffect(
        effectRuntime,
        false,
        runtimeState.vfx.holdHitCylinderComboInstanceId,
        "hit_cylinder_combo",
        {},
        {},
        {});
}
} // namespace

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
    ImGui::SliderFloat(
        "Environment Reflection",
        &runtimeState.materialData.environmentCoefficient,
        0.0f,
        1.0f);

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

    ImGui::SeparatorText("Animated Cube");
    ImGui::Checkbox("Show Animated Cube", &runtimeState.showAnimatedCube);
    ImGui::SameLine();
    ImGui::Checkbox("Show Skinned", &runtimeState.showSkinnedModel);
    ImGui::SameLine();
    ImGui::Checkbox("Show Skeleton", &runtimeState.showSkeletonDebug);
    ImGui::SameLine();
    ImGui::Checkbox("Play Animation", &runtimeState.playAnimatedCube);
    ImGui::SameLine();
    ImGui::Checkbox("Loop Animation", &runtimeState.loopAnimatedCube);
    const char* skinnedModelLabels[] = {
        "simpleSkin",
        "human walk",
        "human sneakWalk",
    };
    int selectedSkinnedModel = static_cast<int>(runtimeState.selectedSkinnedModelIndex);
    if (ImGui::Combo(
            "Skinned Model",
            &selectedSkinnedModel,
            skinnedModelLabels,
            IM_ARRAYSIZE(skinnedModelLabels))) {
        runtimeState.selectedSkinnedModelIndex =
            static_cast<uint32_t>((std::max)(0, selectedSkinnedModel));
        runtimeState.animatedCubeTime = 0.0f;
    }
    ImGui::TextUnformatted("Skeleton Debug Source: selected Skinned Model");
    ImGui::SliderFloat("Animation Speed", &runtimeState.animatedCubeSpeed, -2.0f, 2.0f);
    ImGui::SliderFloat("Animation Time", &runtimeState.animatedCubeTime, 0.0f, 5.0f);
    ImGui::DragFloat3(
        "Animated Cube Scale",
        reinterpret_cast<float*>(&runtimeState.animatedCubeTransform.scale),
        0.01f,
        0.01f,
        10.0f);
    ImGui::DragFloat3(
        "Animated Cube Rotate",
        reinterpret_cast<float*>(&runtimeState.animatedCubeTransform.rotate),
        0.01f,
        -3.14f,
        3.14f);
    ImGui::DragFloat3(
        "Animated Cube Translate",
        reinterpret_cast<float*>(&runtimeState.animatedCubeTransform.translate),
        0.01f,
        -100.0f,
        100.0f);
    ImGui::DragFloat3(
        "Skinned Scale",
        reinterpret_cast<float*>(&runtimeState.skinnedModelTransform.scale),
        0.01f,
        0.01f,
        10.0f);
    ImGui::DragFloat3(
        "Skinned Rotate",
        reinterpret_cast<float*>(&runtimeState.skinnedModelTransform.rotate),
        0.01f,
        -3.14f,
        3.14f);
    ImGui::DragFloat3(
        "Skinned Translate",
        reinterpret_cast<float*>(&runtimeState.skinnedModelTransform.translate),
        0.01f,
        -100.0f,
        100.0f);

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
    if (ImGui::Button("Place Emitter On Front Hit")) {
        runtimeState.emitter.transform.translate = FrontHitEffectPosition(runtimeState);
    }

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
    ImGui::SeparatorText("VFX Visibility");
    ImGui::Checkbox("Particles", &runtimeState.vfx.enableParticles);
    ImGui::SameLine();
    ImGui::Checkbox("Trails", &runtimeState.vfx.enableTrails);
    ImGui::SameLine();
    ImGui::Checkbox("Beams", &runtimeState.vfx.enableBeams);
    ImGui::SameLine();
    ImGui::Checkbox("Distortion", &runtimeState.vfx.enableDistortions);
    ImGui::SameLine();
    ImGui::Checkbox("Rings", &runtimeState.vfx.enableRings);
    ImGui::SameLine();
    ImGui::Checkbox("Cylinders", &runtimeState.vfx.enableCylinders);
    ImGui::Checkbox("Skinned Surface VFX", &runtimeState.vfx.enableSkinnedSurfaceVfx);
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
    const Vector3 heldEffectPosition = FrontHitEffectPosition(runtimeState);
    ImGui::SeparatorText("Held Hit Effects");
    ImGui::Checkbox("Hold Plane Burst", &runtimeState.vfx.holdHitPlaneBurst);
    ImGui::SameLine();
    ImGui::Checkbox("Hold Ring", &runtimeState.vfx.holdHitRing);
    ImGui::SameLine();
    ImGui::Checkbox("Hold Cylinder", &runtimeState.vfx.holdHitCylinder);
    ImGui::SameLine();
    ImGui::Checkbox("Hold Cylinder Combo", &runtimeState.vfx.holdHitCylinderCombo);
    SyncHeldEffect(
        effectRuntime,
        runtimeState.vfx.holdHitPlaneBurst,
        runtimeState.vfx.holdHitPlaneBurstInstanceId,
        "hit_plane_burst",
        heldEffectPosition,
        {1.0f, 1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f});
    SyncHeldEffect(
        effectRuntime,
        runtimeState.vfx.holdHitRing,
        runtimeState.vfx.holdHitRingInstanceId,
        "hit_ring",
        heldEffectPosition,
        {0.9f, 0.95f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f});
    SyncHeldEffect(
        effectRuntime,
        runtimeState.vfx.holdHitCylinder,
        runtimeState.vfx.holdHitCylinderInstanceId,
        "hit_cylinder",
        heldEffectPosition,
        {0.65f, 0.85f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f});
    SyncHeldEffect(
        effectRuntime,
        runtimeState.vfx.holdHitCylinderCombo,
        runtimeState.vfx.holdHitCylinderComboInstanceId,
        "hit_cylinder_combo",
        heldEffectPosition,
        {1.0f, 1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f});
    if (ImGui::Button("Play warp_core")) {
        effectRuntime.PlayEffectWithParams(
            "warp_core",
            runtimeState.emitter.transform.translate,
            {1.0f, 0.75f, 0.35f, 1.0f},
            {1.0f, 1.0f, 1.0f});
    }
    if (ImGui::Button("Play hit_plane_burst")) {
        runtimeState.vfx.enableParticles = true;
        effectRuntime.PlayEffectWithParams(
            "hit_plane_burst",
            runtimeState.emitter.transform.translate,
            {1.0f, 1.0f, 1.0f, 1.0f},
            {1.0f, 1.0f, 1.0f});
    }
    if (ImGui::Button("Play hit_ring")) {
        runtimeState.vfx.enableRings = true;
        effectRuntime.PlayEffectWithParams(
            "hit_ring",
            runtimeState.emitter.transform.translate,
            {0.9f, 0.95f, 1.0f, 1.0f},
            {1.0f, 1.0f, 1.0f});
    }
    ImGui::SameLine();
    if (ImGui::Button("Play hit_cylinder")) {
        runtimeState.vfx.enableCylinders = true;
        effectRuntime.PlayEffectWithParams(
            "hit_cylinder",
            runtimeState.emitter.transform.translate,
            {0.65f, 0.85f, 1.0f, 1.0f},
            {1.0f, 1.0f, 1.0f});
    }
    ImGui::SameLine();
    if (ImGui::Button("Focus hit_combo")) {
        DisableHeldHitEffects(runtimeState, effectRuntime);
        runtimeState.vfx.autoPlayVfxDemo = false;
        runtimeState.vfx.enableParticles = true;
        runtimeState.vfx.enableRings = true;
        runtimeState.vfx.enableCylinders = false;
        runtimeState.vfx.enableTrails = false;
        runtimeState.vfx.enableBeams = false;
        runtimeState.vfx.enableDistortions = false;
        runtimeState.emitter.transform.translate = FrontHitEffectPosition(runtimeState);
        effectRuntime.ClearInstances();
        effectRuntime.PlayEffectWithParams(
            "hit_ring_plane_combo",
            runtimeState.emitter.transform.translate,
            {1.0f, 1.0f, 1.0f, 1.0f},
            {1.0f, 1.0f, 1.0f});
    }
    ImGui::SameLine();
    if (ImGui::Button("Focus hit_cylinder")) {
        DisableHeldHitEffects(runtimeState, effectRuntime);
        runtimeState.vfx.autoPlayVfxDemo = false;
        runtimeState.vfx.enableParticles = false;
        runtimeState.vfx.enableRings = false;
        runtimeState.vfx.enableCylinders = true;
        runtimeState.vfx.enableTrails = false;
        runtimeState.vfx.enableBeams = false;
        runtimeState.vfx.enableDistortions = false;
        runtimeState.emitter.transform.translate = FrontHitEffectPosition(runtimeState);
        effectRuntime.ClearInstances();
        effectRuntime.PlayEffectWithParams(
            "hit_cylinder",
            runtimeState.emitter.transform.translate,
            {0.65f, 0.85f, 1.0f, 1.0f},
            {1.0f, 1.0f, 1.0f});
    }
    ImGui::SameLine();
    if (ImGui::Button("Focus cylinder_combo")) {
        DisableHeldHitEffects(runtimeState, effectRuntime);
        runtimeState.vfx.autoPlayVfxDemo = false;
        runtimeState.vfx.enableParticles = true;
        runtimeState.vfx.enableRings = true;
        runtimeState.vfx.enableCylinders = true;
        runtimeState.vfx.enableTrails = false;
        runtimeState.vfx.enableBeams = false;
        runtimeState.vfx.enableDistortions = false;
        runtimeState.emitter.transform.translate = FrontHitEffectPosition(runtimeState);
        effectRuntime.ClearInstances();
        effectRuntime.PlayEffectWithParams(
            "hit_cylinder_combo",
            runtimeState.emitter.transform.translate,
            {1.0f, 1.0f, 1.0f, 1.0f},
            {1.0f, 1.0f, 1.0f});
    }
    ImGui::SameLine();
    if (ImGui::Button("Focus hit_plane_burst")) {
        DisableHeldHitEffects(runtimeState, effectRuntime);
        runtimeState.vfx.autoPlayVfxDemo = false;
        runtimeState.vfx.enableParticles = true;
        runtimeState.vfx.enableTrails = false;
        runtimeState.vfx.enableBeams = false;
        runtimeState.vfx.enableDistortions = false;
        runtimeState.vfx.enableRings = false;
        runtimeState.vfx.enableCylinders = false;
        runtimeState.emitter.transform.translate = FrontHitEffectPosition(runtimeState);
        effectRuntime.ClearInstances();
        effectRuntime.PlayEffectWithParams(
            "hit_plane_burst",
            runtimeState.emitter.transform.translate,
            {1.0f, 1.0f, 1.0f, 1.0f},
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
        DisableHeldHitEffects(runtimeState, effectRuntime);
        effectRuntime.ClearInstances();
    }
}
