#pragma once

#include "EditorAssetGpuThumbnailRenderer.h"
#include "EditorAssetThumbnailTextureLoader.h"

namespace editor {

bool RenderEditorAssetMeshThumbnailPreview(
    const EditorAssetGpuThumbnailAllocationRequest& request,
    EditorAssetThumbnailPixelData& outPixels,
    std::string& outError);

} // namespace editor
