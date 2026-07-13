#pragma once

#include "EditorAssetRegistry.h"
#include "EditorAssetThumbnailService.h"
#include "EditorValidationService.h"

namespace editor {

class EditorAssetThumbnailDiagnosticsAdapter final : public EditorValidationAdapter {
public:
    EditorAssetThumbnailDiagnosticsAdapter(
        const EditorAssetRegistry* assetRegistry,
        const EditorAssetThumbnailService* thumbnails);

    void Validate(EditorValidationReport& report) const override;

private:
    const EditorAssetRegistry* assetRegistry_ = nullptr;
    const EditorAssetThumbnailService* thumbnails_ = nullptr;
};

} // namespace editor
