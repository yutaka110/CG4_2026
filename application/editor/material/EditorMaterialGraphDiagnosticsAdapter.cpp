#include "EditorMaterialGraphDiagnosticsAdapter.h"

#include "EditorMaterialGraph.h"
#include "../EditorAssetRegistry.h"
#include "../documents/EditorDocumentManager.h"
#include "../documents/EditorMaterialGraphDocumentProvider.h"

namespace editor {

void EditorMaterialGraphDiagnosticsAdapter::Validate(EditorValidationReport& report) const {
    if (provider_ == nullptr || documents_ == nullptr) return;
    const EditorGraphSchema schema = BuildEditorMaterialGraphSchema();
    for (const EditorDocumentRecord* document : documents_->OpenDocuments()) {
        if (document == nullptr || document->id.type != EditorDocumentTypes::MaterialGraph) continue;
        const EditorMaterialGraphAsset* asset = provider_->Asset(document->id);
        if (asset == nullptr) continue;
        const EditorMaterialCompileArtifact artifact = CompileEditorMaterialGraph(*asset, schema);
        for (const EditorMaterialCompileDiagnostic& diagnostic : artifact.diagnostics) {
            EditorObjectHandle target;
            target.domain = EditorDomainId::MaterialGraphNode;
            target.stableId = diagnostic.nodeId.empty() ? asset->assetGuid : diagnostic.nodeId;
            target.displayName = diagnostic.nodeId.empty() ? asset->name : "Material Graph Node";
            report.AddIssue({
                diagnostic.severity == EditorGraphIssueSeverity::Error
                    ? EditorValidationSeverity::Error
                    : EditorValidationSeverity::Warning,
                std::move(target),
                diagnostic.nodeId,
                diagnostic.code,
                diagnostic.message});
        }
        if (assets_ == nullptr) continue;
        for (const std::string& textureGuid : artifact.textureAssetGuids) {
            const EditorAssetRecord* texture = assets_->FindByGuid(textureGuid);
            if (texture != nullptr && texture->kind == EditorAssetKind::Texture && !texture->missing) continue;
            EditorObjectHandle target;
            target.domain = EditorDomainId::MaterialGraphNode;
            target.stableId = asset->assetGuid;
            target.displayName = asset->name;
            report.AddIssue({
                EditorValidationSeverity::Error,
                std::move(target),
                "textureAssetGuid",
                "material.texture_reference",
                "Material Graph texture GUID is missing or does not resolve to a Texture asset: " + textureGuid});
        }
    }
}

} // namespace editor
