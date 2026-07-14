#include "EditorPrefab.h"

#include <unordered_set>

namespace editor {

EditorPrefabValidationReport EditorPrefabAsset::Validate() const {
    EditorPrefabValidationReport report{};
    if (schemaVersion == 0 || schemaVersion > kEditorPrefabSchemaVersion) {
        report.errors.push_back("Prefab schema version is unsupported.");
    }
    if (assetGuid.empty()) report.errors.push_back("Prefab Asset GUID is empty.");
    if (name.empty()) report.warnings.push_back("Prefab display name is empty.");
    if (rootEntityGuid.empty() || templateScene.FindEntity(rootEntityGuid) == nullptr) {
        report.errors.push_back("Prefab root Entity is missing.");
    }
    const EditorSceneValidationReport scene = templateScene.Validate();
    report.errors.insert(report.errors.end(), scene.errors.begin(), scene.errors.end());
    report.warnings.insert(report.warnings.end(), scene.warnings.begin(), scene.warnings.end());
    if (!templateScene.prefabInstances.empty()) {
        report.errors.push_back(
            "Prefab template Scene must express nesting through NESTED records, not Scene instances.");
    }
    std::unordered_set<std::string> mounts;
    for (const EditorPrefabNestedReference& nested : nestedPrefabs) {
        if (nested.mountEntityGuid.empty() ||
            templateScene.FindEntity(nested.mountEntityGuid) == nullptr) {
            report.errors.push_back("Nested Prefab mount Entity is missing: " + nested.mountEntityGuid);
        }
        if (!mounts.insert(nested.mountEntityGuid).second) {
            report.errors.push_back("Nested Prefab mount is duplicated: " + nested.mountEntityGuid);
        }
        if (nested.prefabAssetGuid.empty()) {
            report.errors.push_back("Nested Prefab Asset GUID is empty.");
        } else if (nested.prefabAssetGuid == assetGuid) {
            report.errors.push_back("Prefab cannot directly nest itself: " + assetGuid);
        }
    }
    return report;
}

} // namespace editor
