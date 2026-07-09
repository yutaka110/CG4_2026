#include "EditorAssetPreviewSceneRenderer.h"

#include "EditorAssetMeshThumbnailPreviewRenderer.h"

#include <algorithm>
#include <string>

namespace editor {

bool RenderEditorAssetPreviewScenePass(
    const EditorAssetGpuThumbnailAllocationRequest& request,
    EditorAssetThumbnailPixelData& outPixels,
    std::string& outDetail,
    std::string& outError) {
    if (request.previewKind == EditorAssetPreviewKind::Mesh || request.kind == EditorAssetKind::Mesh) {
        if (!RenderEditorAssetMeshThumbnailPreview(request, outPixels, outError)) {
            return false;
        }
        outDetail = "Engine-scene mesh/material preview pass uploaded to GPU SRV";
        if (request.hasPreviewGeometry) {
            outDetail += " with real mesh bounds/proxy geometry";
        }
        if (request.hasMaterialBinding || request.materialSlotCount > 0) {
            outDetail += ", material slots " + std::to_string((std::max)(1u, request.materialSlotCount));
        }
        if (request.previewCameraDistance > 0.0f) {
            outDetail += ", camera " + std::to_string(request.previewCameraDistance);
        }
        outDetail += ".";
        return true;
    }

    outError = "Preview scene renderer does not support this asset kind yet.";
    return false;
}

} // namespace editor
