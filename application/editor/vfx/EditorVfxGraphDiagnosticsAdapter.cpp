#include "EditorVfxGraphDiagnosticsAdapter.h"

#include "EditorVfxGraph.h"
#include "../EditorAssetRegistry.h"
#include "../documents/EditorDocumentManager.h"
#include "../documents/EditorVfxGraphDocumentProvider.h"

namespace editor {
namespace {

void AddReferenceIssue(EditorValidationReport& report, const EditorVfxGraphAsset& asset,
    std::string_view nodeId, std::string_view field, std::string_view code,
    std::string_view kind, std::string_view guid) {
    EditorObjectHandle target;
    target.domain = EditorDomainId::VfxGraphNode;
    target.stableId = nodeId.empty() ? asset.assetGuid : std::string(nodeId);
    target.displayName = nodeId.empty() ? asset.name : "VFX Graph Node";
    report.AddIssue({EditorValidationSeverity::Error, std::move(target), std::string(field),
        std::string(code), "VFX Graph " + std::string(kind) +
            " GUID does not resolve to a compatible Asset: " + std::string(guid)});
}

} // namespace

void EditorVfxGraphDiagnosticsAdapter::Validate(EditorValidationReport& report) const {
    if (provider_ == nullptr || documents_ == nullptr) return;
    const EditorGraphSchema schema = BuildEditorVfxGraphSchema();
    for (const EditorDocumentRecord* document : documents_->OpenDocuments()) {
        if (document == nullptr || document->id.type != EditorDocumentTypes::VfxGraph) continue;
        const EditorVfxGraphAsset* asset = provider_->Asset(document->id);
        if (asset == nullptr) continue;
        const EditorVfxCompileArtifact artifact = CompileEditorVfxGraph(*asset, schema);
        for (const EditorVfxCompileDiagnostic& diagnostic : artifact.diagnostics) {
            EditorObjectHandle target;
            target.domain = EditorDomainId::VfxGraphNode;
            target.stableId = diagnostic.nodeId.empty() ? asset->assetGuid : diagnostic.nodeId;
            target.displayName = diagnostic.nodeId.empty() ? asset->name : "VFX Graph Node";
            report.AddIssue({diagnostic.severity == EditorGraphIssueSeverity::Error
                    ? EditorValidationSeverity::Error : EditorValidationSeverity::Warning,
                std::move(target), diagnostic.nodeId, diagnostic.code, diagnostic.message});
        }
        if (assets_ == nullptr) continue;
        for (const EditorVfxEmitterProgram& emitter : artifact.emitters) {
            if (!emitter.materialAssetGuid.empty()) {
                const EditorAssetRecord* record = assets_->FindByGuid(emitter.materialAssetGuid);
                if (record == nullptr || record->kind != EditorAssetKind::MaterialGraph || record->missing) {
                    AddReferenceIssue(report, *asset, emitter.nodeId, "materialAssetGuid",
                        "vfx.material_reference", "Material", emitter.materialAssetGuid);
                }
            }
            if (!emitter.textureAssetGuid.empty()) {
                const EditorAssetRecord* record = assets_->FindByGuid(emitter.textureAssetGuid);
                if (record == nullptr || record->kind != EditorAssetKind::Texture || record->missing) {
                    AddReferenceIssue(report, *asset, emitter.nodeId, "textureAssetGuid",
                        "vfx.texture_reference", "Texture", emitter.textureAssetGuid);
                }
            }
        }
    }
}

} // namespace editor
