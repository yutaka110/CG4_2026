#pragma once

#include "../scene/EditorScene.h"

#include <cstdint>
#include <string>
#include <vector>

namespace editor {

inline constexpr uint32_t kEditorPrefabSchemaVersion = 2;
inline constexpr uint32_t kEditorPrefabMaxNestedDepth = 8;

struct EditorPrefabNestedReference {
    std::string mountEntityGuid;
    std::string prefabAssetGuid;
};

struct EditorPrefabValidationReport {
    std::vector<std::string> errors;
    std::vector<std::string> warnings;

    bool Succeeded() const noexcept { return errors.empty(); }
};

struct EditorPrefabAsset {
    uint32_t schemaVersion = kEditorPrefabSchemaVersion;
    uint64_t revision = 0;
    std::string assetGuid;
    std::string name;
    std::string rootEntityGuid;
    EditorScene templateScene;
    std::vector<EditorPrefabNestedReference> nestedPrefabs;

    EditorPrefabValidationReport Validate() const;
    void Touch() noexcept { ++revision; }
};

} // namespace editor
