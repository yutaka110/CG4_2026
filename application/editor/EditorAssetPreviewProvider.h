#pragma once

#include "EditorAssetRegistry.h"

#include <cstdint>
#include <string>

namespace editor {

enum class EditorAssetPreviewKind {
    Unknown,
    Icon,
    Texture,
    Mesh,
    Text,
    Audio,
};

enum class EditorAssetPreviewReadiness {
    Missing,
    Unsupported,
    Ready,
    Failed,
};

struct EditorAssetPreviewInfo {
    EditorAssetPreviewKind kind = EditorAssetPreviewKind::Icon;
    EditorAssetPreviewReadiness readiness = EditorAssetPreviewReadiness::Unsupported;
    std::string label;
    std::string detail;
    std::string format;
    uint64_t byteSize = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t vertexCount = 0;
    uint32_t faceCount = 0;
    uint32_t materialSlotCount = 0;
    uint32_t materialTextureCount = 0;
    uint32_t lineCount = 0;
    uint64_t materialTextureTimestamp = 0;
    float boundsMin[3] = {};
    float boundsMax[3] = {};
    float boundsRadius = 0.0f;
    float previewCameraDistance = 0.0f;
    float previewLightDirection[3] = {0.35f, -0.85f, 0.38f};
    bool hasPreviewGeometry = false;
    bool hasMaterialBinding = false;
    bool fallbackIcon = true;
};

class EditorAssetPreviewProvider {
public:
    EditorAssetPreviewInfo BuildPreview(const EditorAssetRecord& record) const;
};

const char* ToString(EditorAssetPreviewKind kind);
const char* ToString(EditorAssetPreviewReadiness readiness);

} // namespace editor
