#include "EditorAnimationStateMachineDiagnosticsAdapter.h"

#include "EditorAnimationStateMachine.h"
#include "../EditorAssetRegistry.h"
#include "../documents/EditorAnimationStateMachineDocumentProvider.h"
#include "../documents/EditorDocumentManager.h"

namespace editor {

void EditorAnimationStateMachineDiagnosticsAdapter::Validate(
    EditorValidationReport& report) const {
    if (provider_ == nullptr || documents_ == nullptr) return;
    const EditorGraphSchema schema = BuildEditorAnimationStateMachineSchema();
    for (const EditorDocumentRecord* document : documents_->OpenDocuments()) {
        if (document == nullptr || document->id.type != EditorDocumentTypes::AnimationStateMachine) continue;
        const auto* asset = provider_->Asset(document->id);
        if (asset == nullptr) continue;
        const auto artifact = CompileEditorAnimationStateMachine(*asset, schema);
        for (const auto& diagnostic : artifact.diagnostics) {
            EditorObjectHandle target;
            target.domain = EditorDomainId::AnimationStateMachineNode;
            target.stableId = diagnostic.nodeId.empty() ? asset->assetGuid : diagnostic.nodeId;
            target.displayName = diagnostic.nodeId.empty() ? asset->name : "Animation State Machine Node";
            report.AddIssue({diagnostic.severity == EditorGraphIssueSeverity::Error
                    ? EditorValidationSeverity::Error : EditorValidationSeverity::Warning,
                std::move(target), diagnostic.nodeId, diagnostic.code, diagnostic.message});
        }
        if (assets_ == nullptr) continue;
        for (const std::string& guid : artifact.animationSourceAssetGuids) {
            const EditorAssetRecord* source = assets_->FindByGuid(guid);
            if (source != nullptr && source->kind == EditorAssetKind::Mesh && !source->missing) continue;
            EditorObjectHandle target;
            target.domain = EditorDomainId::AnimationStateMachineNode;
            target.stableId = asset->assetGuid;
            target.displayName = asset->name;
            report.AddIssue({EditorValidationSeverity::Error, std::move(target),
                "sourceAssetGuid", "animation.source_reference",
                "Animation source GUID does not resolve to a skinned Mesh Asset: " + guid});
        }
    }
}

} // namespace editor
