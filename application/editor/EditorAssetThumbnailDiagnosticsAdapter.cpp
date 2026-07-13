#include "EditorAssetThumbnailDiagnosticsAdapter.h"

#include "EditorAssetReferenceDiagnosticsAdapter.h"

#include <string>
#include <utility>

namespace editor {
namespace {

EditorObjectHandle MakeAssetThumbnailDiagnosticHandle(const EditorAssetRecord& record) {
    EditorObjectHandle handle{};
    handle.domain = EditorDomainId::Asset;
    handle.stableId = BuildEditorAssetDiagnosticStableId(record.kind, record.id);
    handle.displayName = std::string(ToString(record.kind)) + ":" + record.id;
    return handle;
}

} // namespace

EditorAssetThumbnailDiagnosticsAdapter::EditorAssetThumbnailDiagnosticsAdapter(
    const EditorAssetRegistry* assetRegistry,
    const EditorAssetThumbnailService* thumbnails)
    : assetRegistry_(assetRegistry)
    , thumbnails_(thumbnails) {
}

void EditorAssetThumbnailDiagnosticsAdapter::Validate(EditorValidationReport& report) const {
    if (assetRegistry_ == nullptr || thumbnails_ == nullptr) {
        return;
    }

    for (const EditorAssetRecord& record : assetRegistry_->Records()) {
        const EditorAssetThumbnailEntry thumbnail = thumbnails_->Resolve(record);
        if (thumbnail.status != EditorAssetThumbnailStatus::Failed) {
            continue;
        }

        EditorValidationIssue issue{};
        issue.severity = EditorValidationSeverity::Warning;
        issue.target = MakeAssetThumbnailDiagnosticHandle(record);
        issue.propertyPath = "thumbnail";
        issue.title = "Asset thumbnail failed";
        issue.message =
            std::string(ToString(record.kind)) +
            ":" +
            record.id +
            " cannot produce a thumbnail preview. " +
            thumbnail.detail;
        report.AddIssue(std::move(issue));
    }
}

} // namespace editor
