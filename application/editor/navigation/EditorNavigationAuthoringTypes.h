#pragma once

#include "utils/math/Vector.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

inline constexpr uint32_t kEditorNavigationAuthoringSchemaVersion = 1;
inline constexpr uint32_t kEditorNavigationMaximumAreas = 64;
inline constexpr uint32_t kEditorNavigationMaximumAgentProfiles = 16;
inline constexpr uint32_t kEditorNavigationMaximumOffMeshLinks = 256;

struct EditorNavigationAreaDefinition {
    std::string id;
    float cost = 1.0f;
    Vector3 debugColor{0.25f, 0.75f, 1.0f};
    bool enabled = true;
};

struct EditorNavigationAgentProfile {
    std::string id;
    float radius = 0.5f;
    float height = 1.8f;
    float maximumStepHeight = 0.75f;
    float maximumSlopeDegrees = 45.0f;
};

struct EditorNavigationOffMeshLink {
    std::string id;
    Vector3 start{};
    Vector3 end{};
    float radius = 1.0f;
    float costMultiplier = 1.0f;
    bool bidirectional = true;
    bool enabled = true;
    std::string areaId = "Default";
    std::string agentProfileId = "Default";
};

struct EditorNavigationAuthoringAsset {
    uint32_t schemaVersion = kEditorNavigationAuthoringSchemaVersion;
    std::string assetGuid;
    std::string name;
    std::vector<EditorNavigationAreaDefinition> areas;
    std::vector<EditorNavigationAgentProfile> agentProfiles;
    std::vector<EditorNavigationOffMeshLink> offMeshLinks;
};

struct EditorNavigationAuthoringDiagnostic {
    std::string code;
    std::string objectId;
    std::string message;
};

struct EditorNavigationAuthoringProgram {
    uint64_t sourceFingerprint = 0;
    std::vector<EditorNavigationAreaDefinition> areas;
    std::vector<EditorNavigationAgentProfile> agentProfiles;
    std::vector<EditorNavigationOffMeshLink> offMeshLinks;
};

struct EditorNavigationAuthoringCompileResult {
    bool succeeded = false;
    EditorNavigationAuthoringProgram program{};
    std::vector<EditorNavigationAuthoringDiagnostic> diagnostics;
};

EditorNavigationAuthoringAsset MakeDefaultEditorNavigationAuthoringAsset(
    std::string assetGuid, std::string name);
EditorNavigationAuthoringCompileResult CompileEditorNavigationAuthoring(
    const EditorNavigationAuthoringAsset& asset);
bool EncodeEditorNavigationAuthoring(const EditorNavigationAuthoringAsset& asset,
    std::string& output, std::string* errorMessage = nullptr);
bool DecodeEditorNavigationAuthoring(std::string_view input,
    EditorNavigationAuthoringAsset& asset, std::string* errorMessage = nullptr);

} // namespace editor
