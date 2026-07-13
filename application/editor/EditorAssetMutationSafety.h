#pragma once

#include "EditorAssetRegistry.h"

#include <string>
#include <vector>

namespace editor {

enum class EditorAssetMutationKind {
    Rename,
    Move,
    Delete,
};

enum class EditorAssetMutationRisk {
    Allowed,
    Warning,
    Blocked,
};

struct EditorAssetMutationSafetyIssue {
    EditorAssetMutationRisk risk = EditorAssetMutationRisk::Allowed;
    std::string message;
};

struct EditorAssetMutationSafetyReport {
    EditorAssetMutationKind kind = EditorAssetMutationKind::Rename;
    EditorAssetMutationRisk risk = EditorAssetMutationRisk::Allowed;
    EditorAssetRecord target;
    std::size_t dependentCount = 0;
    std::vector<std::string> dependents;
    std::vector<EditorAssetMutationSafetyIssue> issues;

    bool Blocked() const { return risk == EditorAssetMutationRisk::Blocked; }
    bool HasWarnings() const { return risk == EditorAssetMutationRisk::Warning; }
};

const char* ToString(EditorAssetMutationKind kind);
const char* ToString(EditorAssetMutationRisk risk);

EditorAssetMutationSafetyReport EvaluateEditorAssetMutationSafety(
    const EditorAssetRegistry& registry,
    const EditorAssetRecord& target,
    EditorAssetMutationKind kind);

std::string FormatEditorAssetMutationSafetyReport(
    const EditorAssetMutationSafetyReport& report);

} // namespace editor
