#include "EditorAssetReferenceDiagnosticsAdapter.h"

#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace editor {
namespace {

EditorObjectHandle MakeAssetDiagnosticHandle(const EditorAssetRecord& record) {
    EditorObjectHandle handle{};
    handle.domain = EditorDomainId::Asset;
    handle.stableId = BuildEditorAssetDiagnosticStableId(record.kind, record.id);
    handle.displayName = std::string(ToString(record.kind)) + ":" + record.id;
    return handle;
}

void AddIssue(
    EditorValidationReport& report,
    EditorValidationSeverity severity,
    EditorObjectHandle target,
    std::string propertyPath,
    std::string title,
    std::string message) {
    EditorValidationIssue issue{};
    issue.severity = severity;
    issue.target = std::move(target);
    issue.propertyPath = std::move(propertyPath);
    issue.title = std::move(title);
    issue.message = std::move(message);
    report.AddIssue(std::move(issue));
}

} // namespace

EditorAssetReferenceDiagnosticsAdapter::EditorAssetReferenceDiagnosticsAdapter(
    const EditorAssetRegistry* assetRegistry)
    : assetRegistry_(assetRegistry) {
}

void EditorAssetReferenceDiagnosticsAdapter::Validate(EditorValidationReport& report) const {
    if (assetRegistry_ == nullptr) {
        return;
    }

    const std::vector<std::string> duplicateGuids = assetRegistry_->DuplicateGuids();
    const std::unordered_set<std::string> duplicates(
        duplicateGuids.begin(), duplicateGuids.end());

    for (const EditorAssetRecord& record : assetRegistry_->Records()) {
        const EditorObjectHandle target = MakeAssetDiagnosticHandle(record);
        if (!record.runtimeOnly && (!record.hasMetadata || record.provisionalGuid)) {
            AddIssue(
                report,
                EditorValidationSeverity::Warning,
                target,
                "metadataPath",
                "Asset metadata migration recommended",
                std::string(ToString(record.kind)) +
                    ":" +
                    record.id +
                    " is using path fallback or provisional GUID metadata. Create a durable .meta file before commercial rename/move workflows.");
        }
        if (!record.guid.empty() && duplicates.find(record.guid) != duplicates.end()) {
            AddIssue(
                report,
                EditorValidationSeverity::Error,
                target,
                "guid",
                "Duplicate durable Asset GUID",
                std::string(ToString(record.kind)) + ":" + record.id +
                    " shares GUID " + record.guid +
                    ". Rename/Move is blocked until identity is regenerated or repaired.");
        }
        if (!record.pathOnlyReferences.empty()) {
            AddIssue(
                report,
                EditorValidationSeverity::Warning,
                target,
                "pathOnlyReferences",
                "Path-only asset reference",
                std::string(ToString(record.kind)) + ":" + record.id + " contains " +
                    std::to_string(record.pathOnlyReferences.size()) +
                    " path-only reference(s). Run Repair Path-only References to convert resolvable entries to GUID references.");
        }
        if (record.missing) {
            AddIssue(
                report,
                record.referenceable ? EditorValidationSeverity::Error : EditorValidationSeverity::Warning,
                target,
                "sourcePath",
                "Missing asset file",
                std::string(ToString(record.kind)) +
                    ":" +
                    record.id +
                    " is indexed, but the source file is missing: " +
                    (record.sourcePath.empty() ? std::string("-") : record.sourcePath));
        }

        for (const std::string& dependency : record.dependencies) {
            EditorAssetDependencyToken dependencyToken{};
            if (!ParseEditorAssetDependencyToken(dependency, dependencyToken)) {
                AddIssue(
                    report,
                    EditorValidationSeverity::Warning,
                    target,
                    "dependencies",
                    "Malformed asset dependency",
                    "Dependency token cannot be resolved: " + dependency);
                continue;
            }

            const EditorAssetRecord* referenced =
                assetRegistry_->Find(dependencyToken.kind, dependencyToken.id);
            if (referenced == nullptr) {
                AddIssue(
                    report,
                    EditorValidationSeverity::Warning,
                    target,
                    "dependencies",
                    "Missing asset dependency",
                    std::string(ToString(record.kind)) +
                        ":" +
                        record.id +
                        " depends on unregistered asset " +
                        dependency);
            } else if (referenced->missing) {
                AddIssue(
                    report,
                    EditorValidationSeverity::Error,
                    target,
                    "dependencies",
                    "Missing dependency file",
                    std::string(ToString(record.kind)) +
                        ":" +
                        record.id +
                        " depends on missing asset file " +
                        dependency +
                        " at " +
                        (referenced->sourcePath.empty() ? std::string("-") : referenced->sourcePath));
            }
        }
        for (const std::string& dependencyGuid : record.guidDependencies) {
            const std::vector<const EditorAssetRecord*> matches =
                assetRegistry_->FindAllByGuid(dependencyGuid);
            if (matches.empty()) {
                AddIssue(
                    report,
                    EditorValidationSeverity::Error,
                    target,
                    "guidDependencies",
                    "Missing GUID asset reference",
                    std::string(ToString(record.kind)) + ":" + record.id +
                        " references missing durable GUID " + dependencyGuid + ".");
            } else if (matches.size() > 1) {
                AddIssue(
                    report,
                    EditorValidationSeverity::Error,
                    target,
                    "guidDependencies",
                    "Ambiguous GUID asset reference",
                    "GUID reference " + dependencyGuid + " resolves to multiple assets.");
            }
        }
    }

    for (const EditorAssetRedirect& redirect : assetRegistry_->Redirects()) {
        if (assetRegistry_->FindAllByGuid(redirect.guid).empty()) {
            EditorAssetRecord placeholder{};
            placeholder.kind = redirect.kind;
            placeholder.id = redirect.currentId.empty() ? redirect.oldId : redirect.currentId;
            AddIssue(
                report,
                EditorValidationSeverity::Warning,
                MakeAssetDiagnosticHandle(placeholder),
                "redirect",
                "Stale Asset redirect",
                "Redirect from " + redirect.oldId + " targets missing GUID " + redirect.guid + ".");
        }
    }
}

std::string BuildEditorAssetDiagnosticStableId(EditorAssetKind kind, std::string_view id) {
    std::string stableId = "asset:";
    stableId += ToString(kind);
    stableId += ':';
    stableId.append(id.data(), id.size());
    return stableId;
}

} // namespace editor
