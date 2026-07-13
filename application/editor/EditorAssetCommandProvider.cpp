#include "EditorAssetCommandProvider.h"

#include "EditorAssetRegistry.h"
#include "EditorAssetSelection.h"
#include "EditorAssetImportService.h"
#include "EditorAssetMutationExecutor.h"
#include "EditorAssetMutationSafety.h"
#include "EditorCommandContext.h"
#include "EditorCommandRegistry.h"
#include "EditorContext.h"
#include "EditorModalConfirmService.h"
#include "EditorNotificationCenter.h"
#include "EditorPropertyAccessor.h"
#include "EditorPropertyRegistry.h"
#include "EditorSelection.h"
#include "EditorTransactionStack.h"
#include "EditorToolRegistration.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace editor {
namespace {

const EditorAssetRecord* SelectedAssetRecord(const EditorContext& context) {
    if (context.assets == nullptr || context.assetSelection == nullptr) {
        return nullptr;
    }
    const EditorAssetHandle* selected = context.assetSelection->Primary();
    if (selected == nullptr) {
        return nullptr;
    }
    return ResolveEditorAssetHandle(*context.assets, *selected).record;
}

bool WriteAssetMetaFile(const EditorAssetRecord& record, std::string* errorMessage) {
    const std::string metaPath = record.metadataPath.empty() ? record.sourcePath + ".meta" : record.metadataPath;
    if (metaPath.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Selected asset does not have a metadata path.";
        }
        return false;
    }

    std::error_code error;
    const std::filesystem::path path(metaPath);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) {
            if (errorMessage != nullptr) {
                *errorMessage = "Failed to create .meta directory: " + error.message();
            }
            return false;
        }
    }

    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Failed to write .meta file.";
        }
        return false;
    }

    file << "guid=" << record.guid << '\n';
    file << "logicalPath=" << (record.logicalPath.empty() ? record.sourcePath : record.logicalPath) << '\n';
    if (!record.tags.empty()) {
        file << "tags=";
        for (std::size_t i = 0; i < record.tags.size(); ++i) {
            if (i > 0) {
                file << ',';
            }
            file << record.tags[i];
        }
        file << '\n';
    }
    if (!record.dependencies.empty()) {
        file << "dependencies=";
        for (std::size_t i = 0; i < record.dependencies.size(); ++i) {
            if (i > 0) {
                file << ',';
            }
            file << record.dependencies[i];
        }
        file << '\n';
    }
    return true;
}

EditorAssetRecord FirstRepairAsset(
    const EditorAssetRegistry& registry,
    EditorAssetKind kind,
    const EditorAssetHandle* selectedAsset) {
    if (selectedAsset != nullptr &&
        selectedAsset->kind == kind &&
        selectedAsset->referenceable &&
        !selectedAsset->missing) {
        if (const EditorAssetRecord* record = registry.Find(selectedAsset->kind, selectedAsset->id)) {
            return *record;
        }
    }

    for (const EditorAssetRecord* record : registry.List(kind)) {
        if (record != nullptr && record->referenceable && !record->missing) {
            return *record;
        }
    }
    return {};
}

struct MissingAssetRefTarget {
    EditorObjectHandle object{};
    const EditorPropertyDescriptor* descriptor = nullptr;
    EditorPropertyValue currentValue{};
    EditorAssetRecord repairAsset{};
};

MissingAssetRefTarget FindMissingAssetRefTarget(const EditorContext& context) {
    MissingAssetRefTarget result{};
    if (context.selection == nullptr ||
        context.assets == nullptr ||
        context.propertyRegistry == nullptr ||
        context.propertyAccessor == nullptr) {
        return result;
    }

    const EditorObjectHandle* selectedObject = context.selection->Primary();
    if (selectedObject == nullptr) {
        return result;
    }

    const EditorAssetHandle* selectedAsset =
        context.assetSelection != nullptr ? context.assetSelection->Primary() : nullptr;
    const std::vector<const EditorPropertyDescriptor*> properties =
        context.propertyRegistry->FindByDomain(selectedObject->domain);
    for (const EditorPropertyDescriptor* descriptor : properties) {
        if (descriptor == nullptr ||
            descriptor->kind != EditorPropertyKind::AssetRef ||
            descriptor->assetKind == EditorAssetKind::Unknown ||
            descriptor->readOnly) {
            continue;
        }
        EditorPropertyValue value{};
        if (!context.propertyAccessor->Get(*selectedObject, *descriptor, value)) {
            continue;
        }
        if (!value.stringValue.empty()) {
            const EditorAssetRecord* currentAsset =
                context.assets->Find(descriptor->assetKind, value.stringValue);
            if (currentAsset != nullptr && !currentAsset->missing) {
                continue;
            }
        }

        EditorAssetRecord repairAsset =
            FirstRepairAsset(*context.assets, descriptor->assetKind, selectedAsset);
        if (repairAsset.id.empty()) {
            continue;
        }

        result.object = *selectedObject;
        result.descriptor = descriptor;
        result.currentValue = std::move(value);
        result.repairAsset = std::move(repairAsset);
        return result;
    }
    return result;
}

void StageAssetRepairDelta(
    EditorTransactionStack* transactions,
    const EditorObjectHandle& object,
    const EditorPropertyDescriptor& descriptor,
    const EditorPropertyValue& before,
    const EditorPropertyValue& after) {
    if (transactions == nullptr) {
        return;
    }

    EditorPropertyChange change{};
    change.target = object;
    change.propertyPath = descriptor.name;
    change.displayName = "Repair Missing Asset Reference";
    change.valueType = ToString(descriptor.kind);
    change.beforeValue = FormatEditorPropertyValue(descriptor, before);
    change.afterValue = FormatEditorPropertyValue(descriptor, after);
    transactions->StagePropertyDelta(std::move(change));
}

EditorCommandResult BuildSafetyCommandResult(
    const EditorAssetMutationSafetyReport& report) {
    return EditorCommandResult{
        !report.Blocked(),
        FormatEditorAssetMutationSafetyReport(report),
        report.HasWarnings()};
}

EditorCommandResult RunSafetyPreflight(
    const EditorContext& context,
    EditorAssetMutationKind kind) {
    const EditorAssetRecord* selected = SelectedAssetRecord(context);
    if (selected == nullptr || context.assets == nullptr) {
        return EditorCommandResult{false, "No asset is selected."};
    }

    const EditorAssetMutationSafetyReport report =
        EvaluateEditorAssetMutationSafety(*context.assets, *selected, kind);
    return BuildSafetyCommandResult(report);
}

EditorCommandResult RequestDeleteSafetyConfirmation(EditorContext& context) {
    const EditorAssetRecord* selected = SelectedAssetRecord(context);
    if (selected == nullptr || context.assets == nullptr) {
        return EditorCommandResult{false, "No asset is selected."};
    }

    const EditorAssetMutationSafetyReport report =
        EvaluateEditorAssetMutationSafety(
            *context.assets,
            *selected,
            EditorAssetMutationKind::Delete);
    if (report.Blocked()) {
        return BuildSafetyCommandResult(report);
    }

    const EditorAssetKind targetKind = selected->kind;
    const std::string targetId = selected->id;
    const std::string message = FormatEditorAssetMutationSafetyReport(report);

    auto executeDelete =
        [assets = context.assets,
         assetSelection = context.assetSelection,
         transactions = context.transactions,
         notifications = context.notifications,
         targetKind,
         targetId]() {
            if (assets == nullptr) {
                return;
            }
            EditorAssetMutationExecutor executor(*assets);
            const EditorAssetMutationResult result =
                executor.Execute(
                    EditorAssetMutationRequest{
                        EditorAssetMutationKind::Delete,
                        targetKind,
                        targetId,
                        {},
                        {},
                        transactions});
            if (result.succeeded && assetSelection != nullptr) {
                assetSelection->Clear();
            }
            if (notifications != nullptr) {
                notifications->Push(
                    result.succeeded
                        ? (result.warning ? EditorNotificationSeverity::Warning : EditorNotificationSeverity::Info)
                        : EditorNotificationSeverity::Error,
                    "Asset",
                    result.message);
            }
        };

    if (context.confirmService == nullptr) {
        executeDelete();
        return EditorCommandResult{true, message, report.HasWarnings()};
    }

    EditorModalConfirmRequest request{};
    request.severity = report.HasWarnings()
        ? EditorModalConfirmSeverity::Warning
        : EditorModalConfirmSeverity::Info;
    request.source = "Asset";
    request.title = "Delete Asset";
    request.message = message;
    request.confirmLabel = "Delete";
    request.cancelLabel = "Cancel";
    request.onConfirm = std::move(executeDelete);

    if (!context.confirmService->Request(std::move(request))) {
        return EditorCommandResult{false, "Could not queue delete safety confirmation."};
    }
    return EditorCommandResult{true, "Delete asset confirmation requested.", report.HasWarnings()};
}

EditorCommandResult ReimportSelectedAsset(EditorContext& context) {
    const EditorAssetRecord* selected = SelectedAssetRecord(context);
    if (selected == nullptr || context.assets == nullptr) {
        return EditorCommandResult{false, "No asset is selected."};
    }

    const EditorAssetKind kind = selected->kind;
    const std::string id = selected->id;
    EditorAssetImportService importService(*context.assets, context.assetThumbnails);
    const EditorAssetImportResult result = importService.Reimport(kind, id);
    if (result.succeeded &&
        context.assetSelection != nullptr &&
        result.record.kind != EditorAssetKind::Unknown &&
        !result.record.id.empty()) {
        context.assetSelection->SetPrimary(
            MakeEditorAssetHandle(result.record, context.assets->Revision()));
    }
    if (context.notifications != nullptr) {
        context.notifications->Push(
            result.succeeded
                ? (result.warning ? EditorNotificationSeverity::Warning : EditorNotificationSeverity::Info)
                : EditorNotificationSeverity::Error,
            "Asset",
            result.message);
    }
    return EditorCommandResult{result.succeeded, result.message, result.warning};
}

EditorCommandResult BatchMigrateAssetMetadata(EditorContext& context) {
    if (context.assets == nullptr) {
        return EditorCommandResult{false, "Asset registry is unavailable."};
    }

    EditorAssetImportService importService(*context.assets, context.assetThumbnails);
    const EditorAssetImportResult result = importService.BatchMigrateMetadata();
    if (context.assetSelection != nullptr) {
        if (const EditorAssetHandle* selected = context.assetSelection->Primary()) {
            const EditorAssetHandle refreshed =
                RefreshEditorAssetHandle(*context.assets, *selected);
            if (refreshed.Valid()) {
                context.assetSelection->SetPrimary(refreshed);
            }
        }
    }
    if (context.notifications != nullptr) {
        context.notifications->Push(
            result.succeeded
                ? (result.warning ? EditorNotificationSeverity::Warning : EditorNotificationSeverity::Info)
                : EditorNotificationSeverity::Error,
            "Asset",
            result.message);
    }
    return EditorCommandResult{result.succeeded, result.message, result.warning};
}

} // namespace

void EditorAssetCommandProvider::RegisterCommands(EditorContext& context) const {
    if (context.commands == nullptr || context.commandContext == nullptr) {
        return;
    }

    const EditorCommandContext& commandContext = *context.commandContext;

    RegisterEditorToolCommand(
        context,
        EditorCommand{
            "asset.createMeta",
            "Create Asset .meta",
            "Asset",
            "",
            [&context, &commandContext]() {
                const EditorAssetRecord* selected = SelectedAssetRecord(context);
                return commandContext.developerToolsVisible &&
                    selected != nullptr &&
                    !selected->hasMetadata;
            },
            [&context, &commandContext]() {
                if (!commandContext.developerToolsVisible) {
                    return std::string("Developer tools are hidden.");
                }
                const EditorAssetRecord* selected = SelectedAssetRecord(context);
                if (selected == nullptr) {
                    return std::string("No asset is selected.");
                }
                return selected->hasMetadata
                    ? std::string("Selected asset already has .meta metadata.")
                    : std::string();
            },
            [&context]() {
                const EditorAssetRecord* selected = SelectedAssetRecord(context);
                if (selected == nullptr || context.assets == nullptr) {
                    return EditorCommandResult{false, "No asset is selected."};
                }

                EditorAssetRecord updated = *selected;
                EnsureEditorAssetIdentity(updated);
                std::string error;
                if (!WriteAssetMetaFile(updated, &error)) {
                    return EditorCommandResult{false, error.empty() ? std::string("Failed to write .meta.") : error};
                }

                updated.hasMetadata = true;
                updated.provisionalGuid = false;
                context.assets->Register(updated);
                if (context.assetSelection != nullptr) {
                    context.assetSelection->SetPrimary(
                        MakeEditorAssetHandle(updated, context.assets->Revision()));
                }
                return EditorCommandResult{true, "Created .meta for selected asset."};
            }});

    RegisterEditorToolCommand(
        context,
        EditorCommand{
            "asset.renameSafety",
            "Check Asset Rename Safety",
            "Asset",
            "",
            [&context, &commandContext]() {
                return commandContext.developerToolsVisible &&
                    SelectedAssetRecord(context) != nullptr;
            },
            [&context, &commandContext]() {
                if (!commandContext.developerToolsVisible) {
                    return std::string("Developer tools are hidden.");
                }
                return SelectedAssetRecord(context) != nullptr
                    ? std::string()
                    : std::string("No asset is selected.");
            },
            [&context]() {
                return RunSafetyPreflight(context, EditorAssetMutationKind::Rename);
            }});

    RegisterEditorToolCommand(
        context,
        EditorCommand{
            "asset.moveSafety",
            "Check Asset Move Safety",
            "Asset",
            "",
            [&context, &commandContext]() {
                return commandContext.developerToolsVisible &&
                    SelectedAssetRecord(context) != nullptr;
            },
            [&context, &commandContext]() {
                if (!commandContext.developerToolsVisible) {
                    return std::string("Developer tools are hidden.");
                }
                return SelectedAssetRecord(context) != nullptr
                    ? std::string()
                    : std::string("No asset is selected.");
            },
            [&context]() {
                return RunSafetyPreflight(context, EditorAssetMutationKind::Move);
            }});

    RegisterEditorToolCommand(
        context,
        EditorCommand{
            "asset.deleteSafety",
            "Check Asset Delete Safety",
            "Asset",
            "",
            [&context, &commandContext]() {
                return commandContext.developerToolsVisible &&
                    commandContext.canMutateAuthoring &&
                    SelectedAssetRecord(context) != nullptr;
            },
            [&context, &commandContext]() {
                if (!commandContext.developerToolsVisible) {
                    return std::string("Developer tools are hidden.");
                }
                if (!commandContext.canMutateAuthoring) {
                    return std::string("Authoring is locked during Play/Sim.");
                }
                return SelectedAssetRecord(context) != nullptr
                    ? std::string()
                    : std::string("No asset is selected.");
            },
            [&context]() {
                return RequestDeleteSafetyConfirmation(context);
            }});

    RegisterEditorToolCommand(
        context,
        EditorCommand{
            "asset.reimport",
            "Reimport Selected Asset",
            "Asset",
            "",
            [&context, &commandContext]() {
                return commandContext.developerToolsVisible &&
                    commandContext.canMutateAuthoring &&
                    SelectedAssetRecord(context) != nullptr;
            },
            [&context, &commandContext]() {
                if (!commandContext.developerToolsVisible) {
                    return std::string("Developer tools are hidden.");
                }
                if (!commandContext.canMutateAuthoring) {
                    return std::string("Authoring is locked during Play/Sim.");
                }
                return SelectedAssetRecord(context) != nullptr
                    ? std::string()
                    : std::string("No asset is selected.");
            },
            [&context]() {
                return ReimportSelectedAsset(context);
            }});

    RegisterEditorToolCommand(
        context,
        EditorCommand{
            "asset.batchMigrateMetadata",
            "Batch Migrate Asset Metadata",
            "Asset",
            "",
            [&context, &commandContext]() {
                return commandContext.developerToolsVisible &&
                    commandContext.canMutateAuthoring &&
                    context.assets != nullptr &&
                    context.assets->Count() > 0;
            },
            [&context, &commandContext]() {
                if (!commandContext.developerToolsVisible) {
                    return std::string("Developer tools are hidden.");
                }
                if (!commandContext.canMutateAuthoring) {
                    return std::string("Authoring is locked during Play/Sim.");
                }
                return context.assets != nullptr && context.assets->Count() > 0
                    ? std::string()
                    : std::string("No assets are registered.");
            },
            [&context]() {
                return BatchMigrateAssetMetadata(context);
            }});

    RegisterEditorToolCommand(
        context,
        EditorCommand{
            "asset.repairMissingReference",
            "Repair Missing Asset Reference",
            "Asset",
            "",
            [&context, &commandContext]() {
                return commandContext.developerToolsVisible &&
                    commandContext.canMutateAuthoring &&
                    FindMissingAssetRefTarget(context).descriptor != nullptr;
            },
            [&context, &commandContext]() {
                if (!commandContext.developerToolsVisible) {
                    return std::string("Developer tools are hidden.");
                }
                if (!commandContext.canMutateAuthoring) {
                    return std::string("Authoring is locked during Play/Sim.");
                }
                return FindMissingAssetRefTarget(context).descriptor != nullptr
                    ? std::string()
                    : std::string("No selected object has a repairable missing AssetRef.");
            },
            [&context]() {
                MissingAssetRefTarget target = FindMissingAssetRefTarget(context);
                if (target.descriptor == nullptr || context.propertyAccessor == nullptr) {
                    return EditorCommandResult{false, "No selected object has a repairable missing AssetRef."};
                }

                EditorPropertyValue repaired = target.currentValue;
                repaired.stringValue = target.repairAsset.id;
                std::string error;
                if (!context.propertyAccessor->Set(
                        target.object,
                        *target.descriptor,
                        repaired,
                        &error)) {
                    return EditorCommandResult{
                        false,
                        error.empty() ? std::string("Failed to repair missing AssetRef.") : error};
                }
                StageAssetRepairDelta(
                    context.transactions,
                    target.object,
                    *target.descriptor,
                    target.currentValue,
                    repaired);
                return EditorCommandResult{
                    true,
                    "Repaired missing AssetRef with " +
                        std::string(ToString(target.repairAsset.kind)) +
                        ":" +
                        target.repairAsset.id};
            }});
}

} // namespace editor
