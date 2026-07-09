#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace editor {

struct EditorAssetThumbnailPixelData {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t rowPitch = 0;
    std::vector<uint8_t> rgba8;
};

bool LoadEditorAssetTextureThumbnailPixels(
    const std::string& sourcePath,
    uint32_t maxExtent,
    EditorAssetThumbnailPixelData& outPixels,
    std::string& outError);

} // namespace editor
