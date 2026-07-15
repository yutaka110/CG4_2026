#include "EditorGameplayVisualScriptDiagnosticsAdapter.h"
#include "EditorGameplayVisualScript.h"
#include "../documents/EditorDocumentManager.h"
#include "../documents/EditorGameplayVisualScriptDocumentProvider.h"
namespace editor {
void EditorGameplayVisualScriptDiagnosticsAdapter::Validate(EditorValidationReport& report) const {
    if(provider_==nullptr||documents_==nullptr)return;
    const auto schema=BuildEditorGameplayVisualScriptSchema();
    for(const EditorDocumentRecord* document:documents_->OpenDocuments()){
        if(document==nullptr||document->id.type!=EditorDocumentTypes::GameplayVisualScript)continue;
        const auto* asset=provider_->Asset(document->id);if(asset==nullptr)continue;
        const auto artifact=CompileEditorGameplayVisualScript(*asset,schema);
        for(const auto& diagnostic:artifact.diagnostics){
            EditorObjectHandle target;target.domain=EditorDomainId::GameplayVisualScriptNode;
            target.stableId=diagnostic.nodeId.empty()?asset->assetGuid:diagnostic.nodeId;
            target.displayName=diagnostic.nodeId.empty()?asset->name:"Gameplay Visual Script Node";
            report.AddIssue({diagnostic.severity==EditorGraphIssueSeverity::Error?EditorValidationSeverity::Error:EditorValidationSeverity::Warning,
                std::move(target),diagnostic.nodeId,diagnostic.code,diagnostic.message});
        }
    }
}
} // namespace editor
