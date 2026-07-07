#include "ExistingFeatureProtection.h"

#include <sstream>

#include "EditorPropertyAccessor.h"
#include "EditorPropertyRegistry.h"
#include "EditorAssetRegistry.h"
#include "EditorAssetSelection.h"
#include "EditorSelection.h"
#include "EditorTransactionStack.h"
#include "EditorValidation.h"
#include "../AppRuntimeState.h"
#include "../EffectAssetLoader.h"
#include "../EffectRuntime.h"
#include "../course/CourseAsset.h"

namespace editor {
namespace {

void AddCheck(
    ExistingFeatureProtectionReport& report,
    ExistingFeatureStatus status,
    std::string area,
    std::string name,
    std::string detail) {
    switch (status) {
    case ExistingFeatureStatus::Ok:
        ++report.okCount;
        break;
    case ExistingFeatureStatus::Attention:
        ++report.attentionCount;
        break;
    case ExistingFeatureStatus::Blocked:
        ++report.blockedCount;
        break;
    }

    ExistingFeatureCheck check{};
    check.status = status;
    check.area = std::move(area);
    check.name = std::move(name);
    check.detail = std::move(detail);
    report.checks.push_back(std::move(check));
}

std::string CountText(uint32_t count, const char* singular, const char* plural) {
    std::ostringstream stream;
    stream << count << ' ' << (count == 1 ? singular : plural);
    return stream.str();
}

void AddBooleanCheck(
    ExistingFeatureProtectionReport& report,
    bool ok,
    std::string area,
    std::string name,
    std::string okDetail,
    std::string blockedDetail) {
    AddCheck(
        report,
        ok ? ExistingFeatureStatus::Ok : ExistingFeatureStatus::Blocked,
        std::move(area),
        std::move(name),
        ok ? std::move(okDetail) : std::move(blockedDetail));
}

} // namespace

const char* ToString(ExistingFeatureStatus status) {
    switch (status) {
    case ExistingFeatureStatus::Ok:
        return "Ok";
    case ExistingFeatureStatus::Attention:
        return "Attention";
    case ExistingFeatureStatus::Blocked:
        return "Blocked";
    }
    return "Unknown";
}

ExistingFeatureProtectionReport BuildExistingFeatureProtectionReport(
    const ExistingFeatureProtectionInput& input) {
    ExistingFeatureProtectionReport report{};

    AddBooleanCheck(
        report,
        input.runtimeState != nullptr,
        "Core",
        "Runtime state",
        "AppRuntimeState is available to editor panels.",
        "AppRuntimeState is missing; editor diagnostics cannot protect runtime-backed tools.");

    if (input.selection != nullptr) {
        std::ostringstream detail;
        detail << input.selection->Count() << " selected, revision " << input.selection->Revision();
        AddCheck(
            report,
            ExistingFeatureStatus::Ok,
            "Editor Core",
            "Selection service",
            detail.str());
    } else {
        AddCheck(
            report,
            ExistingFeatureStatus::Blocked,
            "Editor Core",
            "Selection service",
            "EditorSelection is missing; shared editor selection cannot protect migration work.");
    }

    if (input.propertyRegistry != nullptr && input.propertyRegistry->Count() > 0) {
        uint32_t enumCount = 0;
        uint32_t enumMissingOptions = 0;
        for (const EditorPropertyDescriptor& descriptor : input.propertyRegistry->Descriptors()) {
            if (descriptor.kind == EditorPropertyKind::Enum) {
                ++enumCount;
                if (descriptor.enumOptions.empty()) {
                    ++enumMissingOptions;
                }
            }
        }
        std::ostringstream detail;
        detail << input.propertyRegistry->Count()
               << " descriptors, revision "
               << input.propertyRegistry->Revision()
               << " / enum options "
               << (enumCount - enumMissingOptions)
               << "/" << enumCount;
        AddCheck(
            report,
            enumMissingOptions == 0 ? ExistingFeatureStatus::Ok : ExistingFeatureStatus::Attention,
            "Editor Core",
            "Property registry",
            enumMissingOptions == 0
                ? detail.str()
                : detail.str() + " / enum descriptor missing options");
    } else {
        AddCheck(
            report,
            input.propertyRegistry != nullptr ? ExistingFeatureStatus::Attention : ExistingFeatureStatus::Blocked,
            "Editor Core",
            "Property registry",
            input.propertyRegistry != nullptr
                ? "EditorPropertyRegistry is empty; PropertyDelta paths cannot be validated."
                : "EditorPropertyRegistry is missing; generic Details and PropertyDelta validation are unavailable.");
    }

    if (input.propertyAccessor != nullptr) {
        ExistingFeatureStatus accessorStatus = ExistingFeatureStatus::Ok;
        std::ostringstream detail;
        detail << "EditorPropertyAccessor connected";
        const EditorObjectHandle* primary =
            input.selection != nullptr ? input.selection->Primary() : nullptr;
        if (primary != nullptr && input.propertyRegistry != nullptr) {
            const std::vector<const EditorPropertyDescriptor*> descriptors =
                input.propertyRegistry->FindByDomain(primary->domain);
            uint32_t accessibleCount = 0;
            for (const EditorPropertyDescriptor* descriptor : descriptors) {
                if (descriptor != nullptr && input.propertyAccessor->CanAccess(*primary, *descriptor)) {
                    ++accessibleCount;
                }
            }
            detail << " / selected " << ToString(primary->domain)
                   << " accessible " << accessibleCount
                   << "/" << descriptors.size();
            if (!descriptors.empty() && accessibleCount == 0) {
                accessorStatus = ExistingFeatureStatus::Attention;
            }
        }
        AddCheck(
            report,
            accessorStatus,
            "Editor Core",
            "Property accessor",
            detail.str());
    } else {
        AddCheck(
            report,
            ExistingFeatureStatus::Attention,
            "Editor Core",
            "Property accessor",
            "EditorPropertyAccessor is missing; Editor Details can only show descriptors.");
    }

    if (input.assetRegistry != nullptr) {
        std::ostringstream detail;
        detail << input.assetRegistry->Count()
               << " assets, mesh "
               << input.assetRegistry->Count(EditorAssetKind::Mesh)
               << ", revision "
               << input.assetRegistry->Revision();
        AddCheck(
            report,
            input.assetRegistry->Count(EditorAssetKind::Mesh) > 0
                ? ExistingFeatureStatus::Ok
                : ExistingFeatureStatus::Attention,
            "Editor Core",
            "Asset registry",
            input.assetRegistry->Count(EditorAssetKind::Mesh) > 0
                ? detail.str()
                : detail.str() + " / mesh asset candidates are empty");
    } else {
        AddCheck(
            report,
            ExistingFeatureStatus::Attention,
            "Editor Core",
            "Asset registry",
            "EditorAssetRegistry is missing; AssetRef properties cannot be resolved safely.");
    }

    if (input.assetSelection != nullptr) {
        std::ostringstream detail;
        detail << "Revision " << input.assetSelection->Revision();
        if (const EditorAssetHandle* selected = input.assetSelection->Primary()) {
            detail << " / selected " << ToString(selected->kind) << ":" << selected->id;
        } else {
            detail << " / no asset selected";
        }
        AddCheck(
            report,
            ExistingFeatureStatus::Ok,
            "Editor Core",
            "Asset selection",
            detail.str());
    } else {
        AddCheck(
            report,
            ExistingFeatureStatus::Attention,
            "Editor Core",
            "Asset selection",
            "EditorAssetSelection is missing; AssetBrowser cannot drive Details AssetRef edits.");
    }

    if (input.validationReport != nullptr) {
        std::ostringstream detail;
        detail << "Issues " << input.validationReport->issues.size()
               << " / errors " << input.validationReport->errorCount
               << ", warnings " << input.validationReport->warningCount
               << ", info " << input.validationReport->infoCount;
        AddCheck(
            report,
            input.validationReport->errorCount == 0
                ? ExistingFeatureStatus::Ok
                : ExistingFeatureStatus::Attention,
            "Editor Core",
            "Validation service",
            input.validationReport->errorCount == 0
                ? detail.str()
                : detail.str() + " / authoring data has validation errors");
    } else {
        AddCheck(
            report,
            ExistingFeatureStatus::Attention,
            "Editor Core",
            "Validation service",
            "EditorValidationService report is missing; Details edits are not being validated.");
    }

    AddBooleanCheck(
        report,
        input.effectRuntime != nullptr && input.effectRuntime->IsAttached(),
        "VFX",
        "Effect runtime attachment",
        "EffectRuntime is attached to EffectSystem.",
        "EffectRuntime is missing or detached; VFX inspector and runtime queues are not protected.");

    if (input.transactions != nullptr) {
        const EditorTransactionLegacyMirror& mirror = input.transactions->LegacyMirror();
        const bool bridgeDepthMatches =
            !mirror.active ||
            (mirror.undoDepth == input.transactions->UndoDepth() &&
                mirror.redoDepth == input.transactions->RedoDepth());
        bool propertyLookupOk = true;
        std::ostringstream detail;
        detail << "Editor undo " << input.transactions->UndoDepth()
               << ", redo " << input.transactions->RedoDepth()
               << ", revision " << input.transactions->Revision();
        if (mirror.active) {
            detail << " / legacy " << mirror.label
                   << " undo " << mirror.undoDepth
                   << ", redo " << mirror.redoDepth
                   << ", revision " << mirror.revision;
        }
        if (const EditorTransactionRecord* last = input.transactions->LastTransaction()) {
            detail << " / last " << ToString(last->payload.kind);
            if (last->payload.kind == EditorTransactionPayloadKind::PropertyDelta) {
                detail << " " << last->payload.propertyPath;
                const bool resolved =
                    input.propertyRegistry != nullptr &&
                    input.propertyRegistry->Find(last->target.domain, last->payload.propertyPath) != nullptr;
                detail << (resolved ? " resolved" : " descriptor-missing");
                propertyLookupOk = propertyLookupOk && resolved;
            }
        }
        if (const EditorPropertyChange* staged = input.transactions->StagedPropertyDelta()) {
            detail << " / staged " << staged->propertyPath;
            const bool resolved =
                input.propertyRegistry != nullptr &&
                input.propertyRegistry->Find(staged->target.domain, staged->propertyPath) != nullptr;
            detail << (resolved ? " resolved" : " descriptor-missing");
            propertyLookupOk = propertyLookupOk && resolved;
        }
        AddCheck(
            report,
            (bridgeDepthMatches && propertyLookupOk) ? ExistingFeatureStatus::Ok : ExistingFeatureStatus::Attention,
            "Editor Core",
            "Transaction service",
            bridgeDepthMatches && propertyLookupOk
                ? detail.str()
                : detail.str() + " / bridge depth or property descriptor lookup needs attention");
    } else {
        AddCheck(
            report,
            ExistingFeatureStatus::Blocked,
            "Editor Core",
            "Transaction service",
            "EditorTransactionStack is missing; edits cannot be safely bridged to shared Undo/Redo.");
    }

    if (input.loadedEffectAssets == nullptr) {
        AddCheck(
            report,
            ExistingFeatureStatus::Blocked,
            "VFX",
            "Loaded effect assets",
            "LoadedEffectAsset list is missing.");
    } else {
        const uint32_t assetCount = static_cast<uint32_t>(input.loadedEffectAssets->size());
        uint32_t warningCount = 0;
        uint32_t errorCount = 0;
        for (const LoadedEffectAsset& loaded : *input.loadedEffectAssets) {
            for (const EffectAssetDiagnostic& diagnostic : loaded.diagnostics) {
                if (diagnostic.severity == EffectAssetDiagnosticSeverity::Error) {
                    ++errorCount;
                } else if (diagnostic.severity == EffectAssetDiagnosticSeverity::Warning) {
                    ++warningCount;
                }
            }
        }

        ExistingFeatureStatus status = ExistingFeatureStatus::Ok;
        if (assetCount == 0 || errorCount > 0) {
            status = ExistingFeatureStatus::Attention;
        } else if (warningCount > 0) {
            status = ExistingFeatureStatus::Attention;
        }

        std::ostringstream detail;
        detail << CountText(assetCount, "asset", "assets");
        if (errorCount > 0 || warningCount > 0) {
            detail << ", " << errorCount << " errors, " << warningCount << " warnings";
        } else {
            detail << ", diagnostics clean";
        }
        AddCheck(report, status, "VFX", "Effect asset diagnostics", detail.str());
    }

    AddCheck(
        report,
        ExistingFeatureStatus::Ok,
        "VFX",
        "Runtime/render boundary",
        "Guard is read-only and does not mutate EffectAsset storage, runtime queues, or renderer inputs.");

    AddBooleanCheck(
        report,
        input.renderGraphDescription != nullptr && input.renderPassDebugInfo != nullptr,
        "RenderGraph",
        "Debug data connection",
        input.renderPassDebugInfo != nullptr
            ? CountText(static_cast<uint32_t>(input.renderPassDebugInfo->size()), "pass", "passes")
            : "RenderGraph debug pointers are connected.",
        "RenderGraph debug data is missing.");

    if (input.renderGraphError != nullptr && !input.renderGraphError->empty()) {
        AddCheck(
            report,
            ExistingFeatureStatus::Attention,
            "RenderGraph",
            "Validation error",
            *input.renderGraphError);
    } else {
        AddCheck(
            report,
            ExistingFeatureStatus::Ok,
            "RenderGraph",
            "Validation error",
            "No RenderGraph error reported.");
    }

    AddBooleanCheck(
        report,
        input.course != nullptr,
        "Course",
        "Course asset",
        "Course asset is available to Course Timeline.",
        "Course asset is missing; Course Timeline cannot be protected.");

    if (input.course != nullptr) {
        AddCheck(
            report,
            input.course->IsValid() ? ExistingFeatureStatus::Ok : ExistingFeatureStatus::Attention,
            "Course",
            "Rail validity",
            input.course->IsValid()
                ? "Course has enough rail points for runtime use."
                : "Course has fewer than two rail points.");

        AddCheck(
            report,
            input.courseRailLength > 0.0f ? ExistingFeatureStatus::Ok : ExistingFeatureStatus::Attention,
            "Course",
            "Rail length",
            input.courseRailLength > 0.0f
                ? "Rail length is positive."
                : "Rail length is zero or unavailable.");
    }

    AddBooleanCheck(
        report,
        input.coursePath != nullptr && !input.coursePath->empty(),
        "Course",
        "Course path",
        input.coursePath != nullptr ? *input.coursePath : std::string{"Course path is available."},
        "Course path is missing.");

    AddBooleanCheck(
        report,
        input.courseLoadStatus != nullptr,
        "Course",
        "Load status",
        input.courseLoadStatus != nullptr ? *input.courseLoadStatus : std::string{"Course status is available."},
        "Course load status pointer is missing.");

    AddBooleanCheck(
        report,
        input.hasSaveCourseCommand &&
            input.hasApplyCourseCommand &&
            input.hasReloadCourseCommand &&
            input.hasTeleportCourseCommand,
        "Course",
        "Authoring commands",
        "Save, Apply, Reload, and Teleport callbacks are connected.",
        "One or more Course authoring callbacks are missing.");

    if (input.runtimeState != nullptr) {
        std::ostringstream detail;
        detail << "Undo " << input.runtimeState->terrain.courseObjectUndoDepth
               << ", Redo " << input.runtimeState->terrain.courseObjectRedoDepth
               << ", Revision " << input.runtimeState->terrain.courseObjectEditRevision;
        AddCheck(
            report,
            ExistingFeatureStatus::Ok,
            "Course",
            "Object edit history",
            detail.str());
    }

    AddCheck(
        report,
        ExistingFeatureStatus::Ok,
        "Editor Core",
        "Migration mode",
        "Existing panels stay authoritative; guard only observes until shared Selection/Transaction are bridged.");

    return report;
}

} // namespace editor
