#include "ExistingFeatureProtection.h"

#include <sstream>

#include "CourseDocumentAdapter.h"
#include "EditorPropertyAccessor.h"
#include "EditorPropertyRegistry.h"
#include "EditorAssetRegistry.h"
#include "EditorAssetSelection.h"
#include "EditorAuthoringMutationGuard.h"
#include "EditorDirtyStateService.h"
#include "EditorDocumentLifecycleService.h"
#include "EditorLayoutService.h"
#include "EditorLayoutPersistenceService.h"
#include "EditorModalConfirmService.h"
#include "EditorNotificationCenter.h"
#include "EditorPanelLayoutService.h"
#include "EditorPanelRegistry.h"
#include "EditorPlaySessionState.h"
#include "EditorRailRuntimePause.h"
#include "EditorRuntimeInspector.h"
#include "EditorSaveApplyPolicy.h"
#include "EditorSelection.h"
#include "EditorTransformGizmoService.h"
#include "EditorTransactionStack.h"
#include "EditorValidation.h"
#include "EditorViewportAuthoringInputGuard.h"
#include "EditorViewportInteractionService.h"
#include "EditorViewportSelectionBridge.h"
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
               << ", meta "
               << input.assetRegistry->CountWithMetadata()
               << ", autoGuid "
               << input.assetRegistry->CountWithProvisionalGuid()
               << ", deps "
               << input.assetRegistry->CountWithDependencies()
               << ", missing "
               << input.assetRegistry->CountMissing()
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
        const bool identityReady =
            input.assetRegistry->Count() > 0 &&
            input.assetRegistry->CountWithProvisionalGuid() <= input.assetRegistry->Count();
        AddCheck(
            report,
            identityReady
                ? ExistingFeatureStatus::Ok
                : ExistingFeatureStatus::Attention,
            "Editor Core",
            "Asset identity metadata",
            detail.str() + " / GUID identity is available with path fallback.");
        if (input.assetRegistry->CountMissing() > 0) {
            AddCheck(
                report,
                ExistingFeatureStatus::Attention,
                "Editor Core",
                "Missing assets",
                "One or more indexed assets are marked missing.");
        }
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

    if (input.courseDocument != nullptr) {
        const CourseDocumentState& state = input.courseDocument->State();
        std::ostringstream detail;
        detail << (state.open ? "open" : "closed")
               << ", "
               << (state.dirty ? "dirty" : "clean")
               << ", "
               << (state.reopenAvailable ? "reopen-ready" : "reopen-unavailable");
        if (!state.displayName.empty()) {
            detail << ", " << state.displayName;
        }
        if (!state.path.empty()) {
            detail << ", path " << state.path;
        }
        AddCheck(
            report,
            state.open ? ExistingFeatureStatus::Ok : ExistingFeatureStatus::Attention,
            "Editor Core",
            "Course document adapter",
            detail.str());
    } else {
        AddCheck(
            report,
            ExistingFeatureStatus::Attention,
            "Editor Core",
            "Course document adapter",
            "CourseDocumentAdapter is missing; Course document open/close state is not visible to Editor Core.");
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

    if (input.dirtyState != nullptr) {
        std::ostringstream detail;
        detail << input.dirtyState->Count()
               << " dirty records, revision "
               << input.dirtyState->Revision()
               << ", summary "
               << input.dirtyState->Summary();
        AddCheck(
            report,
            ExistingFeatureStatus::Ok,
            "Editor Core",
            "Dirty state service",
            detail.str());
    } else {
        AddCheck(
            report,
            ExistingFeatureStatus::Attention,
            "Editor Core",
            "Dirty state service",
            "EditorDirtyStateService is missing; save/apply state cannot be shared.");
    }

    if (input.documentLifecycle != nullptr) {
        std::ostringstream detail;
        detail << "revision "
               << input.documentLifecycle->Revision()
               << ", last action "
               << ToString(input.documentLifecycle->LastAction());
        if (!input.documentLifecycle->LastMessage().empty()) {
            detail << ", "
                   << input.documentLifecycle->LastMessage();
        }
        AddCheck(
            report,
            ExistingFeatureStatus::Ok,
            "Editor Core",
            "Document lifecycle",
            detail.str());
    } else {
        AddCheck(
            report,
            ExistingFeatureStatus::Attention,
            "Editor Core",
            "Document lifecycle",
            "EditorDocumentLifecycleService is missing; dirty document operations are not centralized.");
    }

    if (input.layout != nullptr) {
        std::ostringstream detail;
        detail << "top reserved "
               << input.layout->TopReservedHeight()
               << ", bottom reserved "
               << input.layout->BottomReservedHeight()
               << ", toolbar "
               << (input.layout->ToolbarVisible() ? "visible" : "hidden")
               << ", tabs "
               << (input.layout->DocumentTabsVisible() ? "visible" : "hidden")
               << ", status "
               << (input.layout->StatusBarVisible() ? "visible" : "hidden");
        AddCheck(
            report,
            ExistingFeatureStatus::Ok,
            "Editor Core",
            "Layout service",
            detail.str());
    } else {
        AddCheck(
            report,
            ExistingFeatureStatus::Attention,
            "Editor Core",
            "Layout service",
            "EditorLayoutService is missing; editor chrome reservation is not centralized.");
    }

    if (input.layoutPersistence != nullptr) {
        std::ostringstream detail;
        detail << (input.layoutPersistence->Loaded() ? "loaded" : "not loaded")
               << ", "
               << (input.layoutPersistence->Dirty() ? "dirty" : "clean")
               << ", revision "
               << input.layoutPersistence->Revision()
               << ", path "
               << input.layoutPersistence->Path().generic_string();
        if (!input.layoutPersistence->StatusMessage().empty()) {
            detail << ", "
                   << input.layoutPersistence->StatusMessage();
        }
        AddCheck(
            report,
            input.layoutPersistence->Loaded()
                ? (input.layoutPersistence->LastLoadValid()
                    ? ExistingFeatureStatus::Ok
                    : ExistingFeatureStatus::Attention)
                : ExistingFeatureStatus::Attention,
            "Editor Core",
            "Layout persistence service",
            detail.str());
    } else {
        AddCheck(
            report,
            ExistingFeatureStatus::Attention,
            "Editor Core",
            "Layout persistence service",
            "EditorLayoutPersistenceService is missing; editor workspace layout cannot be restored.");
    }

    if (input.panelLayout != nullptr) {
        const EditorPanelRect& content = input.panelLayout->ContentRect();
        const EditorPanelRect& leftSidebar = input.panelLayout->LeftSidebarRect();
        const EditorPanelRect& inspector = input.panelLayout->InspectorRect();
        const EditorPanelRect& contentBrowser = input.panelLayout->ContentBrowserRect();
        const EditorPanelRect& diagnostics = input.panelLayout->DiagnosticsRect();
        const EditorPanelRect& viewport = input.panelLayout->ViewportRect();
        std::ostringstream detail;
        detail << "content "
               << content.width
               << "x"
               << content.height
               << ", inspector "
               << inspector.width
               << "x"
               << inspector.height
               << ", left "
               << leftSidebar.width
               << "x"
               << leftSidebar.height
               << ", content browser "
               << contentBrowser.width
               << "x"
               << contentBrowser.height
               << ", diagnostics "
               << diagnostics.width
               << "x"
               << diagnostics.height
               << ", viewport "
               << viewport.width
               << "x"
               << viewport.height;
        AddCheck(
            report,
            content.Valid() && viewport.Valid()
                ? ExistingFeatureStatus::Ok
                : ExistingFeatureStatus::Attention,
            "Editor Core",
            "Panel layout service",
            content.Valid() && viewport.Valid()
                ? detail.str()
                : detail.str() + " / content or viewport rect is unavailable");
    } else {
        AddCheck(
            report,
            ExistingFeatureStatus::Attention,
            "Editor Core",
            "Panel layout service",
            "EditorPanelLayoutService is missing; inspector and diagnostics layout is not centralized.");
    }

    if (input.panelRegistry != nullptr) {
        const std::size_t leftCount =
            input.panelRegistry->Count(EditorPanelHostArea::LeftSidebar);
        const std::size_t inspectorCount =
            input.panelRegistry->Count(EditorPanelHostArea::RightInspector);
        const std::size_t contentCount =
            input.panelRegistry->Count(EditorPanelHostArea::ContentBrowser);
        const std::size_t bottomCount =
            input.panelRegistry->Count(EditorPanelHostArea::BottomDock);
        std::ostringstream detail;
        detail << "left "
               << leftCount
               << ", right "
               << inspectorCount
               << ", content "
               << contentCount
               << ", bottom "
               << bottomCount
               << ", total "
               << input.panelRegistry->Count()
               << ", revision "
               << input.panelRegistry->Revision();
        AddCheck(
            report,
            leftCount > 0 && inspectorCount > 0 && contentCount > 0 && bottomCount > 0
                ? ExistingFeatureStatus::Ok
                : ExistingFeatureStatus::Attention,
            "Editor Core",
            "Panel host multi-area registry",
            leftCount > 0 && inspectorCount > 0 && contentCount > 0 && bottomCount > 0
                ? detail.str()
                : detail.str() + " / one or more editor host areas have no panels");
    } else {
        AddCheck(
            report,
            ExistingFeatureStatus::Attention,
            "Editor Core",
            "Panel host multi-area registry",
            "EditorPanelRegistry is missing; multi-area editor panel hosting cannot be guarded.");
    }

    if (input.notifications != nullptr) {
        std::ostringstream detail;
        detail << input.notifications->Count()
               << " notifications, revision "
               << input.notifications->Revision();
        if (const EditorNotification* latest = input.notifications->Latest()) {
            detail << ", latest "
                   << ToString(latest->severity)
                   << " "
                   << latest->source;
        }
        AddCheck(
            report,
            ExistingFeatureStatus::Ok,
            "Editor Core",
            "Notification center",
            detail.str());
    } else {
        AddCheck(
            report,
            ExistingFeatureStatus::Attention,
            "Editor Core",
            "Notification center",
            "EditorNotificationCenter is missing; command and policy results are not retained.");
    }

    if (input.confirmService != nullptr) {
        std::ostringstream detail;
        detail << "revision "
               << input.confirmService->Revision()
               << ", "
               << (input.confirmService->HasPending() ? "pending confirmation" : "idle");
        if (const EditorModalConfirmRequest* pending = input.confirmService->Pending()) {
            detail << ", "
                   << ToString(pending->severity)
                   << " "
                   << pending->title;
        }
        AddCheck(
            report,
            ExistingFeatureStatus::Ok,
            "Editor Core",
            "Modal confirmation service",
            detail.str());
    } else {
        AddCheck(
            report,
            ExistingFeatureStatus::Attention,
            "Editor Core",
            "Modal confirmation service",
            "EditorModalConfirmService is missing; destructive editor operations cannot be gated.");
    }

    if (input.saveApplyPolicy != nullptr) {
        AddCheck(
            report,
            ExistingFeatureStatus::Ok,
            "Editor Core",
            "Save/apply policy",
            BuildEditorSaveApplyPolicySummary(*input.saveApplyPolicy));
    } else {
        AddCheck(
            report,
            ExistingFeatureStatus::Attention,
            "Editor Core",
            "Save/apply policy",
            "EditorSaveApplyPolicy input is missing; commands may diverge from status/guard display.");
    }

    if (input.runtimeInspector != nullptr) {
        std::ostringstream detail;
        detail << input.runtimeInspector->Count()
               << " runtime watch records, revision "
               << input.runtimeInspector->Revision()
               << ", mode "
               << (input.runtimeInspector->ReadOnly() ? "read-only" : "editable");
        AddCheck(
            report,
            input.runtimeInspector->ReadOnly() ? ExistingFeatureStatus::Ok : ExistingFeatureStatus::Blocked,
            "Editor Core",
            "Runtime inspector",
            input.runtimeInspector->ReadOnly()
                ? detail.str()
                : detail.str() + " / runtime inspector must not mutate authoring or runtime state");
    } else {
        AddCheck(
            report,
            ExistingFeatureStatus::Attention,
            "Editor Core",
            "Runtime inspector",
            "EditorRuntimeInspector is missing; runtime watch data is not unified.");
    }

    if (input.playSession != nullptr) {
        const EditorAuthoringMutationGuard mutationGuard =
            MakeEditorAuthoringMutationGuard(input.playSession);
        const char* isolationState = "inactive";
        if (input.playSession->RuntimeIsolationSnapshotActive()) {
            isolationState = "snapshot-active";
        } else if (input.playSession->RuntimeIsolationPending()) {
            isolationState = "pending";
        } else if (input.playSession->RuntimeIsolationRestored()) {
            isolationState = "restored";
        }
        std::ostringstream detail;
        detail << "Mode " << ToString(input.playSession->Mode())
               << ", serial " << input.playSession->SessionSerial()
               << ", frames " << input.playSession->FrameCount()
               << ", runtime isolation " << isolationState
               << ", authoring mutation "
               << (mutationGuard.CanMutate() ? "open" : "locked");
        AddCheck(
            report,
            ExistingFeatureStatus::Ok,
            "Editor Core",
            "Play session boundary",
            detail.str());
    } else {
        AddCheck(
            report,
            ExistingFeatureStatus::Attention,
            "Editor Core",
            "Play session boundary",
            "EditorPlaySessionState is missing; PIE/SIM boundary is not visible to editor services.");
    }

    if (input.runtimeState != nullptr) {
        const EditorViewportAuthoringInputGuard inputGuard =
            MakeEditorViewportAuthoringInputGuard(
                !input.runtimeState->terrain.courseObjectAuthoringInputLocked);
        const bool playActive =
            input.playSession != nullptr && input.playSession->IsActive();
        const bool protectedState =
            playActive ? !inputGuard.CanMutate() : inputGuard.CanMutate();
        std::ostringstream detail;
        detail << "Course object viewport authoring input "
               << inputGuard.StateLabel()
               << ", play active "
               << (playActive ? "yes" : "no");
        AddCheck(
            report,
            protectedState ? ExistingFeatureStatus::Ok : ExistingFeatureStatus::Attention,
            "Editor Core",
            "Viewport authoring input",
            protectedState
                ? detail.str()
                : detail.str() + " / expected lock state does not match Play/Sim boundary");
    } else {
        AddCheck(
            report,
            ExistingFeatureStatus::Attention,
            "Editor Core",
            "Viewport authoring input",
            "Runtime state is missing; viewport authoring input guard cannot be observed.");
    }

    if (input.viewportInteraction != nullptr) {
        const EditorViewportInteractionState& state = input.viewportInteraction->State();
        const bool lockMatchesRuntime =
            input.runtimeState == nullptr ||
            input.runtimeState->terrain.courseObjectAuthoringInputLocked ==
                input.viewportInteraction->AuthoringInputLocked();
        std::ostringstream detail;
        detail << input.viewportInteraction->BoundaryLabel()
               << ", "
               << input.viewportInteraction->ViewportInputLabel()
               << ", "
               << input.viewportInteraction->AuthoringLabel()
               << ", mouse "
               << state.mouseX
               << ","
               << state.mouseY
               << ", revision "
               << state.revision;
        AddCheck(
            report,
            input.viewportInteraction->ViewportAvailable() && lockMatchesRuntime
                ? ExistingFeatureStatus::Ok
                : ExistingFeatureStatus::Attention,
            "Editor Core",
            "Viewport interaction service",
            input.viewportInteraction->ViewportAvailable() && lockMatchesRuntime
                ? detail.str()
                : detail.str() + " / viewport boundary or runtime lock needs attention");
    } else {
        AddCheck(
            report,
            ExistingFeatureStatus::Attention,
            "Editor Core",
            "Viewport interaction service",
            "EditorViewportInteractionService is missing; viewport input boundary is not centralized.");
    }

    if (input.viewportSelectionBridge != nullptr) {
        const EditorViewportSelectionBridgeState& state =
            input.viewportSelectionBridge->State();
        std::ostringstream detail;
        detail << input.viewportSelectionBridge->BoundaryLabel()
               << ", "
               << input.viewportSelectionBridge->CourseSelectionLabel()
               << ", "
               << input.viewportSelectionBridge->RequestLabel()
               << ", picks "
               << state.pickResultCount
               << ", handles "
               << state.bridgedHandleCount
               << ", primary "
               << ToString(state.primaryPickSource)
               << ", revision "
               << state.revision;
        AddCheck(
            report,
            state.selectionConnected && state.viewportBoundaryConnected
                ? ExistingFeatureStatus::Ok
                : ExistingFeatureStatus::Attention,
            "Editor Core",
            "Viewport selection bridge",
            state.selectionConnected && state.viewportBoundaryConnected
                ? detail.str()
                : detail.str() + " / selection or viewport boundary is not connected");
    } else {
        AddCheck(
            report,
            ExistingFeatureStatus::Attention,
            "Editor Core",
            "Viewport selection bridge",
            "EditorViewportSelectionBridge is missing; viewport picks are not routed through EditorSelection.");
    }

    if (input.transformGizmo != nullptr) {
        const EditorTransformGizmoState& state = input.transformGizmo->State();
        std::ostringstream detail;
        detail << input.transformGizmo->TargetLabel()
               << ", "
               << input.transformGizmo->ModeLabel()
               << ", "
               << input.transformGizmo->AxisLabel()
               << ", "
               << input.transformGizmo->ManipulationLabel()
               << ", snap "
               << (state.snapEnabled ? "on" : "off")
               << ", tx "
               << (state.transactionConnected ? "connected" : "missing")
               << ", undo "
               << state.undoDepth
               << ", redo "
               << state.redoDepth
               << ", revision "
               << state.revision;
        const bool connected =
            state.selectionConnected &&
            state.viewportBoundaryConnected &&
            state.selectionRequestConnected &&
            state.transactionConnected;
        AddCheck(
            report,
            connected
                ? ExistingFeatureStatus::Ok
                : ExistingFeatureStatus::Attention,
            "Editor Core",
            "Transform gizmo service",
            connected
                ? detail.str()
                : detail.str() + " / selection, request, transaction, or viewport boundary is not connected");
    } else {
        AddCheck(
            report,
            ExistingFeatureStatus::Attention,
            "Editor Core",
            "Transform gizmo service",
            "EditorTransformGizmoService is missing; transform gizmo state is not centralized.");
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
            input.hasTeleportCourseCommand &&
            input.hasFreezeCourseCommand,
        "Course",
        "Authoring commands",
        "Save, Apply, Reload, Teleport, and Freeze callbacks are connected.",
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

    if (input.railRuntimePause != nullptr) {
        const EditorRailRuntimePauseState& state = input.railRuntimePause->State();
        std::ostringstream detail;
        detail << input.railRuntimePause->StatusLabel()
               << ", distance " << state.distance
               << ", speed " << state.speed
               << ", frozenFrames " << state.frozenFrames
               << ", revision " << state.revision;
        AddCheck(
            report,
            ExistingFeatureStatus::Ok,
            "Course",
            "Preview freeze",
            detail.str());
    } else {
        AddCheck(
            report,
            ExistingFeatureStatus::Attention,
            "Course",
            "Preview freeze",
            "EditorRailRuntimePause is missing; rail preview freeze state is not visible to Editor Core.");
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
