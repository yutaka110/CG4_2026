#include "EditorAssetMutationSafety.h"

#include <sstream>
#include <utility>

namespace editor {
namespace {

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
    return kind == EditorAssetMutationKind::Duplicate ||
        kind == EditorAssetMutationKind::Rename ||
        kind == EditorAssetMutationKind::Move;
}

void AddDependents(
    const EditorAssetRegistry& registry,
    const EditorAssetRecord& target,
    EditorAssetMutationSafetyReport& report) {
    for (const EditorAssetRecord* dependent : registry.FindDependents(target)) {
        if (dependent != nullptr) {
            report.dependents.push_back(AssetLabel(*dependent));
        }
    }
    report.dependentCount = report.dependents.size();
}

} // namespace

const char* ToString(EditorAssetMutationKind kind) {
    switch (kind) {
    case EditorAssetMutationKind::Duplicate:
        return "Duplicate";
    case EditorAssetMutationKind::Rename:
        return "Rename";
    case EditorAssetMutationKind::Move:
        return "Move";
    case EditorAssetMutationKind::Delete:
        return "Delete";
    case EditorAssetMutationKind::RepairReferences:
        return "Repair References";
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
    if (RequiresDurableGuid(kind) &&
        (!target.hasMetadata || target.provisionalGuid || !IsDurableEditorAssetGuid(target.guid))) {
        AddIssue(
            report,
            EditorAssetMutationRisk::Blocked,
            "Duplicate/Rename/Move requires durable .meta GUID metadata. Run Create Asset .meta first.");
    }
    if (RequiresDurableGuid(kind) && registry.FindAllByGuid(target.guid).size() > 1) {
        AddIssue(
            report,
            EditorAssetMutationRisk::Blocked,
            "Duplicate/Rename/Move is blocked because the durable GUID is duplicated.");
    }
    if (kind == EditorAssetMutationKind::Delete && report.dependentCount > 0) {
        AddIssue(
            report,
            EditorAssetMutationRisk::Blocked,
            "Delete is blocked because other indexed assets depend on this asset.");
    } else if ((kind == EditorAssetMutationKind::Rename ||
                   kind == EditorAssetMutationKind::Move) &&
        report.dependentCount > 0) {
        AddIssue(
            report,
            EditorAssetMutationRisk::Warning,
            "Rename/Move will rewrite indexed dependent references.");
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
    if (kind == EditorAssetMutationKind::RepairReferences &&
        (!target.hasMetadata || target.provisionalGuid || target.metadataPath.empty())) {
        AddIssue(
            report,
            EditorAssetMutationRisk::Blocked,
            "Reference repair requires durable owner .meta metadata.");
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
