#include "EditorBuiltinDetailsSectionProviders.h"
#include "EditorAssetSelection.h"

#include "../../externals/imgui/imgui.h"

#include <memory>
#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>
#include "world/EditorWorldMutationService.h"
#include "world/SceneWorldObjectProvider.h"
#include "scene/EditorScene.h"
#include "scene/EditorSceneComponentRegistry.h"
#include "scene/EditorGimmickComponent.h"
#include "scene/EditorGimmickDefinitionRegistry.h"
#include "scene/EditorGimmickEventBindingComponent.h"
#include "scene/EditorGimmickEventBindingMutation.h"
#include "scene/EditorGimmickEventSequenceComponent.h"
#include "scene/EditorGimmickEventSequenceMutation.h"
#include "scene/EditorGimmickPresentationPhysicsAdapter.h"
#include "scene/EditorGimmickRuntimeBehavior.h"
#include "scene/EditorGimmickRuntimeFactory.h"
#include "scene/EditorGimmickRuntimeEventRouter.h"
#include "scene/EditorGimmickRuntimeDelayedEventScheduler.h"
#include "scene/EditorGimmickRuntimeEventSequenceRegistry.h"
#include "scene/EditorGimmickRuntimeInteractionSystem.h"
#include "scene/EditorGimmickRuntimeTriggerSystem.h"
#include "scene/EditorProductionScenePipeline.h"

namespace editor {
namespace {

const EditorObjectHandle* PrimarySelection(const EditorDetailsSectionContext& context) {
    if (context.selectedObjects == nullptr || context.selectedObjects->empty()) {
        return nullptr;
    }
    return &context.selectedObjects->front();
}

const EditorPropertyDescriptor* FindDescriptor(
    const EditorDetailsSectionContext& context,
    EditorDomainId domain,
    const char* propertyPath) {
    return context.propertyRegistry != nullptr
        ? context.propertyRegistry->Find(domain, propertyPath)
        : nullptr;
}

bool ReadSummary(
    const EditorDetailsSectionContext& context,
    const EditorObjectHandle& target,
    const EditorPropertyDescriptor* descriptor,
    std::string& outSummary) {
    if (context.propertyAccessor == nullptr || descriptor == nullptr) {
        return false;
    }
    EditorPropertyValue value{};
    if (!context.propertyAccessor->Get(target, *descriptor, value)) {
        return false;
    }
    outSummary = FormatEditorPropertyValue(*descriptor, value);
    return true;
}

void DrawPropertySummaryRow(
    const EditorDetailsSectionContext& context,
    const EditorObjectHandle& target,
    const char* propertyPath,
    const char* label) {
    const EditorPropertyDescriptor* descriptor =
        FindDescriptor(context, target.domain, propertyPath);
    std::string summary;
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(label);
    ImGui::TableNextColumn();
    if (ReadSummary(context, target, descriptor, summary)) {
        ImGui::TextUnformatted(summary.c_str());
    } else {
        ImGui::TextDisabled("unavailable");
    }
}

bool ApplySectionBatch(
    const EditorDetailsSectionContext& context,
    const EditorObjectHandle& target,
    std::vector<EditorPropertyBatchEdit> edits,
    const char* label) {
    if (context.propertyAccessor == nullptr || edits.empty()) {
        return false;
    }

    EditorPropertyEditService service;
    const EditorPropertyBatchEditResult result =
        service.ApplyBatch(
            EditorPropertyBatchEditRequest{
                context.propertyAccessor,
                context.transactions,
                context.dirtyState,
                context.notifications,
                std::move(edits),
                label != nullptr ? label : "Details Section Edit",
                target,
                context.canMutateAuthoring,
                true,
                context.source});
    if (!result.applied && context.notifications != nullptr) {
        context.notifications->Push(
            EditorNotificationSeverity::Error,
            context.source,
            result.message.empty() ? "Details section edit failed." : result.message);
    }
    return result.applied;
}

class VfxEffectAssetDetailsSection final : public EditorDetailsSectionProvider {
public:
    EditorDomainId Domain() const override { return EditorDomainId::VfxEffectAsset; }
    const char* SectionId() const override { return "details.vfxEffectAsset.production"; }
    const char* DisplayName() const override { return "Production VFX"; }

    void Draw(const EditorDetailsSectionContext& context) override {
        const EditorObjectHandle* target = PrimarySelection(context);
        if (target == nullptr) {
            return;
        }

        if (ImGui::BeginTable("VfxEffectAssetProductionDetails", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 140.0f);
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();
            DrawPropertySummaryRow(context, *target, "VfxEffectAsset.technique", "Technique");
            DrawPropertySummaryRow(context, *target, "VfxEffectAsset.spawnRate", "Spawn Rate");
            DrawPropertySummaryRow(context, *target, "VfxEffectAsset.lifetime", "Lifetime");
            DrawPropertySummaryRow(context, *target, "VfxEffectAsset.texture", "Texture");
            ImGui::EndTable();
        }

        const EditorPropertyDescriptor* spawnRate =
            FindDescriptor(context, EditorDomainId::VfxEffectAsset, "VfxEffectAsset.spawnRate");
        const EditorPropertyDescriptor* lifetime =
            FindDescriptor(context, EditorDomainId::VfxEffectAsset, "VfxEffectAsset.lifetime");
        const bool canNormalize =
            context.canMutateAuthoring &&
            context.propertyAccessor != nullptr &&
            spawnRate != nullptr &&
            lifetime != nullptr &&
            !spawnRate->readOnly &&
            !lifetime->readOnly;
        if (!canNormalize) {
            ImGui::BeginDisabled();
        }
        if (ImGui::SmallButton("Normalize Particle Defaults")) {
            EditorPropertyValue spawnValue{};
            spawnValue.floatValue = 64.0f;
            EditorPropertyValue lifetimeValue{};
            lifetimeValue.floatValue = 1.0f;
            ApplySectionBatch(
                context,
                *target,
                {
                    EditorPropertyBatchEdit{*target, spawnRate, spawnValue},
                    EditorPropertyBatchEdit{*target, lifetime, lifetimeValue},
                },
                "Normalize VFX Particle Defaults");
        }
        if (!canNormalize) {
            ImGui::EndDisabled();
        }
    }
};

class PostProcessPassDetailsSection final : public EditorDetailsSectionProvider {
public:
    EditorDomainId Domain() const override { return EditorDomainId::PostProcessPass; }
    const char* SectionId() const override { return "details.postProcess.production"; }
    const char* DisplayName() const override { return "Production Post Process"; }

    void Draw(const EditorDetailsSectionContext& context) override {
        const EditorObjectHandle* target = PrimarySelection(context);
        if (target == nullptr) {
            return;
        }

        if (ImGui::BeginTable("PostProcessProductionDetails", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 140.0f);
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();
            DrawPropertySummaryRow(context, *target, "PostProcessPass.stage", "Stage");
            DrawPropertySummaryRow(context, *target, "PostProcessPass.intensity", "Intensity");
            DrawPropertySummaryRow(context, *target, "PostProcessPass.threshold", "Threshold");
            DrawPropertySummaryRow(context, *target, "PostProcessPass.radius", "Radius");
            ImGui::EndTable();
        }

        const EditorPropertyDescriptor* intensity =
            FindDescriptor(context, EditorDomainId::PostProcessPass, "PostProcessPass.intensity");
        const EditorPropertyDescriptor* threshold =
            FindDescriptor(context, EditorDomainId::PostProcessPass, "PostProcessPass.threshold");
        const EditorPropertyDescriptor* radius =
            FindDescriptor(context, EditorDomainId::PostProcessPass, "PostProcessPass.radius");
        const bool canReset =
            context.canMutateAuthoring &&
            context.propertyAccessor != nullptr &&
            intensity != nullptr &&
            threshold != nullptr &&
            radius != nullptr;
        if (!canReset) {
            ImGui::BeginDisabled();
        }
        if (ImGui::SmallButton("Reset Tuning Triplet")) {
            EditorPropertyValue one{};
            one.floatValue = 1.0f;
            EditorPropertyValue radiusValue{};
            radiusValue.floatValue = 8.0f;
            ApplySectionBatch(
                context,
                *target,
                {
                    EditorPropertyBatchEdit{*target, intensity, one},
                    EditorPropertyBatchEdit{*target, threshold, one},
                    EditorPropertyBatchEdit{*target, radius, radiusValue},
                },
                "Reset Post Process Tuning");
        }
        if (!canReset) {
            ImGui::EndDisabled();
        }
    }
};

class CourseEventDetailsSection final : public EditorDetailsSectionProvider {
public:
    EditorDomainId Domain() const override { return EditorDomainId::CourseEventMarker; }
    const char* SectionId() const override { return "details.courseEvent.production"; }
    const char* DisplayName() const override { return "Course Event Dispatch"; }

    void Draw(const EditorDetailsSectionContext& context) override {
        const EditorObjectHandle* target = PrimarySelection(context);
        if (target == nullptr) {
            return;
        }
        if (ImGui::BeginTable("CourseEventDispatchDetails", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 140.0f);
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();
            DrawPropertySummaryRow(context, *target, "CourseEventMarker.distance", "Distance");
            DrawPropertySummaryRow(context, *target, "CourseEventMarker.type", "Type");
            DrawPropertySummaryRow(context, *target, "CourseEventMarker.id", "Id");
            DrawPropertySummaryRow(context, *target, "CourseEventMarker.payload", "Payload");
            ImGui::EndTable();
        }
    }
};

class RenderPresetDetailsSection final : public EditorDetailsSectionProvider {
public:
    EditorDomainId Domain() const override { return EditorDomainId::RenderPreset; }
    const char* SectionId() const override { return "details.renderPreset.production"; }
    const char* DisplayName() const override { return "Render Preset Authoring"; }

    void Draw(const EditorDetailsSectionContext&) override {
        ImGui::TextDisabled("Render preset authoring store is not bound yet.");
    }
};

class SceneEntityDetailsSection final : public EditorDetailsSectionProvider {
public:
    EditorDomainId Domain() const override { return EditorDomainId::SceneEntity; }
    const char* SectionId() const override { return "details.sceneEntity.components"; }
    const char* DisplayName() const override { return "Scene Components"; }

    void Draw(const EditorDetailsSectionContext& context) override {
        const EditorObjectHandle* target = PrimarySelection(context);
        if (target == nullptr || context.sceneWorldProvider == nullptr) return;
        const EditorSceneEntity* entity = context.sceneWorldProvider->ResolveEntity(*target);
        if (entity == nullptr) {
            ImGui::TextDisabled("Scene Entity no longer resolves.");
            return;
        }
        ImGui::Text("Entity GUID: %s", entity->guid.c_str());
        ImGui::Text("Parent GUID: %s", entity->parentGuid.empty() ? "<Scene Root>" : entity->parentGuid.c_str());
        bool runtimeEnabled = entity->runtimeEnabled;
        ImGui::BeginDisabled(!context.canMutateAuthoring);
        if (ImGui::Checkbox("Runtime Enabled", &runtimeEnabled)) {
            ExecuteRuntimeEnabledMutation(
                context, *target, runtimeEnabled);
            ImGui::EndDisabled();
            return;
        }
        ImGui::EndDisabled();
        const EditorScene* scene =
            context.sceneWorldProvider->BoundScene();
        const bool runtimeActive =
            scene != nullptr &&
            scene->IsRuntimeActiveInHierarchy(entity->guid);
        ImGui::TextDisabled(
            "Active in Hierarchy: %s",
            runtimeActive ? "true" : "false");
        DrawProductionMeshPresentationDebugger(
            context, *entity, scene);
        DrawGimmickRuntimeDebugger(
            context, *entity, scene);
        ImGui::Separator();

        for (const EditorSceneComponent& component : entity->components) {
            ImGui::PushID(component.typeId.c_str());
            const EditorSceneComponentRegistry& registry =
                context.sceneComponentRegistry != nullptr
                ? *context.sceneComponentRegistry
                : BuiltInEditorSceneComponentRegistry();
            const EditorSceneComponentDescriptor* descriptor =
                registry.Find(component.typeId);
            const bool required = descriptor != nullptr
                ? descriptor->required
                : component.typeId == kEditorTransformComponentType;
            const bool canDisable = descriptor == nullptr || descriptor->canDisable;
            bool enabled = component.enabled;
            ImGui::BeginDisabled(!context.canMutateAuthoring || !canDisable);
            if (ImGui::Checkbox("##Enabled", &enabled)) {
                ExecuteComponentEnabledMutation(
                    context, *target, component.typeId, enabled);
                ImGui::EndDisabled();
                ImGui::PopID();
                return;
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::Text(
                "%s",
                descriptor != nullptr
                    ? descriptor->displayName.c_str()
                    : DisplayNameForEditorSceneComponent(component.typeId));
            ImGui::SameLine();
            ImGui::TextDisabled("[%s]", component.typeId.c_str());
            if (!required) {
                ImGui::SameLine();
                ImGui::BeginDisabled(!context.canMutateAuthoring);
                if (ImGui::SmallButton("Remove")) {
                    ExecuteMutation(context, *target, EditorWorldMutationKind::RemoveComponent,
                        component.typeId);
                    ImGui::EndDisabled();
                    ImGui::PopID();
                    return;
                }
                ImGui::EndDisabled();
            }
            if (component.typeId == kEditorGimmickComponentType &&
                DrawGimmickDefinitionPicker(
                    context, *target, component)) {
                ImGui::PopID();
                return;
            }
            if (component.typeId ==
                    kEditorGimmickEventBindingComponentType &&
                DrawGimmickEventBindings(
                    context,
                    *target,
                    *entity,
                    component)) {
                ImGui::PopID();
                return;
            }
            if (component.typeId ==
                    kEditorGimmickEventSequenceComponentType &&
                DrawGimmickEventSequence(
                    context,
                    *target,
                    *entity,
                    component)) {
                ImGui::PopID();
                return;
            }
            if (descriptor != nullptr) {
                for (const EditorSceneComponentPropertyDescriptor&
                     propertyDescriptor : descriptor->properties) {
                    if (propertyDescriptor.kind !=
                            EditorScenePropertyKind::EntityReference ||
                        component.typeId ==
                            kEditorGimmickComponentType) {
                        continue;
                    }
                    if (DrawEntityReference(
                            context,
                            *target,
                            *entity,
                            component,
                            propertyDescriptor)) {
                        ImGui::PopID();
                        return;
                    }
                }
            }
            for (const EditorSceneProperty& property : component.properties) {
                if (component.typeId ==
                        kEditorGimmickComponentType &&
                    (property.name == "definition" ||
                     property.name == "parameterData")) {
                    continue;
                }
                if (component.typeId ==
                        kEditorGimmickEventBindingComponentType &&
                    property.name == "bindingData") {
                    continue;
                }
                if (component.typeId ==
                        kEditorGimmickEventSequenceComponentType &&
                    property.name == "sequenceData") {
                    continue;
                }
                ImGui::PushID(property.name.c_str());
                const EditorSceneComponentPropertyDescriptor* propertyDescriptor =
                    descriptor != nullptr
                    ? FindEditorSceneComponentPropertyDescriptor(
                        *descriptor, property.name)
                    : nullptr;
                ImGui::TextUnformatted(
                    propertyDescriptor != nullptr
                        ? propertyDescriptor->displayName.c_str()
                        : property.name.c_str());
                ImGui::SameLine(120.0f);
                ImGui::BeginDisabled(!context.canMutateAuthoring || !component.enabled);
                bool commit = false;
                std::string committedValue;
                if (propertyDescriptor != nullptr &&
                    propertyDescriptor->kind == EditorScenePropertyKind::Boolean) {
                    bool value = property.value == "true" || property.value == "1";
                    if (ImGui::Checkbox("##Value", &value)) {
                        commit = true;
                        committedValue = value ? "true" : "false";
                    }
                } else if (propertyDescriptor != nullptr &&
                    propertyDescriptor->kind == EditorScenePropertyKind::Enumeration) {
                    ImGui::SetNextItemWidth(-1.0f);
                    if (ImGui::BeginCombo("##Value", property.value.c_str())) {
                        for (const std::string& option : propertyDescriptor->enumValues) {
                            const bool selected = option == property.value;
                            if (ImGui::Selectable(option.c_str(), selected)) {
                                commit = true;
                                committedValue = option;
                            }
                            if (selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                } else {
                    std::array<char, 512> value{};
                    std::snprintf(
                        value.data(), value.size(), "%s", property.value.c_str());
                    ImGui::SetNextItemWidth(-1.0f);
                    commit = ImGui::InputText(
                        "##Value",
                        value.data(),
                        value.size(),
                        ImGuiInputTextFlags_EnterReturnsTrue);
                    if (commit) committedValue = value.data();
                }
                ImGui::EndDisabled();
                ImGui::PopID();
                if (commit && ExecutePropertyMutation(
                        context,
                        *target,
                        component.typeId,
                        property.name,
                        std::move(committedValue))) {
                    ImGui::PopID();
                    return;
                }
            }
            if (component.typeId == kEditorGimmickComponentType &&
                DrawGimmickParameters(
                    context,
                    *target,
                    *entity,
                    component)) {
                ImGui::PopID();
                return;
            }
            for (const EditorSceneObjectReference& reference : component.references) {
                if (component.typeId ==
                        kEditorGimmickEventBindingComponentType ||
                    component.typeId ==
                        kEditorGimmickEventSequenceComponentType) {
                    continue;
                }
                const EditorSceneComponentPropertyDescriptor*
                    referenceDescriptor =
                    descriptor != nullptr
                    ? FindEditorSceneComponentPropertyDescriptor(
                        *descriptor, reference.property)
                    : nullptr;
                if (referenceDescriptor != nullptr &&
                    referenceDescriptor->kind ==
                        EditorScenePropertyKind::EntityReference) {
                    continue;
                }
                ImGui::BulletText("%s -> %s", reference.property.c_str(),
                    !reference.assetGuid.empty() ? reference.assetGuid.c_str() :
                    (!reference.entityGuid.empty() ? reference.entityGuid.c_str() : "None"));
                if (reference.property == "material" ||
                    reference.property.rfind("material:", 0) == 0) {
                    if (AcceptMaterialDrop(
                            context, *target, component.typeId, reference.property)) {
                        ImGui::PopID();
                        return;
                    }
                }
            }
            if (component.typeId == kEditorMeshRendererComponentType &&
                std::none_of(component.references.begin(), component.references.end(),
                    [](const EditorSceneObjectReference& reference) {
                        return reference.property == "material" ||
                            reference.property.rfind("material:", 0) == 0;
                    })) {
                ImGui::Button("Drop Material Instance for Slot 0", ImVec2(-1.0f, 0.0f));
                if (AcceptMaterialDrop(context, *target, component.typeId, "material:0")) {
                    ImGui::PopID();
                    return;
                }
            }
            ImGui::Separator();
            ImGui::PopID();
        }

        ImGui::BeginDisabled(!context.canMutateAuthoring || context.worldMutations == nullptr);
        if (ImGui::Button("Add Component")) ImGui::OpenPopup("AddSceneComponent");
        if (ImGui::BeginPopup("AddSceneComponent")) {
            const EditorSceneComponentRegistry& registry =
                context.sceneComponentRegistry != nullptr
                ? *context.sceneComponentRegistry
                : BuiltInEditorSceneComponentRegistry();
            const auto ordered = registry.Ordered();
            std::size_t begin = 0;
            while (begin < ordered.size()) {
                const std::string& category = ordered[begin]->category;
                std::size_t end = begin;
                while (end < ordered.size() &&
                    ordered[end]->category == category) {
                    ++end;
                }
                bool hasAddable = false;
                for (std::size_t index = begin; index < end; ++index) {
                    hasAddable = hasAddable || ordered[index]->addable;
                }
                if (hasAddable &&
                    ImGui::BeginMenu(category.empty() ? "Other" : category.c_str())) {
                    for (std::size_t index = begin; index < end; ++index) {
                        if (ordered[index]->addable) {
                            DrawAddItem(
                                context, *target, *entity, *ordered[index]);
                        }
                    }
                    ImGui::EndMenu();
                }
                begin = end;
            }
            ImGui::EndPopup();
        }
        ImGui::EndDisabled();
    }

private:
    static void DrawRuntimeRow(
        const char* label,
        const char* value) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(label);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(
            value != nullptr ? value : "<None>");
    }

    static void DrawRuntimeVectorRow(
        const char* label,
        const Vector3& value) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(label);
        ImGui::TableNextColumn();
        ImGui::Text(
            "%.3f  %.3f  %.3f",
            value.x,
            value.y,
            value.z);
    }

    static void DrawProductionMeshPresentationDebugger(
        const EditorDetailsSectionContext& context,
        const EditorSceneEntity& entity,
        const EditorScene* scene) {
        const EditorSceneComponent* meshRenderer =
            scene != nullptr
                ? scene->FindComponent(
                      entity,
                      kEditorMeshRendererComponentType)
                : nullptr;
        if (meshRenderer == nullptr ||
            context.productionScenePipeline == nullptr) {
            return;
        }

        ImGui::Spacing();
        if (!ImGui::CollapsingHeader(
                "Production Mesh Presentation",
                ImGuiTreeNodeFlags_DefaultOpen)) {
            return;
        }

        EditorProductionMeshDrawMode drawMode =
            context.productionScenePipeline->DrawMode();
        if (ImGui::BeginCombo(
                "Draw Mode",
                EditorProductionMeshDrawModeLabel(drawMode))) {
            constexpr std::array<EditorProductionMeshDrawMode, 3>
                modes{
                    EditorProductionMeshDrawMode::Auto,
                    EditorProductionMeshDrawMode::ForceDirect,
                    EditorProductionMeshDrawMode::ForceGpuDriven};
            for (const EditorProductionMeshDrawMode candidate : modes) {
                const bool selected = candidate == drawMode;
                if (ImGui::Selectable(
                        EditorProductionMeshDrawModeLabel(candidate),
                        selected)) {
                    context.productionScenePipeline->SetDrawMode(
                        candidate);
                    drawMode = candidate;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::TextDisabled(
            "Force Direct bypasses GPU-driven indirect submission.");

        const EditorProductionMeshPresentationDiagnostic* diagnostic =
            context.productionScenePipeline
                ->FindPresentationDiagnostic(entity.guid);
        if (diagnostic == nullptr) {
            ImGui::TextDisabled(
                "No Presentation diagnostic was produced this frame.");
            return;
        }

        if (ImGui::BeginTable(
                "ProductionMeshPresentationState",
                2,
                ImGuiTableFlags_Borders |
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn(
                "Stage",
                ImGuiTableColumnFlags_WidthFixed,
                150.0f);
            ImGui::TableSetupColumn("State");
            ImGui::TableHeadersRow();
            const auto drawBool = [](const char* label, bool value) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(label);
                ImGui::TableNextColumn();
                ImGui::TextColored(
                    value
                        ? ImVec4{0.35f, 0.9f, 0.55f, 1.0f}
                        : ImVec4{1.0f, 0.45f, 0.35f, 1.0f},
                    "%s",
                    value ? "Ready" : "Not Ready");
            };
            drawBool("Asset Resolved", diagnostic->assetResolved);
            drawBool(
                "Runtime Cache",
                diagnostic->runtimeCacheReady);
            drawBool(
                "Hierarchy Visible",
                diagnostic->hierarchyVisible);
            drawBool(
                "Frustum Visible",
                diagnostic->frustumVisible);
            drawBool(
                "GPU Mesh",
                diagnostic->gpuAssetReady);
            drawBool(
                "GPU Transform",
                diagnostic->transformReady);
            drawBool(
                "GPU Pipeline",
                diagnostic->gpuPipelineReady);
            drawBool(
                "Visibility Dispatch",
                diagnostic->visibilityDispatchSucceeded);
            drawBool(
                "Command Readback",
                diagnostic->commandReadbackAvailable);
            drawBool(
                "Command Generated",
                diagnostic->commandGenerated);
            drawBool(
                "Command Layout",
                diagnostic->commandLayoutValidated);
            drawBool(
                "Hi-Z Fresh",
                diagnostic->hiZFresh);
            drawBool(
                "Batch Prepared",
                diagnostic->batchPrepared);
            drawBool(
                "Batch Submitted",
                diagnostic->batchSubmitted);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Selected LOD");
            ImGui::TableNextColumn();
            ImGui::Text("%u", diagnostic->selectedLod);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Render Packets");
            ImGui::TableNextColumn();
            ImGui::Text("%u", diagnostic->renderPacketCount);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("GPU Visible");
            ImGui::TableNextColumn();
            ImGui::Text("%u", diagnostic->gpuVisibleInstances);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Frustum Rejected");
            ImGui::TableNextColumn();
            ImGui::Text(
                "%u",
                diagnostic->frustumRejectedInstances);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Hi-Z Rejected");
            ImGui::TableNextColumn();
            ImGui::Text(
                "%u",
                diagnostic->hiZRejectedInstances);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Invalid Batch");
            ImGui::TableNextColumn();
            ImGui::Text(
                "%u",
                diagnostic->invalidBatchInstances);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Frustum-only Retry");
            ImGui::TableNextColumn();
            ImGui::TextColored(
                diagnostic->frustumOnlyRetry
                    ? ImVec4{1.0f, 0.75f, 0.25f, 1.0f}
                    : ImVec4{0.35f, 0.9f, 0.55f, 1.0f},
                "%s",
                diagnostic->frustumOnlyRetry
                    ? "Active"
                    : "Inactive");
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Occlusion Quarantine");
            ImGui::TableNextColumn();
            ImGui::TextColored(
                diagnostic->occlusionQuarantined
                    ? ImVec4{1.0f, 0.75f, 0.25f, 1.0f}
                    : ImVec4{0.35f, 0.9f, 0.55f, 1.0f},
                "%s",
                diagnostic->occlusionQuarantined
                    ? "Frustum-only"
                    : "Inactive");
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Prepared / Submitted");
            ImGui::TableNextColumn();
            ImGui::Text(
                "%u / %u",
                diagnostic->preparedBatches,
                diagnostic->executedBatches);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Resolved Path");
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(
                EditorProductionMeshPresentationPathLabel(
                    diagnostic->resolvedPath));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Auto Fallback");
            ImGui::TableNextColumn();
            ImGui::TextColored(
                diagnostic->autoDirectFallback
                    ? ImVec4{1.0f, 0.75f, 0.25f, 1.0f}
                    : ImVec4{0.35f, 0.9f, 0.55f, 1.0f},
                "%s",
                diagnostic->autoDirectFallback
                    ? "Direct Safety Path"
                    : "Inactive");
            ImGui::EndTable();
        }
        if (!diagnostic->lastFailure.empty()) {
            ImGui::TextWrapped(
                "Last rejection: %s",
                diagnostic->lastFailure.c_str());
        }
    }

    static void DrawGimmickRuntimeDebugger(
        const EditorDetailsSectionContext& context,
        const EditorSceneEntity& entity,
        const EditorScene* scene) {
        const EditorSceneComponent* gimmick =
            scene != nullptr
            ? scene->FindComponent(
                  entity, kEditorGimmickComponentType)
            : nullptr;
        if (gimmick == nullptr) return;

        ImGui::Spacing();
        if (!ImGui::CollapsingHeader(
                "Runtime Gimmick Debugger",
                ImGuiTreeNodeFlags_DefaultOpen)) {
            return;
        }
        const EditorGimmickRuntimeWorld* world =
            context.gimmickRuntimeWorld;
        if (world == nullptr || !world->Active()) {
            ImGui::TextDisabled(
                "Runtime inactive. Press Play or Sim to inspect "
                "the instantiated Gimmick.");
            return;
        }
        const EditorGimmickRuntimeInstance* instance =
            world->FindByEntity(entity.guid);
        if (instance == nullptr) {
            ImGui::TextDisabled(
                "No Runtime instance resolves for this Entity.");
            return;
        }

        if (ImGui::BeginTable(
                "GimmickRuntimeState",
                2,
                ImGuiTableFlags_Borders |
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn(
                "Runtime Field",
                ImGuiTableColumnFlags_WidthFixed,
                150.0f);
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();
            DrawRuntimeRow(
                "Stable ID", instance->stableId.c_str());
            DrawRuntimeRow(
                "Definition",
                instance->definitionId.c_str());
            DrawRuntimeRow(
                "Runtime Factory",
                instance->runtimeFactoryId.c_str());
            DrawRuntimeRow(
                "Behavior",
                instance->behavior != nullptr
                    ? instance->behavior->BehaviorId().data()
                    : "<None>");
            DrawRuntimeRow(
                "Lifecycle",
                ToString(instance->lifecycle.State()));
            DrawRuntimeRow(
                "Activation Mode",
                ToString(instance->activationMode));

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Enabled / One Shot");
            ImGui::TableNextColumn();
            ImGui::Text(
                "%s / %s",
                instance->lifecycle.Enabled()
                    ? "true"
                    : "false",
                instance->oneShot ? "true" : "false");

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Activation Count");
            ImGui::TableNextColumn();
            ImGui::Text(
                "%llu",
                static_cast<unsigned long long>(
                    instance->lifecycle.ActivationCount()));

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Cooldown");
            ImGui::TableNextColumn();
            ImGui::Text(
                "%.3f / %.3f sec",
                instance->lifecycle.CooldownRemaining(),
                instance->lifecycle.CooldownSeconds());

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Last Command");
            ImGui::TableNextColumn();
            if (instance->lastCommandSequence == 0) {
                ImGui::TextDisabled("<None>");
            } else {
                ImGui::Text(
                    "#%llu %s",
                    static_cast<unsigned long long>(
                        instance->lastCommandSequence),
                    ToString(instance->lastCommandKind));
            }
            DrawRuntimeRow(
                "Command Source",
                instance->lastCommandSourceEntityGuid.empty()
                    ? "<External / Automatic>"
                    : instance->
                          lastCommandSourceEntityGuid.c_str());

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Command Queue");
            ImGui::TableNextColumn();
            ImGui::Text(
                "pending %llu / processing %llu / dropped %llu",
                static_cast<unsigned long long>(
                    world->Commands().PendingCount()),
                static_cast<unsigned long long>(
                    world->Commands().ProcessingCount()),
                static_cast<unsigned long long>(
                    world->Commands().DroppedCount()));

            const EditorGimmickRuntimeEventRouter* eventRouter =
                context.gimmickRuntimeEventRouter;
            if (eventRouter != nullptr) {
                const EditorGimmickRuntimeEventRouterSnapshot&
                    eventState = eventRouter->Snapshot();
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Event Router");
                ImGui::TableNextColumn();
                ImGui::Text(
                    "received %llu / routed %llu / ignored %llu / "
                    "rejected %llu / broadcasts %llu",
                    static_cast<unsigned long long>(
                        eventState.receivedEventCount),
                    static_cast<unsigned long long>(
                        eventState.routedEventCount),
                    static_cast<unsigned long long>(
                        eventState.ignoredEventCount),
                    static_cast<unsigned long long>(
                        eventState.rejectedEventCount),
                    static_cast<unsigned long long>(
                        eventState.broadcastCount));
                const EditorGimmickRuntimeEventBindingRegistry*
                    bindingRegistry =
                        eventRouter->BindingRegistry();
                if (bindingRegistry != nullptr &&
                    bindingRegistry->Active()) {
                    uint32_t outgoingBindings = 0;
                    for (const auto& binding :
                         bindingRegistry->Bindings()) {
                        if (binding.sourceEntityGuid ==
                            entity.guid) {
                            ++outgoingBindings;
                        }
                    }
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Event Bindings");
                    ImGui::TableNextColumn();
                    ImGui::Text(
                        "%u outgoing / %llu total / %llu consumed / "
                        "%llu unresolved",
                        outgoingBindings,
                        static_cast<unsigned long long>(
                            bindingRegistry->Bindings().size()),
                        static_cast<unsigned long long>(
                            bindingRegistry->ConsumedCount()),
                        static_cast<unsigned long long>(
                            bindingRegistry->UnresolvedCount()));
                }
                const EditorGimmickRuntimeEventSequenceRegistry*
                    sequenceRegistry =
                        eventRouter->SequenceRegistry();
                if (sequenceRegistry != nullptr &&
                    sequenceRegistry->Active()) {
                    uint32_t outgoingSequences = 0;
                    for (const auto& sequence :
                         sequenceRegistry->Sequences()) {
                        if (sequence.sourceEntityGuid ==
                            entity.guid) {
                            ++outgoingSequences;
                        }
                    }
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Event Sequences");
                    ImGui::TableNextColumn();
                    ImGui::Text(
                        "%u outgoing / %llu total / %llu unresolved "
                        "steps / %llu started / %llu ignored",
                        outgoingSequences,
                        static_cast<unsigned long long>(
                            sequenceRegistry->Sequences().size()),
                        static_cast<unsigned long long>(
                            sequenceRegistry->
                                UnresolvedStepCount()),
                        static_cast<unsigned long long>(
                            eventState.
                                broadcastSequenceStartCount),
                        static_cast<unsigned long long>(
                            eventState.
                                broadcastSequenceIgnoredCount));
                }
                const EditorGimmickRuntimeDelayedEventScheduler*
                    delayedEvents =
                        eventRouter->DelayedEventScheduler();
                if (delayedEvents != nullptr) {
                    const auto& delayedState =
                        delayedEvents->Snapshot();
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Delayed Events");
                    ImGui::TableNextColumn();
                    ImGui::Text(
                        "%llu pending / %llu fired / %.3f sec",
                        static_cast<unsigned long long>(
                            delayedState.pendingCount),
                        static_cast<unsigned long long>(
                            delayedState.dispatchedCount),
                        delayedState.clockSeconds);
                }
                if (eventState.lastTargetEntityGuid ==
                    entity.guid) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Last Runtime Event");
                    ImGui::TableNextColumn();
                    ImGui::Text(
                        "#%llu %s -> %s%s",
                        static_cast<unsigned long long>(
                            eventState.lastEventSequence),
                        ToString(eventState.lastEventKind),
                        ToString(eventState.lastDecision),
                        eventState.commandQueued
                            ? " (command queued)"
                            : "");
                    if (!eventState.lastReason.empty()) {
                        DrawRuntimeRow(
                            "Activation Policy",
                            eventState.lastReason.c_str());
                    }
                }
            }

            if (const auto* door =
                    dynamic_cast<
                        const EditorDoorGimmickRuntimeBehavior*>(
                        instance->behavior.get())) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Door Target / Locked");
                ImGui::TableNextColumn();
                ImGui::Text(
                    "%s / %s",
                    door->TargetOpen() ? "Open" : "Closed",
                    door->Locked() ? "true" : "false");

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Door Open");
                ImGui::TableNextColumn();
                ImGui::Text(
                    "%.3f  offset %.3f / %.3f",
                    door->OpenFraction(),
                    door->CurrentOffset(),
                    door->OpenDistance());
            }
            if (const auto* runtimeSwitch =
                    dynamic_cast<
                        const EditorSwitchGimmickRuntimeBehavior*>(
                        instance->behavior.get())) {
                DrawRuntimeRow(
                    "Switch Target",
                    runtimeSwitch->TargetEntityGuid().empty()
                        ? "<Unresolved>"
                        : runtimeSwitch->
                              TargetEntityGuid().data());
                DrawRuntimeRow(
                    "Next Gimmick",
                    runtimeSwitch->
                            NextGimmickEntityGuid().empty()
                        ? "<None>"
                        : runtimeSwitch->
                              NextGimmickEntityGuid().data());
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Switch Dispatches");
                ImGui::TableNextColumn();
                ImGui::Text(
                    "%llu (%s)",
                    static_cast<unsigned long long>(
                        runtimeSwitch->DispatchCount()),
                    runtimeSwitch->ToggleTarget()
                        ? "Toggle"
                        : "Activate");
            }

            const EditorGimmickPresentationPhysicsAdapter*
                adapter = context.gimmickRuntimeAdapter;
            const EditorGimmickPresentationState*
                presentation =
                adapter != nullptr && adapter->Active()
                ? adapter->FindPresentation(entity.guid)
                : nullptr;
            if (presentation != nullptr) {
                DrawRuntimeVectorRow(
                    "Authored Translation",
                    presentation->authoredTranslation);
                DrawRuntimeVectorRow(
                    "Runtime Translation",
                    presentation->runtimeTranslation);
                DrawRuntimeVectorRow(
                    "Presentation Offset",
                    presentation->translationOffset);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Presentation Backends");
                ImGui::TableNextColumn();
                ImGui::Text(
                    "render %s / mesh physics pose %s / box %s",
                    presentation->rendererBacked
                        ? "yes"
                        : "no",
                    presentation->meshPhysicsPoseDriven
                        ? "yes"
                        : "no",
                    presentation->boxCollisionBacked
                        ? "yes"
                        : "no");
            }
            const EditorGimmickRuntimePhysicsBody* body =
                adapter != nullptr && adapter->Active()
                ? adapter->FindPhysicsBody(entity.guid)
                : nullptr;
            if (body != nullptr) {
                DrawRuntimeVectorRow(
                    "Collision AABB Min",
                    body->boundsMin);
                DrawRuntimeVectorRow(
                    "Collision AABB Max",
                    body->boundsMax);
            }

            const EditorGimmickRuntimeInteractionSystem*
                interaction =
                    context.gimmickRuntimeInteraction;
            if (interaction != nullptr) {
                const EditorGimmickRuntimeInteractionSnapshot&
                    interactionState =
                        interaction->Snapshot();
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Interaction Focus");
                ImGui::TableNextColumn();
                if (interactionState.focusedEntityGuid ==
                    entity.guid) {
                    ImGui::Text(
                        "%s  %.3f units%s",
                        interactionState.focused
                            ? "FOCUSED"
                            : "BLOCKED",
                        interactionState.hitDistance,
                        interactionState.commandAccepted
                            ? "  command queued"
                            : "");
                } else {
                    ImGui::TextDisabled("not focused");
                }
                if (interactionState.focusedEntityGuid ==
                        entity.guid &&
                    !interactionState.blockedReason.empty()) {
                    DrawRuntimeRow(
                        "Interaction Reason",
                        interactionState.blockedReason.c_str());
                }
            }

            const EditorGimmickRuntimeTriggerSystem* triggers =
                context.gimmickRuntimeTriggers;
            if (triggers != nullptr) {
                uint32_t selectedContacts = 0;
                for (const EditorGimmickRuntimeTriggerContact&
                         contact : triggers->Contacts()) {
                    if (contact.triggerEntityGuid == entity.guid) {
                        ++selectedContacts;
                    }
                }
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Trigger Contacts");
                ImGui::TableNextColumn();
                ImGui::Text(
                    "%u current  (frame enter %u / stay %u / exit %u)",
                    selectedContacts,
                    triggers->Snapshot().enteredThisFrame,
                    triggers->Snapshot().stayedThisFrame,
                    triggers->Snapshot().exitedThisFrame);
                if (triggers->Snapshot().
                        lastTriggerEntityGuid == entity.guid) {
                    DrawRuntimeRow(
                        "Last Trigger Subject",
                        triggers->Snapshot().
                            lastSubjectEntityGuid.c_str());
                }
            }
            ImGui::EndTable();
        }

        if (!instance->entityReferences.empty()) {
            ImGui::TextDisabled("Resolved Entity References");
            for (const EditorGimmickEntityReferenceValue&
                     reference : instance->entityReferences) {
                const EditorSceneEntity* resolved =
                    scene != nullptr
                    ? scene->FindEntity(reference.entityGuid)
                    : nullptr;
                ImGui::BulletText(
                    "%s -> %s [%s]",
                    reference.id.c_str(),
                    resolved != nullptr
                        ? resolved->name.c_str()
                        : reference.entityGuid.empty()
                        ? "<None>"
                        : "<Missing>",
                    reference.entityGuid.empty()
                        ? "-"
                        : reference.entityGuid.c_str());
            }
        }
    }

    static const EditorGimmickDefinitionRegistry&
    GimmickRegistry(
        const EditorDetailsSectionContext& context) {
        return context.gimmickDefinitionRegistry != nullptr
            ? *context.gimmickDefinitionRegistry
            : BuiltInEditorGimmickDefinitionRegistry();
    }

    static bool DrawGimmickDefinitionPicker(
        const EditorDetailsSectionContext& context,
        const EditorObjectHandle& target,
        const EditorSceneComponent& component) {
        const EditorGimmickDefinitionRegistry& registry =
            GimmickRegistry(context);
        EditorGimmickComponent gimmick{};
        std::string error;
        if (!EditorGimmickComponent::FromSceneComponent(
                component,
                gimmick,
                registry,
                &error,
                EditorGimmickValidationPolicy::Authoring)) {
            ImGui::TextColored(
                ImVec4(1.0f, 0.35f, 0.3f, 1.0f),
                "Gimmick decode failed: %s",
                error.c_str());
            return false;
        }
        const EditorGimmickDefinition* current =
            registry.Find(gimmick.definitionId);
        ImGui::TextUnformatted("Definition");
        ImGui::SameLine(120.0f);
        bool commit = false;
        std::string selectedType;
        ImGui::BeginDisabled(
            !context.canMutateAuthoring || !component.enabled);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo(
                "##GimmickDefinition",
                current != nullptr
                    ? current->displayName.c_str()
                    : gimmick.definitionId.c_str())) {
            for (const EditorGimmickDefinition* definition :
                 registry.Ordered()) {
                if (definition == nullptr) continue;
                const bool selected =
                    definition->typeId == gimmick.definitionId;
                const std::string label =
                    definition->category.empty()
                    ? definition->displayName
                    : definition->category + " / " +
                        definition->displayName;
                if (ImGui::Selectable(
                        label.c_str(), selected) &&
                    !selected) {
                    commit = true;
                    selectedType = definition->typeId;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        if (current != nullptr) {
            ImGui::TextDisabled(
                "Runtime Factory: %s",
                current->runtimeFactoryId.c_str());
        }
        return commit && ExecuteGimmickDefinitionMutation(
            context,
            target,
            std::move(selectedType));
    }

    static bool DrawGimmickParameters(
        const EditorDetailsSectionContext& context,
        const EditorObjectHandle& target,
        const EditorSceneEntity& owner,
        const EditorSceneComponent& component) {
        const EditorGimmickDefinitionRegistry& registry =
            GimmickRegistry(context);
        EditorGimmickComponent gimmick{};
        std::string error;
        if (!EditorGimmickComponent::FromSceneComponent(
                component,
                gimmick,
                registry,
                &error,
                EditorGimmickValidationPolicy::Authoring)) {
            return false;
        }
        const EditorGimmickDefinition* definition =
            registry.Find(gimmick.definitionId);
        if (definition == nullptr) return false;

        ImGui::SeparatorText("Definition Parameters");
        for (const EditorGimmickParameterDefinition& parameter :
             definition->parameters) {
            ImGui::PushID(parameter.id.c_str());
            if (parameter.kind ==
                EditorGimmickParameterKind::EntityReference) {
                EditorSceneComponentPropertyDescriptor descriptor{};
                descriptor.name = parameter.id;
                descriptor.displayName = parameter.displayName;
                descriptor.kind =
                    EditorScenePropertyKind::EntityReference;
                descriptor.required = parameter.required;
                descriptor.entityReferenceTargetComponentType =
                    parameter.entityReferenceTargetComponentType;
                descriptor.entityReferenceDefaultsToSelf =
                    parameter.entityReferenceDefaultsToSelf;
                const bool unresolved =
                    parameter.required &&
                    !parameter.entityReferenceDefaultsToSelf &&
                    FindEditorSceneEntityReference(
                        component, parameter.id) == nullptr;
                if (DrawEntityReference(
                        context,
                        target,
                        owner,
                        component,
                        descriptor)) {
                    ImGui::PopID();
                    return true;
                }
                if (unresolved) {
                    ImGui::TextColored(
                        ImVec4(1.0f, 0.72f, 0.25f, 1.0f),
                        "Required before Play");
                }
                ImGui::PopID();
                continue;
            }

            const auto authored = std::find_if(
                gimmick.parameters.begin(),
                gimmick.parameters.end(),
                [&](const EditorGimmickParameterValue& value) {
                    return value.id == parameter.id;
                });
            if (authored == gimmick.parameters.end()) {
                ImGui::TextDisabled(
                    "%s: missing",
                    parameter.displayName.c_str());
                ImGui::PopID();
                continue;
            }
            ImGui::TextUnformatted(parameter.displayName.c_str());
            ImGui::SameLine(120.0f);
            bool commit = false;
            std::string committedValue;
            ImGui::BeginDisabled(
                !context.canMutateAuthoring || !component.enabled);
            if (parameter.kind ==
                EditorGimmickParameterKind::Boolean) {
                bool value =
                    authored->value == "true" ||
                    authored->value == "1";
                if (ImGui::Checkbox("##Value", &value)) {
                    commit = true;
                    committedValue = value ? "true" : "false";
                }
            } else if (parameter.kind ==
                EditorGimmickParameterKind::Enumeration) {
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::BeginCombo(
                        "##Value",
                        authored->value.c_str())) {
                    for (const std::string& option :
                         parameter.enumValues) {
                        const bool selected =
                            option == authored->value;
                        if (ImGui::Selectable(
                                option.c_str(), selected) &&
                            !selected) {
                            commit = true;
                            committedValue = option;
                        }
                        if (selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            } else {
                std::array<char, 512> value{};
                std::snprintf(
                    value.data(),
                    value.size(),
                    "%s",
                    authored->value.c_str());
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::InputText(
                        "##Value",
                        value.data(),
                        value.size(),
                        ImGuiInputTextFlags_EnterReturnsTrue)) {
                    commit = true;
                    committedValue = value.data();
                }
            }
            ImGui::EndDisabled();
            if (parameter.hasNumericRange) {
                ImGui::TextDisabled(
                    "Range: %.3g to %.3g",
                    parameter.minimumValue,
                    parameter.maximumValue);
            }
            ImGui::PopID();
            if (commit &&
                ExecuteGimmickParameterMutation(
                    context,
                    target,
                    parameter.id,
                    std::move(committedValue))) {
                return true;
            }
        }
        return false;
    }

    static std::string MakeEventSequenceStepId(
        const EditorGimmickEventSequenceComponent& authored) {
        for (;;) {
            std::string id =
                "step-" + GenerateEditorWorldGuid().substr(0, 16);
            const bool exists = std::any_of(
                authored.steps.begin(),
                authored.steps.end(),
                [&](const EditorGimmickEventSequenceStep& step) {
                    return step.id == id;
                });
            if (!exists) return id;
        }
    }

    static bool DrawEventSequenceTargetPicker(
        const EditorDetailsSectionContext& context,
        const EditorObjectHandle& target,
        const EditorSceneComponent& component,
        const EditorGimmickEventSequenceStep& step) {
        const EditorScene* scene =
            context.sceneWorldProvider != nullptr
            ? context.sceneWorldProvider->BoundScene()
            : nullptr;
        if (scene == nullptr) return false;
        const EditorSceneEntity* resolved =
            scene->FindEntity(step.targetEntityGuid);
        const bool correctType =
            resolved != nullptr &&
            scene->FindComponent(
                *resolved, kEditorGimmickComponentType) != nullptr;
        const std::string preview =
            correctType ? resolved->name : "<Missing or Wrong Type>";

        bool commit = false;
        std::string selectedGuid;
        ImGui::TextUnformatted("Target");
        ImGui::SameLine(120.0f);
        ImGui::BeginDisabled(
            !context.canMutateAuthoring || !component.enabled);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo(
                "##EventSequenceTarget", preview.c_str())) {
            static std::array<char, 128> filter{};
            if (ImGui::IsWindowAppearing()) filter.fill('\0');
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint(
                "##EventSequenceTargetFilter",
                "Search Gimmick Entity...",
                filter.data(),
                filter.size());
            for (const EditorSceneEntity& candidate :
                 scene->entities) {
                if (scene->FindComponent(
                        candidate,
                        kEditorGimmickComponentType) == nullptr) {
                    continue;
                }
                std::string label = candidate.name + " [" +
                    candidate.guid.substr(
                        0,
                        std::min<std::size_t>(
                            8, candidate.guid.size())) +
                    "]";
                if (filter[0] != '\0' &&
                    label.find(filter.data()) ==
                        std::string::npos) {
                    continue;
                }
                const bool selected =
                    step.targetEntityGuid == candidate.guid;
                if (ImGui::Selectable(label.c_str(), selected) &&
                    !selected) {
                    selectedGuid = candidate.guid;
                    commit = true;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        if (context.canMutateAuthoring && component.enabled &&
            ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload =
                    ImGui::AcceptDragDropPayload(
                        "EDITOR_WORLD_OBJECT")) {
                const char* stableId =
                    static_cast<const char*>(payload->Data);
                EditorObjectHandle droppedHandle{};
                droppedHandle.domain =
                    EditorDomainId::SceneEntity;
                droppedHandle.stableId =
                    stableId != nullptr ? stableId : "";
                const EditorSceneEntity* candidate =
                    context.sceneWorldProvider->ResolveEntity(
                        droppedHandle);
                if (candidate != nullptr &&
                    scene->FindComponent(
                        *candidate,
                        kEditorGimmickComponentType) != nullptr) {
                    selectedGuid = candidate->guid;
                    commit = true;
                } else if (context.notifications != nullptr) {
                    context.notifications->Push(
                        EditorNotificationSeverity::Warning,
                        "Event Sequence",
                        "Dropped Entity must contain a Gimmick "
                        "Component.");
                }
            }
            ImGui::EndDragDropTarget();
        }
        if (!correctType) {
            ImGui::TextColored(
                ImVec4(1.0f, 0.72f, 0.25f, 1.0f),
                "Target is unresolved or not a Gimmick.");
        }
        if (!commit) return false;
        EditorGimmickEventSequenceStep updated = step;
        updated.targetEntityGuid = std::move(selectedGuid);
        EditorGimmickEventSequenceMutation mutation{};
        mutation.kind =
            EditorGimmickEventSequenceMutationKind::Replace;
        mutation.stepId = step.id;
        mutation.value = std::move(updated);
        return ExecuteEventSequenceMutation(
            context, target, std::move(mutation));
    }

    static bool DrawGimmickEventSequence(
        const EditorDetailsSectionContext& context,
        const EditorObjectHandle& target,
        const EditorSceneEntity& owner,
        const EditorSceneComponent& component) {
        EditorGimmickEventSequenceComponent authored{};
        std::string error;
        if (!EditorGimmickEventSequenceComponent::
                FromSceneComponent(
                    component, authored, &error)) {
            ImGui::TextColored(
                ImVec4(1.0f, 0.35f, 0.3f, 1.0f),
                "Event Sequence decode failed: %s",
                error.c_str());
            return false;
        }
        const EditorScene* scene =
            context.sceneWorldProvider != nullptr
            ? context.sceneWorldProvider->BoundScene()
            : nullptr;
        ImGui::SeparatorText("Event Sequence Timeline");
        ImGui::TextDisabled(
            "%zu / %zu steps",
            authored.steps.size(),
            EditorGimmickEventSequenceComponent::kMaximumSteps);

        EditorGimmickRuntimeEventKind sourceEvent =
            authored.sourceEvent;
        EditorGimmickEventSequencePlaybackPolicy playback =
            authored.playbackPolicy;
        bool settingsChanged = false;
        ImGui::BeginDisabled(
            !context.canMutateAuthoring || !component.enabled);
        ImGui::TextUnformatted("Source Event");
        ImGui::SameLine(120.0f);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo(
                "##SequenceSourceEvent",
                ToString(sourceEvent))) {
            for (EditorGimmickRuntimeEventKind candidate : {
                     EditorGimmickRuntimeEventKind::Automatic,
                     EditorGimmickRuntimeEventKind::
                         InteractionPressed,
                     EditorGimmickRuntimeEventKind::TriggerEntered,
                     EditorGimmickRuntimeEventKind::TriggerStayed,
                     EditorGimmickRuntimeEventKind::TriggerExited,
                     EditorGimmickRuntimeEventKind::
                         ActivateRequested,
                     EditorGimmickRuntimeEventKind::
                         DeactivateRequested,
                     EditorGimmickRuntimeEventKind::
                         ToggleRequested,
                     EditorGimmickRuntimeEventKind::ResetRequested,
                     EditorGimmickRuntimeEventKind::EnableRequested,
                     EditorGimmickRuntimeEventKind::
                         DisableRequested}) {
                const bool selected = candidate == sourceEvent;
                if (ImGui::Selectable(
                        ToString(candidate), selected) &&
                    !selected) {
                    sourceEvent = candidate;
                    settingsChanged = true;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::TextUnformatted("Playback");
        ImGui::SameLine(120.0f);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo(
                "##SequencePlayback", ToString(playback))) {
            for (EditorGimmickEventSequencePlaybackPolicy candidate : {
                     EditorGimmickEventSequencePlaybackPolicy::
                         Restart,
                     EditorGimmickEventSequencePlaybackPolicy::
                         IgnoreWhilePlaying,
                     EditorGimmickEventSequencePlaybackPolicy::
                         AllowParallel}) {
                const bool selected = candidate == playback;
                if (ImGui::Selectable(
                        ToString(candidate), selected) &&
                    !selected) {
                    playback = candidate;
                    settingsChanged = true;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        if (settingsChanged) {
            EditorGimmickEventSequenceMutation mutation{};
            mutation.kind =
                EditorGimmickEventSequenceMutationKind::SetSettings;
            mutation.sourceEvent = sourceEvent;
            mutation.playbackPolicy = playback;
            if (ExecuteEventSequenceMutation(
                    context, target, std::move(mutation))) {
                return true;
            }
        }

        double duration = 0.0;
        for (const auto& step : authored.steps) {
            duration = (std::max)(duration, step.timeSeconds);
        }
        ImGui::TextDisabled(
            "Duration %.3f s | order is stable for equal times",
            duration);
        bool descending = false;
        for (std::size_t index = 1;
             index < authored.steps.size();
             ++index) {
            descending =
                descending ||
                authored.steps[index].timeSeconds <
                    authored.steps[index - 1].timeSeconds;
        }
        if (descending) {
            ImGui::TextColored(
                ImVec4(1.0f, 0.72f, 0.25f, 1.0f),
                "Timeline contains out-of-time-order steps.");
        }

        for (std::size_t index = 0;
             index < authored.steps.size();
             ++index) {
            const EditorGimmickEventSequenceStep& step =
                authored.steps[index];
            ImGui::PushID(step.id.c_str());
            char overlay[64]{};
            std::snprintf(
                overlay,
                sizeof(overlay),
                "%.3f s  %s",
                step.timeSeconds,
                ToString(step.command));
            ImGui::ProgressBar(
                duration > 0.0
                    ? static_cast<float>(
                        step.timeSeconds / duration)
                    : 0.0f,
                ImVec2(-1.0f, 0.0f),
                overlay);

            const bool open = ImGui::TreeNodeEx(
                "##SequenceStep",
                ImGuiTreeNodeFlags_DefaultOpen |
                    ImGuiTreeNodeFlags_SpanAvailWidth,
                "Step %zu",
                index + 1);
            ImGui::SameLine();
            ImGui::BeginDisabled(
                !context.canMutateAuthoring || index == 0);
            if (ImGui::SmallButton("Up")) {
                EditorGimmickEventSequenceMutation mutation{};
                mutation.kind =
                    EditorGimmickEventSequenceMutationKind::
                        MoveEarlier;
                mutation.stepId = step.id;
                if (ExecuteEventSequenceMutation(
                        context, target, std::move(mutation))) {
                    ImGui::EndDisabled();
                    if (open) ImGui::TreePop();
                    ImGui::PopID();
                    return true;
                }
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(
                !context.canMutateAuthoring ||
                index + 1 >= authored.steps.size());
            if (ImGui::SmallButton("Down")) {
                EditorGimmickEventSequenceMutation mutation{};
                mutation.kind =
                    EditorGimmickEventSequenceMutationKind::
                        MoveLater;
                mutation.stepId = step.id;
                if (ExecuteEventSequenceMutation(
                        context, target, std::move(mutation))) {
                    ImGui::EndDisabled();
                    if (open) ImGui::TreePop();
                    ImGui::PopID();
                    return true;
                }
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(!context.canMutateAuthoring);
            if (ImGui::SmallButton("Duplicate")) {
                EditorGimmickEventSequenceMutation mutation{};
                mutation.kind =
                    EditorGimmickEventSequenceMutationKind::Add;
                mutation.value = step;
                mutation.value.id =
                    MakeEventSequenceStepId(authored);
                mutation.value.timeSeconds =
                    (std::min)(
                        EditorGimmickEventSequenceComponent::
                            kMaximumDurationSeconds,
                        step.timeSeconds + 0.1);
                if (ExecuteEventSequenceMutation(
                        context, target, std::move(mutation))) {
                    ImGui::EndDisabled();
                    if (open) ImGui::TreePop();
                    ImGui::PopID();
                    return true;
                }
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove")) {
                EditorGimmickEventSequenceMutation mutation{};
                mutation.kind =
                    EditorGimmickEventSequenceMutationKind::Remove;
                mutation.stepId = step.id;
                if (ExecuteEventSequenceMutation(
                        context, target, std::move(mutation))) {
                    ImGui::EndDisabled();
                    if (open) ImGui::TreePop();
                    ImGui::PopID();
                    return true;
                }
            }
            ImGui::EndDisabled();
            if (!open) {
                ImGui::PopID();
                continue;
            }

            ImGui::TextDisabled("Stable ID: %s", step.id.c_str());
            EditorGimmickEventSequenceStep updated = step;
            bool replace = false;
            ImGui::BeginDisabled(
                !context.canMutateAuthoring || !component.enabled);
            bool enabled = step.enabled;
            if (ImGui::Checkbox("Enabled", &enabled)) {
                updated.enabled = enabled;
                replace = true;
            }
            float time = static_cast<float>(step.timeSeconds);
            ImGui::TextUnformatted("Time (s)");
            ImGui::SameLine(120.0f);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputFloat(
                    "##SequenceStepTime",
                    &time,
                    0.1f,
                    1.0f,
                    "%.3f",
                    ImGuiInputTextFlags_EnterReturnsTrue)) {
                updated.timeSeconds = (std::clamp)(
                    static_cast<double>(time),
                    0.0,
                    EditorGimmickEventSequenceComponent::
                        kMaximumDurationSeconds);
                replace = true;
            }
            ImGui::TextUnformatted("Command");
            ImGui::SameLine(120.0f);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo(
                    "##SequenceCommand",
                    ToString(step.command))) {
                for (EditorGimmickRuntimeCommandKind candidate : {
                         EditorGimmickRuntimeCommandKind::Activate,
                         EditorGimmickRuntimeCommandKind::
                             Deactivate,
                         EditorGimmickRuntimeCommandKind::Toggle,
                         EditorGimmickRuntimeCommandKind::Reset,
                         EditorGimmickRuntimeCommandKind::Enable,
                         EditorGimmickRuntimeCommandKind::Disable}) {
                    const bool selected =
                        candidate == step.command;
                    if (ImGui::Selectable(
                            ToString(candidate), selected) &&
                        !selected) {
                        updated.command = candidate;
                        replace = true;
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            int priority = step.priority;
            ImGui::TextUnformatted("Priority");
            ImGui::SameLine(120.0f);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputInt(
                    "##SequencePriority",
                    &priority,
                    1,
                    10,
                    ImGuiInputTextFlags_EnterReturnsTrue)) {
                updated.priority = (std::clamp)(
                    priority, -1000000, 1000000);
                replace = true;
            }
            std::array<char, 4097> payload{};
            std::snprintf(
                payload.data(),
                payload.size(),
                "%s",
                step.payload.c_str());
            ImGui::TextUnformatted("Payload");
            ImGui::SameLine(120.0f);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputText(
                    "##SequencePayload",
                    payload.data(),
                    payload.size(),
                    ImGuiInputTextFlags_EnterReturnsTrue)) {
                updated.payload = payload.data();
                replace = true;
            }
            ImGui::EndDisabled();
            if (replace) {
                EditorGimmickEventSequenceMutation mutation{};
                mutation.kind =
                    EditorGimmickEventSequenceMutationKind::Replace;
                mutation.stepId = step.id;
                mutation.value = std::move(updated);
                if (ExecuteEventSequenceMutation(
                        context, target, std::move(mutation))) {
                    ImGui::TreePop();
                    ImGui::PopID();
                    return true;
                }
            }
            if (DrawEventSequenceTargetPicker(
                    context, target, component, step)) {
                ImGui::TreePop();
                ImGui::PopID();
                return true;
            }
            ImGui::TreePop();
            ImGui::PopID();
        }

        const EditorSceneEntity* defaultTarget = nullptr;
        if (scene != nullptr) {
            if (scene->FindComponent(
                    owner, kEditorGimmickComponentType) != nullptr) {
                defaultTarget = &owner;
            } else {
                const auto found = std::find_if(
                    scene->entities.begin(),
                    scene->entities.end(),
                    [&](const EditorSceneEntity& candidate) {
                        return scene->FindComponent(
                            candidate,
                            kEditorGimmickComponentType) != nullptr;
                    });
                if (found != scene->entities.end()) {
                    defaultTarget = &*found;
                }
            }
        }
        const bool atLimit =
            authored.steps.size() >=
            EditorGimmickEventSequenceComponent::kMaximumSteps;
        ImGui::BeginDisabled(
            !context.canMutateAuthoring ||
            !component.enabled ||
            defaultTarget == nullptr ||
            atLimit);
        if (ImGui::Button("Add Timeline Step")) {
            EditorGimmickEventSequenceMutation mutation{};
            mutation.kind =
                EditorGimmickEventSequenceMutationKind::Add;
            mutation.value.id =
                MakeEventSequenceStepId(authored);
            mutation.value.targetEntityGuid = defaultTarget->guid;
            mutation.value.timeSeconds =
                authored.steps.empty()
                ? 0.0
                : (std::min)(
                    EditorGimmickEventSequenceComponent::
                        kMaximumDurationSeconds,
                    authored.steps.back().timeSeconds + 0.5);
            if (ExecuteEventSequenceMutation(
                    context, target, std::move(mutation))) {
                ImGui::EndDisabled();
                return true;
            }
        }
        ImGui::EndDisabled();
        if (defaultTarget == nullptr) {
            ImGui::TextColored(
                ImVec4(1.0f, 0.72f, 0.25f, 1.0f),
                "Add a Gimmick Component to at least one Entity "
                "before creating a timeline step.");
        }
        return false;
    }

    static std::string MakeEventBindingId(
        const EditorGimmickEventBindingComponent& authored) {
        for (;;) {
            std::string id =
                "binding-" + GenerateEditorWorldGuid().substr(0, 16);
            const bool exists = std::any_of(
                authored.bindings.begin(),
                authored.bindings.end(),
                [&](const EditorGimmickEventBinding& binding) {
                    return binding.id == id;
                });
            if (!exists) return id;
        }
    }

    static const EditorSceneEntity* FirstGimmickTarget(
        const EditorScene& scene,
        const EditorSceneEntity& owner) {
        if (scene.FindComponent(
                owner, kEditorGimmickComponentType) != nullptr) {
            return &owner;
        }
        const auto found = std::find_if(
            scene.entities.begin(),
            scene.entities.end(),
            [&](const EditorSceneEntity& candidate) {
                return scene.FindComponent(
                    candidate,
                    kEditorGimmickComponentType) != nullptr;
            });
        return found == scene.entities.end() ? nullptr : &*found;
    }

    static bool DrawEventBindingTargetPicker(
        const EditorDetailsSectionContext& context,
        const EditorObjectHandle& target,
        const EditorSceneComponent& component,
        const EditorGimmickEventBinding& binding) {
        const EditorScene* scene =
            context.sceneWorldProvider != nullptr
            ? context.sceneWorldProvider->BoundScene()
            : nullptr;
        if (scene == nullptr) return false;

        const EditorSceneEntity* resolved =
            scene->FindEntity(binding.targetEntityGuid);
        const bool correctType =
            resolved != nullptr &&
            scene->FindComponent(
                *resolved, kEditorGimmickComponentType) != nullptr;
        const std::string preview = correctType
            ? resolved->name
            : "<Missing or Wrong Type>";

        bool commit = false;
        std::string selectedGuid;
        ImGui::TextUnformatted("Target");
        ImGui::SameLine(120.0f);
        ImGui::BeginDisabled(
            !context.canMutateAuthoring || !component.enabled);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo(
                "##EventBindingTarget", preview.c_str())) {
            static std::array<char, 128> filter{};
            if (ImGui::IsWindowAppearing()) filter.fill('\0');
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint(
                "##EventBindingTargetFilter",
                "Search Gimmick Entity...",
                filter.data(),
                filter.size());
            for (const EditorSceneEntity& candidate :
                 scene->entities) {
                if (scene->FindComponent(
                        candidate,
                        kEditorGimmickComponentType) == nullptr) {
                    continue;
                }
                std::string label = candidate.name + " [" +
                    candidate.guid.substr(
                        0,
                        std::min<std::size_t>(
                            8, candidate.guid.size())) +
                    "]";
                if (filter[0] != '\0' &&
                    label.find(filter.data()) ==
                        std::string::npos) {
                    continue;
                }
                const bool selected =
                    binding.targetEntityGuid == candidate.guid;
                if (ImGui::Selectable(label.c_str(), selected) &&
                    !selected) {
                    commit = true;
                    selectedGuid = candidate.guid;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();

        if (context.canMutateAuthoring && component.enabled &&
            ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload =
                    ImGui::AcceptDragDropPayload(
                        "EDITOR_WORLD_OBJECT")) {
                const char* stableId =
                    static_cast<const char*>(payload->Data);
                EditorObjectHandle droppedHandle{};
                droppedHandle.domain =
                    EditorDomainId::SceneEntity;
                droppedHandle.stableId =
                    stableId != nullptr ? stableId : "";
                const EditorSceneEntity* candidate =
                    context.sceneWorldProvider->ResolveEntity(
                        droppedHandle);
                if (candidate != nullptr &&
                    scene->FindComponent(
                        *candidate,
                        kEditorGimmickComponentType) != nullptr) {
                    commit = true;
                    selectedGuid = candidate->guid;
                } else if (context.notifications != nullptr) {
                    context.notifications->Push(
                        EditorNotificationSeverity::Warning,
                        "Event Binding",
                        "Dropped Entity must contain a Gimmick "
                        "Component.");
                }
            }
            ImGui::EndDragDropTarget();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "Accepts Entities with Component: %s",
                kEditorGimmickComponentType.data());
        }
        if (!correctType) {
            ImGui::TextColored(
                ImVec4(1.0f, 0.72f, 0.25f, 1.0f),
                "Target is unresolved or not a Gimmick.");
        }
        if (!commit) return false;

        EditorGimmickEventBinding updated = binding;
        updated.targetEntityGuid = std::move(selectedGuid);
        EditorGimmickEventBindingMutation mutation{};
        mutation.kind =
            EditorGimmickEventBindingMutationKind::Replace;
        mutation.bindingId = binding.id;
        mutation.value = std::move(updated);
        return ExecuteEventBindingMutation(
            context, target, std::move(mutation));
    }

    static bool DrawGimmickEventBindings(
        const EditorDetailsSectionContext& context,
        const EditorObjectHandle& target,
        const EditorSceneEntity& owner,
        const EditorSceneComponent& component) {
        EditorGimmickEventBindingComponent authored{};
        std::string error;
        if (!EditorGimmickEventBindingComponent::FromSceneComponent(
                component, authored, &error)) {
            ImGui::TextColored(
                ImVec4(1.0f, 0.35f, 0.3f, 1.0f),
                "Event Binding decode failed: %s",
                error.c_str());
            return false;
        }
        const EditorScene* scene =
            context.sceneWorldProvider != nullptr
            ? context.sceneWorldProvider->BoundScene()
            : nullptr;

        ImGui::SeparatorText("Event Bindings");
        ImGui::TextDisabled(
            "%zu / %zu bindings",
            authored.bindings.size(),
            EditorGimmickEventBindingComponent::kMaximumBindings);

        for (const EditorGimmickEventBinding& binding :
             authored.bindings) {
            ImGui::PushID(binding.id.c_str());
            const std::string header =
                std::string(ToString(binding.sourceEvent)) +
                " -> " + ToString(binding.targetCommand);
            const bool open = ImGui::TreeNodeEx(
                "##Binding",
                ImGuiTreeNodeFlags_DefaultOpen |
                    ImGuiTreeNodeFlags_SpanAvailWidth,
                "%s",
                header.c_str());
            ImGui::SameLine();
            ImGui::BeginDisabled(!context.canMutateAuthoring);
            if (ImGui::SmallButton("Duplicate")) {
                EditorGimmickEventBindingMutation mutation{};
                mutation.kind =
                    EditorGimmickEventBindingMutationKind::Add;
                mutation.value = binding;
                mutation.value.id = MakeEventBindingId(authored);
                if (ExecuteEventBindingMutation(
                        context, target, std::move(mutation))) {
                    ImGui::EndDisabled();
                    if (open) ImGui::TreePop();
                    ImGui::PopID();
                    return true;
                }
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove")) {
                EditorGimmickEventBindingMutation mutation{};
                mutation.kind =
                    EditorGimmickEventBindingMutationKind::Remove;
                mutation.bindingId = binding.id;
                if (ExecuteEventBindingMutation(
                        context, target, std::move(mutation))) {
                    ImGui::EndDisabled();
                    if (open) ImGui::TreePop();
                    ImGui::PopID();
                    return true;
                }
            }
            ImGui::EndDisabled();

            if (!open) {
                ImGui::PopID();
                continue;
            }

            ImGui::TextDisabled(
                "Stable ID: %s", binding.id.c_str());
            EditorGimmickEventBinding updated = binding;
            bool replace = false;

            ImGui::BeginDisabled(
                !context.canMutateAuthoring || !component.enabled);
            bool enabled = binding.enabled;
            if (ImGui::Checkbox("Enabled", &enabled)) {
                updated.enabled = enabled;
                replace = true;
            }
            ImGui::SameLine();
            bool oneShot = binding.oneShot;
            if (ImGui::Checkbox("One Shot", &oneShot)) {
                updated.oneShot = oneShot;
                replace = true;
            }

            ImGui::TextUnformatted("Source Event");
            ImGui::SameLine(120.0f);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo(
                    "##SourceEvent",
                    ToString(binding.sourceEvent))) {
                for (EditorGimmickRuntimeEventKind candidate : {
                         EditorGimmickRuntimeEventKind::Automatic,
                         EditorGimmickRuntimeEventKind::
                             InteractionPressed,
                         EditorGimmickRuntimeEventKind::TriggerEntered,
                         EditorGimmickRuntimeEventKind::TriggerStayed,
                         EditorGimmickRuntimeEventKind::TriggerExited,
                         EditorGimmickRuntimeEventKind::
                             ActivateRequested,
                         EditorGimmickRuntimeEventKind::
                             DeactivateRequested,
                         EditorGimmickRuntimeEventKind::
                             ToggleRequested,
                         EditorGimmickRuntimeEventKind::ResetRequested,
                         EditorGimmickRuntimeEventKind::EnableRequested,
                         EditorGimmickRuntimeEventKind::
                             DisableRequested}) {
                    const bool selected =
                        candidate == binding.sourceEvent;
                    if (ImGui::Selectable(
                            ToString(candidate), selected) &&
                        !selected) {
                        updated.sourceEvent = candidate;
                        replace = true;
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::TextUnformatted("Command");
            ImGui::SameLine(120.0f);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo(
                    "##TargetCommand",
                    ToString(binding.targetCommand))) {
                for (EditorGimmickRuntimeCommandKind candidate : {
                         EditorGimmickRuntimeCommandKind::Activate,
                         EditorGimmickRuntimeCommandKind::Deactivate,
                         EditorGimmickRuntimeCommandKind::Toggle,
                         EditorGimmickRuntimeCommandKind::Reset,
                         EditorGimmickRuntimeCommandKind::Enable,
                         EditorGimmickRuntimeCommandKind::Disable}) {
                    const bool selected =
                        candidate == binding.targetCommand;
                    if (ImGui::Selectable(
                            ToString(candidate), selected) &&
                        !selected) {
                        updated.targetCommand = candidate;
                        replace = true;
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            int priority = binding.priority;
            ImGui::TextUnformatted("Priority");
            ImGui::SameLine(120.0f);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputInt(
                    "##Priority",
                    &priority,
                    1,
                    10,
                    ImGuiInputTextFlags_EnterReturnsTrue)) {
                updated.priority = priority;
                replace = true;
            }

            float delaySeconds =
                static_cast<float>(binding.delaySeconds);
            ImGui::TextUnformatted("Delay (s)");
            ImGui::SameLine(120.0f);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputFloat(
                    "##DelaySeconds",
                    &delaySeconds,
                    0.1f,
                    1.0f,
                    "%.3f",
                    ImGuiInputTextFlags_EnterReturnsTrue)) {
                updated.delaySeconds =
                    (std::max)(0.0, static_cast<double>(
                        delaySeconds));
                replace = true;
            }

            float repeatInterval =
                static_cast<float>(
                    binding.repeatIntervalSeconds);
            ImGui::TextUnformatted("Interval (s)");
            ImGui::SameLine(120.0f);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputFloat(
                    "##RepeatInterval",
                    &repeatInterval,
                    0.1f,
                    1.0f,
                    "%.3f",
                    ImGuiInputTextFlags_EnterReturnsTrue)) {
                updated.repeatIntervalSeconds =
                    (std::max)(0.0, static_cast<double>(
                        repeatInterval));
                replace = true;
            }

            int repeatCount =
                static_cast<int>(binding.repeatCount);
            ImGui::TextUnformatted("Repeat");
            ImGui::SameLine(120.0f);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputInt(
                    "##RepeatCount",
                    &repeatCount,
                    1,
                    10,
                    ImGuiInputTextFlags_EnterReturnsTrue)) {
                repeatCount = (std::clamp)(
                    repeatCount, 0, 1000000);
                updated.repeatCount =
                    static_cast<uint32_t>(repeatCount);
                if (updated.repeatCount != 1 &&
                    updated.repeatIntervalSeconds <= 0.0) {
                    updated.repeatIntervalSeconds = 1.0;
                }
                replace = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "1 = one shot, 0 = repeat until cancelled.");
            }

            std::array<char, 4097> payload{};
            std::snprintf(
                payload.data(),
                payload.size(),
                "%s",
                binding.payload.c_str());
            ImGui::TextUnformatted("Payload");
            ImGui::SameLine(120.0f);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputText(
                    "##Payload",
                    payload.data(),
                    payload.size(),
                    ImGuiInputTextFlags_EnterReturnsTrue)) {
                updated.payload = payload.data();
                replace = true;
            }
            ImGui::EndDisabled();

            if (replace) {
                EditorGimmickEventBindingMutation mutation{};
                mutation.kind =
                    EditorGimmickEventBindingMutationKind::Replace;
                mutation.bindingId = binding.id;
                mutation.value = std::move(updated);
                if (ExecuteEventBindingMutation(
                        context, target, std::move(mutation))) {
                    ImGui::TreePop();
                    ImGui::PopID();
                    return true;
                }
            }

            if (DrawEventBindingTargetPicker(
                    context, target, component, binding)) {
                ImGui::TreePop();
                ImGui::PopID();
                return true;
            }
            ImGui::TreePop();
            ImGui::PopID();
        }

        const EditorSceneEntity* defaultTarget =
            scene != nullptr
            ? FirstGimmickTarget(*scene, owner)
            : nullptr;
        const bool atLimit =
            authored.bindings.size() >=
            EditorGimmickEventBindingComponent::kMaximumBindings;
        ImGui::BeginDisabled(
            !context.canMutateAuthoring ||
            !component.enabled ||
            defaultTarget == nullptr ||
            atLimit);
        if (ImGui::Button("Add Event Binding")) {
            EditorGimmickEventBindingMutation mutation{};
            mutation.kind =
                EditorGimmickEventBindingMutationKind::Add;
            mutation.value.id = MakeEventBindingId(authored);
            mutation.value.targetEntityGuid =
                defaultTarget->guid;
            if (ExecuteEventBindingMutation(
                    context, target, std::move(mutation))) {
                ImGui::EndDisabled();
                return true;
            }
        }
        ImGui::EndDisabled();
        if (defaultTarget == nullptr) {
            ImGui::TextColored(
                ImVec4(1.0f, 0.72f, 0.25f, 1.0f),
                "Add a Gimmick Component to at least one Entity "
                "before creating a binding.");
        }
        return false;
    }

    static bool DrawEntityReference(
        const EditorDetailsSectionContext& context,
        const EditorObjectHandle& target,
        const EditorSceneEntity& owner,
        const EditorSceneComponent& component,
        const EditorSceneComponentPropertyDescriptor& descriptor) {
        const EditorScene* scene =
            context.sceneWorldProvider != nullptr
            ? context.sceneWorldProvider->BoundScene()
            : nullptr;
        if (scene == nullptr) return false;

        ImGui::PushID(descriptor.name.c_str());
        ImGui::TextUnformatted(descriptor.displayName.c_str());
        ImGui::SameLine(120.0f);
        const EditorSceneObjectReference* reference =
            FindEditorSceneEntityReference(component, descriptor.name);
        const bool explicitReference =
            reference != nullptr && !reference->entityGuid.empty();
        const EditorSceneEntity* resolved =
            ResolveEditorSceneEntityReference(
                *scene, owner, component, descriptor);
        std::string preview;
        if (explicitReference) {
            preview = resolved != nullptr
                ? resolved->name
                : "<Missing or Wrong Type>";
        } else if (descriptor.entityReferenceDefaultsToSelf) {
            preview = "<Self: " + owner.name + ">";
        } else {
            preview = "<None>";
        }

        bool commit = false;
        std::string selectedGuid;
        ImGui::BeginDisabled(
            !context.canMutateAuthoring || !component.enabled);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo("##EntityReference", preview.c_str())) {
            static std::array<char, 128> filter{};
            if (ImGui::IsWindowAppearing()) filter.fill('\0');
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint(
                "##EntityReferenceFilter",
                "Search Entity...",
                filter.data(),
                filter.size());

            if (descriptor.entityReferenceDefaultsToSelf ||
                !descriptor.required) {
                const char* defaultLabel =
                    descriptor.entityReferenceDefaultsToSelf
                    ? "<Self>"
                    : "<None>";
                if (ImGui::Selectable(
                        defaultLabel, !explicitReference) &&
                    explicitReference) {
                    commit = true;
                    selectedGuid.clear();
                }
            }

            for (const EditorSceneEntity& candidate :
                 scene->entities) {
                if (!MatchesEditorSceneEntityReferenceTarget(
                        *scene, candidate, descriptor)) {
                    continue;
                }
                if (descriptor.entityReferenceDefaultsToSelf &&
                    candidate.guid == owner.guid) {
                    continue;
                }
                std::string label = candidate.name + " [" +
                    candidate.guid.substr(
                        0,
                        std::min<std::size_t>(
                            8, candidate.guid.size())) +
                    "]";
                if (filter[0] != '\0' &&
                    label.find(filter.data()) ==
                        std::string::npos) {
                    continue;
                }
                const bool selected =
                    explicitReference &&
                    reference->entityGuid == candidate.guid;
                if (ImGui::Selectable(label.c_str(), selected) &&
                    !selected) {
                    commit = true;
                    selectedGuid = candidate.guid;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();

        bool dropped = false;
        std::string droppedGuid;
        if (context.canMutateAuthoring && component.enabled &&
            ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload =
                    ImGui::AcceptDragDropPayload(
                        "EDITOR_WORLD_OBJECT")) {
                const char* stableId =
                    static_cast<const char*>(payload->Data);
                EditorObjectHandle droppedHandle{};
                droppedHandle.domain =
                    EditorDomainId::SceneEntity;
                droppedHandle.stableId =
                    stableId != nullptr ? stableId : "";
                const EditorSceneEntity* candidate =
                    context.sceneWorldProvider->ResolveEntity(
                        droppedHandle);
                if (candidate != nullptr &&
                    MatchesEditorSceneEntityReferenceTarget(
                        *scene, *candidate, descriptor)) {
                    dropped = true;
                    droppedGuid = candidate->guid;
                } else if (context.notifications != nullptr) {
                    context.notifications->Push(
                        EditorNotificationSeverity::Warning,
                        "Entity Reference",
                        "Dropped Entity does not contain the required "
                        "Component type.");
                }
            }
            ImGui::EndDragDropTarget();
        }
        if (!descriptor.entityReferenceTargetComponentType.empty() &&
            ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "Accepts Entities with Component: %s",
                descriptor.entityReferenceTargetComponentType.c_str());
        }
        ImGui::PopID();
        if (dropped) {
            commit = true;
            selectedGuid = std::move(droppedGuid);
        }
        return commit && ExecuteEntityReferenceMutation(
            context,
            target,
            component.typeId,
            descriptor.name,
            std::move(selectedGuid));
    }

    static bool ExecuteRuntimeEnabledMutation(
        const EditorDetailsSectionContext& context,
        const EditorObjectHandle& target,
        bool enabled) {
        if (context.worldMutations == nullptr ||
            context.transactions == nullptr) {
            return false;
        }
        EditorWorldMutationRequest request{};
        request.kind = EditorWorldMutationKind::SetRuntimeEnabled;
        request.targets = {target};
        request.value = enabled;
        const EditorWorldMutationResult result =
            context.worldMutations->Execute(
                request,
                *context.transactions,
                context.canMutateAuthoring);
        if (result.succeeded && context.onWorldMutated) {
            context.onWorldMutated(result);
        }
        if (context.notifications != nullptr) {
            context.notifications->Push(
                result.succeeded
                    ? EditorNotificationSeverity::Info
                    : EditorNotificationSeverity::Error,
                "Runtime Activation",
                result.message);
        }
        return result.succeeded;
    }

    static bool AcceptMaterialDrop(
        const EditorDetailsSectionContext& context,
        const EditorObjectHandle& target,
        std::string_view componentType,
        std::string_view property) {
        if (!ImGui::BeginDragDropTarget()) return false;
        bool changed = false;
        if (const ImGuiPayload* raw = ImGui::AcceptDragDropPayload(kEditorAssetDragDropPayloadId)) {
            if (raw->Data != nullptr && raw->DataSize == sizeof(EditorAssetDragDropPayload)) {
                const auto& payload = *static_cast<const EditorAssetDragDropPayload*>(raw->Data);
                if (payload.kind == EditorAssetKind::MaterialInstance && payload.guid[0] != '\0') {
                    changed = ExecuteAssetReferenceMutation(
                        context, target, std::string(componentType), std::string(property),
                        payload.guid.data());
                } else if (context.notifications != nullptr) {
                    context.notifications->Push(
                        EditorNotificationSeverity::Warning, "Material Slot",
                        "Material slots accept only durable Material Instance Assets.");
                }
            }
        }
        ImGui::EndDragDropTarget();
        return changed;
    }

    static bool ExecuteAssetReferenceMutation(
        const EditorDetailsSectionContext& context,
        const EditorObjectHandle& target,
        std::string componentType,
        std::string property,
        std::string assetGuid) {
        if (context.worldMutations == nullptr || context.transactions == nullptr) return false;
        EditorWorldMutationRequest request{};
        request.kind = EditorWorldMutationKind::SetComponentAssetReference;
        request.targets = {target};
        request.componentType = std::move(componentType);
        request.property = std::move(property);
        request.assetGuid = std::move(assetGuid);
        const EditorWorldMutationResult result = context.worldMutations->Execute(
            request, *context.transactions, context.canMutateAuthoring);
        if (result.succeeded && context.onWorldMutated) context.onWorldMutated(result);
        if (context.notifications != nullptr) {
            context.notifications->Push(
                result.succeeded ? EditorNotificationSeverity::Info : EditorNotificationSeverity::Error,
                "Material Slot", result.message);
        }
        return result.succeeded;
    }

    static bool ExecutePropertyMutation(
        const EditorDetailsSectionContext& context,
        const EditorObjectHandle& target,
        std::string componentType,
        std::string property,
        std::string value) {
        if (context.worldMutations == nullptr || context.transactions == nullptr) return false;
        EditorWorldMutationRequest request{};
        request.kind = EditorWorldMutationKind::SetComponentProperty;
        request.targets = {target};
        request.componentType = std::move(componentType);
        request.property = std::move(property);
        request.propertyValue = std::move(value);
        const EditorWorldMutationResult result = context.worldMutations->Execute(
            request, *context.transactions, context.canMutateAuthoring);
        if (result.succeeded && context.onWorldMutated) context.onWorldMutated(result);
        if (!result.succeeded && context.notifications != nullptr) {
            context.notifications->Push(
                EditorNotificationSeverity::Error, "Scene Components", result.message);
        }
        return result.succeeded;
    }

    static bool ExecuteEntityReferenceMutation(
        const EditorDetailsSectionContext& context,
        const EditorObjectHandle& target,
        std::string componentType,
        std::string property,
        std::string entityGuid) {
        if (context.worldMutations == nullptr ||
            context.transactions == nullptr) {
            return false;
        }
        EditorWorldMutationRequest request{};
        request.kind =
            EditorWorldMutationKind::SetComponentEntityReference;
        request.targets = {target};
        request.componentType = std::move(componentType);
        request.property = std::move(property);
        request.entityGuid = std::move(entityGuid);
        const EditorWorldMutationResult result =
            context.worldMutations->Execute(
                request,
                *context.transactions,
                context.canMutateAuthoring);
        if (result.succeeded && context.onWorldMutated) {
            context.onWorldMutated(result);
        }
        if (context.notifications != nullptr) {
            context.notifications->Push(
                result.succeeded
                    ? EditorNotificationSeverity::Info
                    : EditorNotificationSeverity::Error,
                "Entity Reference",
                result.message);
        }
        return result.succeeded;
    }

    static bool ExecuteGimmickDefinitionMutation(
        const EditorDetailsSectionContext& context,
        const EditorObjectHandle& target,
        std::string definitionTypeId) {
        if (context.worldMutations == nullptr ||
            context.transactions == nullptr) {
            return false;
        }
        EditorWorldMutationRequest request{};
        request.kind =
            EditorWorldMutationKind::SetGimmickDefinition;
        request.targets = {target};
        request.propertyValue = std::move(definitionTypeId);
        const EditorWorldMutationResult result =
            context.worldMutations->Execute(
                request,
                *context.transactions,
                context.canMutateAuthoring);
        if (result.succeeded && context.onWorldMutated) {
            context.onWorldMutated(result);
        }
        if (context.notifications != nullptr) {
            context.notifications->Push(
                result.succeeded
                    ? EditorNotificationSeverity::Info
                    : EditorNotificationSeverity::Error,
                "Gimmick Definition",
                result.message);
        }
        return result.succeeded;
    }

    static bool ExecuteGimmickParameterMutation(
        const EditorDetailsSectionContext& context,
        const EditorObjectHandle& target,
        std::string parameterId,
        std::string value) {
        if (context.worldMutations == nullptr ||
            context.transactions == nullptr) {
            return false;
        }
        EditorWorldMutationRequest request{};
        request.kind =
            EditorWorldMutationKind::SetGimmickParameter;
        request.targets = {target};
        request.property = std::move(parameterId);
        request.propertyValue = std::move(value);
        const EditorWorldMutationResult result =
            context.worldMutations->Execute(
                request,
                *context.transactions,
                context.canMutateAuthoring);
        if (result.succeeded && context.onWorldMutated) {
            context.onWorldMutated(result);
        }
        if (!result.succeeded &&
            context.notifications != nullptr) {
            context.notifications->Push(
                EditorNotificationSeverity::Error,
                "Gimmick Parameter",
                result.message);
        }
        return result.succeeded;
    }

    static bool ExecuteEventBindingMutation(
        const EditorDetailsSectionContext& context,
        const EditorObjectHandle& target,
        EditorGimmickEventBindingMutation mutation) {
        if (context.worldMutations == nullptr ||
            context.transactions == nullptr) {
            return false;
        }
        EditorWorldMutationRequest request{};
        request.kind =
            EditorWorldMutationKind::MutateGimmickEventBinding;
        request.targets = {target};
        request.componentType =
            std::string(kEditorGimmickEventBindingComponentType);
        request.eventBindingMutation = std::move(mutation);
        const EditorWorldMutationResult result =
            context.worldMutations->Execute(
                request,
                *context.transactions,
                context.canMutateAuthoring);
        if (result.succeeded && context.onWorldMutated) {
            context.onWorldMutated(result);
        }
        if (context.notifications != nullptr) {
            context.notifications->Push(
                result.succeeded
                    ? EditorNotificationSeverity::Info
                    : EditorNotificationSeverity::Error,
                "Event Binding",
                result.message);
        }
        return result.succeeded;
    }

    static bool ExecuteEventSequenceMutation(
        const EditorDetailsSectionContext& context,
        const EditorObjectHandle& target,
        EditorGimmickEventSequenceMutation mutation) {
        if (context.worldMutations == nullptr ||
            context.transactions == nullptr) {
            return false;
        }
        EditorWorldMutationRequest request{};
        request.kind =
            EditorWorldMutationKind::MutateGimmickEventSequence;
        request.targets = {target};
        request.componentType =
            std::string(kEditorGimmickEventSequenceComponentType);
        request.eventSequenceMutation = std::move(mutation);
        const EditorWorldMutationResult result =
            context.worldMutations->Execute(
                request,
                *context.transactions,
                context.canMutateAuthoring);
        if (result.succeeded && context.onWorldMutated) {
            context.onWorldMutated(result);
        }
        if (context.notifications != nullptr) {
            context.notifications->Push(
                result.succeeded
                    ? EditorNotificationSeverity::Info
                    : EditorNotificationSeverity::Error,
                "Event Sequence",
                result.message);
        }
        return result.succeeded;
    }

    static bool ExecuteComponentEnabledMutation(
        const EditorDetailsSectionContext& context,
        const EditorObjectHandle& target,
        std::string componentType,
        bool enabled) {
        if (context.worldMutations == nullptr || context.transactions == nullptr) {
            return false;
        }
        EditorWorldMutationRequest request{};
        request.kind = EditorWorldMutationKind::SetComponentEnabled;
        request.targets = {target};
        request.componentType = std::move(componentType);
        request.value = enabled;
        const EditorWorldMutationResult result = context.worldMutations->Execute(
            request, *context.transactions, context.canMutateAuthoring);
        if (result.succeeded && context.onWorldMutated) context.onWorldMutated(result);
        if (context.notifications != nullptr) {
            context.notifications->Push(
                result.succeeded
                    ? EditorNotificationSeverity::Info
                    : EditorNotificationSeverity::Error,
                "Scene Components",
                result.message);
        }
        return result.succeeded;
    }

    static bool ExecuteMutation(
        const EditorDetailsSectionContext& context,
        const EditorObjectHandle& target,
        EditorWorldMutationKind kind,
        std::string typeId) {
        if (context.worldMutations == nullptr || context.transactions == nullptr) return false;
        EditorWorldMutationRequest request{};
        request.kind = kind;
        request.targets = {target};
        request.name = std::move(typeId);
        const EditorWorldMutationResult result = context.worldMutations->Execute(
            request, *context.transactions, context.canMutateAuthoring);
        if (result.succeeded && context.onWorldMutated) context.onWorldMutated(result);
        if (context.notifications != nullptr) {
            context.notifications->Push(
                result.succeeded ? EditorNotificationSeverity::Info : EditorNotificationSeverity::Error,
                "Scene Components", result.message);
        }
        return result.succeeded;
    }

    static void DrawAddItem(
        const EditorDetailsSectionContext& context,
        const EditorObjectHandle& target,
        const EditorSceneEntity& entity,
        const EditorSceneComponentDescriptor& descriptor) {
        const bool exists =
            context.sceneWorldProvider->BoundScene()->FindComponent(
                entity, descriptor.typeId) != nullptr;
        ImGui::BeginDisabled(exists);
        if (ImGui::MenuItem(descriptor.displayName.c_str())) {
            ExecuteMutation(
                context,
                target,
                EditorWorldMutationKind::AddComponent,
                descriptor.typeId);
        }
        ImGui::EndDisabled();
    }
};

} // namespace

void RegisterBuiltInEditorDetailsSectionProviders(
    EditorDetailsSectionProviderRegistry& registry) {
    registry.Add(std::make_unique<VfxEffectAssetDetailsSection>());
    registry.Add(std::make_unique<PostProcessPassDetailsSection>());
    registry.Add(std::make_unique<CourseEventDetailsSection>());
    registry.Add(std::make_unique<RenderPresetDetailsSection>());
    registry.Add(std::make_unique<SceneEntityDetailsSection>());
}

} // namespace editor
