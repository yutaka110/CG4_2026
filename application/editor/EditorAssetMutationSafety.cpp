#include "EditorAssetMutationSafety.h"

#include <algorithm>
#include <sstream>
#include <utility>

namespace editor {
namespace {

std::string DependencyToken(const EditorAssetRecord& record) {
    return std::string(ToString(record.kind)) + ":" + record.id;
}

std::string AssetLabel(const EditorAssetRecord& record) {
    return std::string(ToString(record.kind)) + ":" + record.id;
}

void AddIssue(
    EditorAssetMutationSafetyReport& report,
    EditorAssetMutationRisk risk,
    std::string message) {
    EditorAssetMutationSafetyIssue issue{};
    issue.risk = risk;
    issue.message = std::move(message);
    report.issues.push_back(std::move(issue));
    if (risk == EditorAssetMutationRisk::Blocked) {
        report.risk = EditorAssetMutationRisk::Blocked;
    } else if (risk == EditorAssetMutationRisk::Warning &&
        report.risk != EditorAssetMutationRisk::Blocked) {
        report.risk = EditorAssetMutationRisk::Warning;
    }
}

bool RequiresDurableGuid(EditorAssetMutationKind kind) {
    return kind == EditorAssetMutationKind::Rename ||
        kind == EditorAssetMutationKind::Move;
}

void AddDependents(
    const EditorAssetRegistry& registry,
    const EditorAssetRecord& target,
    EditorAssetMutationSafetyReport& report) {
    const std::string token = DependencyToken(target);
    for (const EditorAssetRecord& candidate : registry.Records()) {
        if (candidate.kind == target.kind && candidate.id == target.id) {
            continue;
        }
        const bool dependsOnTarget =
            std::find(candidate.dependencies.begin(), candidate.dependencies.end(), token) !=
            candidate.dependencies.end();
        if (dependsOnTarget) {
            report.dependents.push_back(AssetLabel(candidate));
        }
    }
    report.dependentCount = report.dependents.size();
}

} // namespace

const char* ToString(EditorAssetMutationKind kind) {
    switch (kind) {
    case EditorAssetMutationKind::Rename:
        return "Rename";
    case EditorAssetMutationKind::Move:
        return "Move";
    case EditorAssetMutationKind::Delete:
        return "Delete";
    }
    return "Unknown";
}

const char* ToString(EditorAssetMutationRisk risk) {
    switch (risk) {
    case EditorAssetMutationRisk::Allowed:
        return "Allowed";
    case EditorAssetMutationRisk::Warning:
        return "Warning";
    case EditorAssetMutationRisk::Blocked:
        return "Blocked";
    }
    return "Unknown";
}

EditorAssetMutationSafetyReport EvaluateEditorAssetMutationSafety(
    const EditorAssetRegistry& registry,
    const EditorAssetRecord& target,
    EditorAssetMutationKind kind) {
    EditorAssetMutationSafetyReport report{};
    report.kind = kind;
    report.target = target;
    AddDependents(registry, target, report);

    if (target.id.empty() || target.kind == EditorAssetKind::Unknown) {
        AddIssue(report, EditorAssetMutationRisk::Blocked, "Asset identity is incomplete.");
    }
    if (target.sourcePath.empty()) {
        AddIssue(report, EditorAssetMutationRisk::Blocked, "Asset source path is empty.");
    }
    if (target.missing) {
        AddIssue(report, EditorAssetMutationRisk::Blocked, "Asset source file is missing.");
    }
    if (RequiresDurableGuid(kind) && (!target.hasMetadata || target.provisionalGuid)) {
        AddIssue(
            report,
            EditorAssetMutationRisk::Blocked,
            "Rename/Move requires durable .meta GUID metadata. Run Create Asset .meta first.");
    }
    if (kind == EditorAssetMutationKind::Delete && report.dependentCount > 0) {
        AddIssue(
            report,
            EditorAssetMutationRisk::Blocked,
            "Delete is blocked because other indexed assets depend on this asset.");
    } else if (RequiresDurableGuid(kind) && report.dependentCount > 0) {
        AddIssue(
            report,
            EditorAssetMutationRisk::Blocked,
            "Rename/Move is blocked while indexed dependents exist; reference rewriting is not enabled yet.");
    }
    if (target.runtimeOnly) {
        AddIssue(report, EditorAssetMutationRisk::Blocked, "Runtime-only assets cannot be mutated from the editor.");
    }
    if (kind == EditorAssetMutationKind::Delete && !target.hasMetadata) {
        AddIssue(
            report,
            EditorAssetMutationRisk::Warning,
            "Deleting a legacy path-only asset cannot update GUID-backed references.");
    }
    if (report.issues.empty()) {
        AddIssue(report, EditorAssetMutationRisk::Allowed, "No blocking asset safety issues were found.");
    }
    return report;
}

std::string FormatEditorAssetMutationSafetyReport(
    const EditorAssetMutationSafetyReport& report) {
    std::ostringstream stream;
    stream << ToString(report.kind)
           << " safety "
           << ToString(report.risk)
           << " for "
           << AssetLabel(report.target)
           << " deps="
           << report.dependentCount;
    if (!report.target.guid.empty()) {
        stream << " guid=" << report.target.guid;
    }
    if (!report.target.metadataPath.empty()) {
        stream << " meta=" << report.target.metadataPath;
    }
    for (const EditorAssetMutationSafetyIssue& issue : report.issues) {
        stream << " | " << ToString(issue.risk) << ": " << issue.message;
    }
    if (!report.dependents.empty()) {
        stream << " | dependents:";
        for (const std::string& dependent : report.dependents) {
            stream << ' ' << dependent;
        }
    }
    return stream.str();
}

} // namespace editor
