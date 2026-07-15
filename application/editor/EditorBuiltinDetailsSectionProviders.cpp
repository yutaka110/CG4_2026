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
        ImGui::Separator();

        for (const EditorSceneComponent& component : entity->components) {
            ImGui::PushID(component.typeId.c_str());
            const bool required = component.typeId == kEditorTransformComponentType;
            ImGui::Text("%s", DisplayNameForEditorSceneComponent(component.typeId));
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
            for (const EditorSceneProperty& property : component.properties) {
                ImGui::PushID(property.name.c_str());
                ImGui::TextUnformatted(property.name.c_str());
                ImGui::SameLine(120.0f);
                std::array<char, 512> value{};
                std::snprintf(value.data(), value.size(), "%s", property.value.c_str());
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::BeginDisabled(!context.canMutateAuthoring);
                const bool commit = ImGui::InputText(
                    "##Value", value.data(), value.size(), ImGuiInputTextFlags_EnterReturnsTrue);
                ImGui::EndDisabled();
                ImGui::PopID();
                if (commit && ExecutePropertyMutation(
                        context, *target, component.typeId, property.name, value.data())) {
                    ImGui::PopID();
                    return;
                }
            }
            for (const EditorSceneObjectReference& reference : component.references) {
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
            DrawAddItem(context, *target, *entity, kEditorMeshRendererComponentType);
            DrawAddItem(context, *target, *entity, kEditorVfxComponentType);
            DrawAddItem(context, *target, *entity, kEditorAudioSourceComponentType);
            DrawAddItem(context, *target, *entity, kEditorDirectionalLightComponentType);
            DrawAddItem(context, *target, *entity, kEditorPointLightComponentType);
            DrawAddItem(context, *target, *entity, kEditorSpotLightComponentType);
            ImGui::EndPopup();
        }
        ImGui::EndDisabled();
    }

private:
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
        std::string_view typeId) {
        const bool exists = context.sceneWorldProvider->BoundScene()->FindComponent(entity, typeId) != nullptr;
        ImGui::BeginDisabled(exists);
        if (ImGui::MenuItem(DisplayNameForEditorSceneComponent(typeId))) {
            ExecuteMutation(context, target, EditorWorldMutationKind::AddComponent, std::string(typeId));
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
