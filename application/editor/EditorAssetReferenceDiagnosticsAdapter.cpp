#include "EditorAssetReferenceDiagnosticsAdapter.h"

#include <string>
#include <string_view>
#include <utility>

namespace editor {
namespace {

EditorAssetKind ParseAssetKind(std::string_view text) {
    if (text == ToString(EditorAssetKind::Mesh)) {
        return EditorAssetKind::Mesh;
    }
    if (text == ToString(EditorAssetKind::Effect)) {
        return EditorAssetKind::Effect;
    }
    if (text == ToString(EditorAssetKind::Course)) {
        return EditorAssetKind::Course;
    }
    if (text == ToString(EditorAssetKind::Texture)) {
        return EditorAssetKind::Texture;
    }
    if (text == ToString(EditorAssetKind::Audio)) {
        return EditorAssetKind::Audio;
    }
    return EditorAssetKind::Unknown;
}

bool ParseDependencyToken(
    std::string_view token,
    EditorAssetKind& outKind,
    std::string_view& outId) {
    const std::size_t separator = token.find(':');
    if (separator == std::string_view::npos) {
        return false;
    }

    outKind = ParseAssetKind(token.substr(0, separator));
    outId = token.substr(separator + 1);
    return outKind != EditorAssetKind::Unknown && !outId.empty();
}

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

    for (const EditorAssetRecord& record : assetRegistry_->Records()) {
        const EditorObjectHandle target = MakeAssetDiagnosticHandle(record);
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
            EditorAssetKind dependencyKind = EditorAssetKind::Unknown;
            std::string_view dependencyId;
            if (!ParseDependencyToken(dependency, dependencyKind, dependencyId)) {
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
                assetRegistry_->Find(dependencyKind, dependencyId);
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
