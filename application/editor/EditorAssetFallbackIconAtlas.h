#pragma once

#include "EditorAssetPreviewProvider.h"
#include "EditorAssetThumbnailTextureLoader.h"

namespace editor {

bool BuildEditorAssetFallbackIconPixels(
    EditorAssetKind kind,
    EditorAssetPreviewKind previewKind,
    uint32_t swatchRgba,
    EditorAssetThumbnailPixelData& outPixels);

} // namespace editor
