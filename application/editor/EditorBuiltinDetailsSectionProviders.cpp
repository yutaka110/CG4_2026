#include "EditorBuiltinDetailsSectionProviders.h"

#include "../../externals/imgui/imgui.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

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

} // namespace

void RegisterBuiltInEditorDetailsSectionProviders(
    EditorDetailsSectionProviderRegistry& registry) {
    registry.Add(std::make_unique<VfxEffectAssetDetailsSection>());
    registry.Add(std::make_unique<PostProcessPassDetailsSection>());
    registry.Add(std::make_unique<CourseEventDetailsSection>());
    registry.Add(std::make_unique<RenderPresetDetailsSection>());
}

} // namespace editor
