#pragma once

#include "EditorAssetGpuThumbnailRenderer.h"
#include "EditorAssetThumbnailTextureLoader.h"

namespace editor {

bool RenderEditorAssetPreviewScenePass(
    const EditorAssetGpuThumbnailAllocationRequest& request,
    EditorAssetThumbnailPixelData& outPixels,
    std::string& outDetail,
    std::string& outError);

} // namespace editor
