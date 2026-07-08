#include "EffectAssetDiagnosticsAdapter.h"

#include "../EffectAssetLoader.h"

#include <string>
#include <utility>

namespace editor {
namespace {

EditorValidationSeverity ToEditorSeverity(EffectAssetDiagnosticSeverity severity) {
    switch (severity) {
    case EffectAssetDiagnosticSeverity::Info:
        return EditorValidationSeverity::Info;
    case EffectAssetDiagnosticSeverity::Warning:
        return EditorValidationSeverity::Warning;
    case EffectAssetDiagnosticSeverity::Error:
        return EditorValidationSeverity::Error;
    }
    return EditorValidationSeverity::Warning;
}

std::string NormalizePathText(const std::filesystem::path& path) {
    return path.generic_string();
}

EditorObjectHandle MakeEffectAssetHandle(const LoadedEffectAsset& loaded, uint64_t index) {
    EditorObjectHandle handle{};
    handle.domain = EditorDomainId::VfxEffectAsset;
    handle.stableId = NormalizePathText(loaded.path);
    handle.localIndex = index;
    handle.displayName = loaded.path.empty()
        ? std::string("Effect Asset #") + std::to_string(index)
        : loaded.path.filename().string();
    return handle;
}

std::string BuildPropertyPath(const EffectAssetDiagnostic& diagnostic) {
    if (!diagnostic.scope.empty() && !diagnostic.field.empty()) {
        return diagnostic.scope + "." + diagnostic.field;
    }
    if (!diagnostic.field.empty()) {
        return diagnostic.field;
    }
    if (!diagnostic.scope.empty()) {
        return diagnostic.scope;
    }
    return {};
}

std::string BuildTitle(const EffectAssetDiagnostic& diagnostic) {
    if (!diagnostic.code.empty()) {
        return diagnostic.code;
    }
    return "Effect asset diagnostic";
}

std::string BuildMessage(const EffectAssetDiagnostic& diagnostic) {
    std::string message = diagnostic.message;
    if (!diagnostic.source.empty()) {
        if (!message.empty()) {
            message += " ";
        }
        message += "(" + diagnostic.source;
        if (diagnostic.lineNumber != 0) {
            message += ":" + std::to_string(diagnostic.lineNumber);
        }
        message += ")";
    }
    return message.empty() ? std::string("Effect asset diagnostic has no message.") : message;
}

} // namespace

EffectAssetDiagnosticsAdapter::EffectAssetDiagnosticsAdapter(
    const std::vector<LoadedEffectAsset>* loadedEffectAssets)
    : loadedEffectAssets_(loadedEffectAssets) {
}

void EffectAssetDiagnosticsAdapter::Validate(EditorValidationReport& report) const {
    if (loadedEffectAssets_ == nullptr) {
        return;
    }

    for (std::size_t index = 0; index < loadedEffectAssets_->size(); ++index) {
        const LoadedEffectAsset& loaded = (*loadedEffectAssets_)[index];
        const EditorObjectHandle target = MakeEffectAssetHandle(loaded, static_cast<uint64_t>(index));
        for (const EffectAssetDiagnostic& diagnostic : loaded.diagnostics) {
            EditorValidationIssue issue{};
            issue.severity = ToEditorSeverity(diagnostic.severity);
            issue.target = target;
            issue.propertyPath = BuildPropertyPath(diagnostic);
            issue.title = BuildTitle(diagnostic);
            issue.message = BuildMessage(diagnostic);
            report.AddIssue(std::move(issue));
        }
    }
}

} // namespace editor
