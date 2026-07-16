#include "EditorNavigationAuthoringTypes.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <unordered_set>

namespace editor {
namespace {
constexpr uint64_t kFnvOffset = 1469598103934665603ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;

void SetError(std::string* output, std::string value) {
    if (output != nullptr) *output = std::move(value);
}

uint64_t HashBytes(uint64_t hash, const void* data, std::size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t index = 0; index < size; ++index) {
        hash ^= bytes[index];
        hash *= kFnvPrime;
    }
    return hash;
}

template <typename T>
uint64_t HashValue(uint64_t hash, const T& value) {
    return HashBytes(hash, &value, sizeof(value));
}

uint64_t HashText(uint64_t hash, std::string_view value) {
    return HashBytes(hash, value.data(), value.size());
}

bool Finite(Vector3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

float DistanceSquared(Vector3 a, Vector3 b) {
    const float x = a.x - b.x;
    const float y = a.y - b.y;
    const float z = a.z - b.z;
    return x * x + y * y + z * z;
}
} // namespace

EditorNavigationAuthoringAsset MakeDefaultEditorNavigationAuthoringAsset(
    std::string assetGuid, std::string name) {
    EditorNavigationAuthoringAsset asset;
    asset.assetGuid = std::move(assetGuid);
    asset.name = std::move(name);
    asset.areas.push_back({"Default", 1.0f, {0.25f, 0.75f, 1.0f}, true});
    asset.agentProfiles.push_back({"Default", 0.5f, 1.8f, 0.75f, 45.0f});
    return asset;
}

EditorNavigationAuthoringCompileResult CompileEditorNavigationAuthoring(
    const EditorNavigationAuthoringAsset& asset) {
    EditorNavigationAuthoringCompileResult result;
    const auto issue = [&](std::string code, std::string id, std::string message) {
        result.diagnostics.push_back({std::move(code), std::move(id), std::move(message)});
    };
    if (asset.schemaVersion != kEditorNavigationAuthoringSchemaVersion)
        issue("navigation.schema", {}, "Navigation Data schema version is unsupported.");
    if (asset.assetGuid.empty() || asset.name.empty())
        issue("navigation.identity", {}, "Navigation Data identity and name are required.");
    if (asset.areas.empty() || asset.areas.size() > kEditorNavigationMaximumAreas)
        issue("navigation.area.capacity", {}, "Navigation areas are empty or exceed capacity.");
    if (asset.agentProfiles.empty() ||
        asset.agentProfiles.size() > kEditorNavigationMaximumAgentProfiles)
        issue("navigation.profile.capacity", {}, "Agent Profiles are empty or exceed capacity.");
    if (asset.offMeshLinks.size() > kEditorNavigationMaximumOffMeshLinks)
        issue("navigation.link.capacity", {}, "Off-Mesh Links exceed capacity.");

    std::unordered_set<std::string> areas;
    bool defaultArea = false;
    for (const auto& area : asset.areas) {
        if (area.id.empty() || !areas.insert(area.id).second)
            issue("navigation.area.identity", area.id, "Area ID is empty or duplicated.");
        if (area.id == "Default") defaultArea = true;
        if (!std::isfinite(area.cost) || area.cost < 0.01f || area.cost > 1000.0f ||
            !Finite(area.debugColor))
            issue("navigation.area.value", area.id, "Area cost or debug color is invalid.");
    }
    if (!defaultArea) issue("navigation.area.default", {}, "Default area is required.");

    std::unordered_set<std::string> profiles;
    bool defaultProfile = false;
    for (const auto& profile : asset.agentProfiles) {
        if (profile.id.empty() || !profiles.insert(profile.id).second)
            issue("navigation.profile.identity", profile.id, "Agent Profile ID is empty or duplicated.");
        if (profile.id == "Default") defaultProfile = true;
        if (!std::isfinite(profile.radius) || !std::isfinite(profile.height) ||
            !std::isfinite(profile.maximumStepHeight) || !std::isfinite(profile.maximumSlopeDegrees) ||
            profile.radius < 0.0f || profile.height <= 0.0f || profile.maximumStepHeight < 0.0f ||
            profile.maximumSlopeDegrees < 1.0f || profile.maximumSlopeDegrees > 89.0f)
            issue("navigation.profile.value", profile.id, "Agent Profile dimensions or slope are invalid.");
    }
    if (!defaultProfile) issue("navigation.profile.default", {}, "Default Agent Profile is required.");

    std::unordered_set<std::string> links;
    for (const auto& link : asset.offMeshLinks) {
        if (link.id.empty() || !links.insert(link.id).second)
            issue("navigation.link.identity", link.id, "Off-Mesh Link ID is empty or duplicated.");
        if (!Finite(link.start) || !Finite(link.end) || !std::isfinite(link.radius) ||
            !std::isfinite(link.costMultiplier) || link.radius <= 0.0f ||
            link.costMultiplier < 0.01f || DistanceSquared(link.start, link.end) <= 1.0e-6f)
            issue("navigation.link.value", link.id, "Off-Mesh Link endpoints, radius, or cost are invalid.");
        if (!areas.contains(link.areaId))
            issue("navigation.link.area", link.id, "Off-Mesh Link references a missing Area.");
        if (!profiles.contains(link.agentProfileId))
            issue("navigation.link.profile", link.id, "Off-Mesh Link references a missing Agent Profile.");
    }

    if (!result.diagnostics.empty()) return result;
    result.program.areas = asset.areas;
    result.program.agentProfiles = asset.agentProfiles;
    result.program.offMeshLinks = asset.offMeshLinks;
    std::sort(result.program.areas.begin(), result.program.areas.end(),
        [](const auto& a, const auto& b) { return a.id < b.id; });
    std::sort(result.program.agentProfiles.begin(), result.program.agentProfiles.end(),
        [](const auto& a, const auto& b) { return a.id < b.id; });
    std::sort(result.program.offMeshLinks.begin(), result.program.offMeshLinks.end(),
        [](const auto& a, const auto& b) { return a.id < b.id; });
    uint64_t hash = HashText(kFnvOffset, asset.assetGuid);
    for (const auto& area : result.program.areas) {
        hash = HashText(hash, area.id); hash = HashValue(hash, area.cost);
        hash = HashValue(hash, area.debugColor); hash = HashValue(hash, area.enabled);
    }
    for (const auto& profile : result.program.agentProfiles) {
        hash = HashText(hash, profile.id); hash = HashValue(hash, profile.radius);
        hash = HashValue(hash, profile.height); hash = HashValue(hash, profile.maximumStepHeight);
        hash = HashValue(hash, profile.maximumSlopeDegrees);
    }
    for (const auto& link : result.program.offMeshLinks) {
        hash = HashText(hash, link.id); hash = HashValue(hash, link.start);
        hash = HashValue(hash, link.end); hash = HashValue(hash, link.radius);
        hash = HashValue(hash, link.costMultiplier); hash = HashValue(hash, link.bidirectional);
        hash = HashValue(hash, link.enabled); hash = HashText(hash, link.areaId);
        hash = HashText(hash, link.agentProfileId);
    }
    result.program.sourceFingerprint = hash;
    result.succeeded = true;
    return result;
}

bool EncodeEditorNavigationAuthoring(const EditorNavigationAuthoringAsset& asset,
    std::string& output, std::string* errorMessage) {
    const EditorNavigationAuthoringCompileResult compiled = CompileEditorNavigationAuthoring(asset);
    if (!compiled.succeeded) {
        SetError(errorMessage, compiled.diagnostics.empty() ? "Navigation Data compile failed."
            : compiled.diagnostics.front().message);
        return false;
    }
    std::ostringstream stream;
    stream << std::setprecision(9)
           << "NAVIGATION_DATA " << kEditorNavigationAuthoringSchemaVersion << '\n'
           << "asset " << std::quoted(asset.assetGuid) << ' ' << std::quoted(asset.name) << '\n'
           << "areas " << asset.areas.size() << '\n';
    for (const auto& area : asset.areas)
        stream << "area " << std::quoted(area.id) << ' ' << area.cost << ' '
               << area.debugColor.x << ' ' << area.debugColor.y << ' ' << area.debugColor.z
               << ' ' << area.enabled << '\n';
    stream << "profiles " << asset.agentProfiles.size() << '\n';
    for (const auto& profile : asset.agentProfiles)
        stream << "profile " << std::quoted(profile.id) << ' ' << profile.radius << ' '
               << profile.height << ' ' << profile.maximumStepHeight << ' '
               << profile.maximumSlopeDegrees << '\n';
    stream << "links " << asset.offMeshLinks.size() << '\n';
    for (const auto& link : asset.offMeshLinks)
        stream << "link " << std::quoted(link.id) << ' '
               << link.start.x << ' ' << link.start.y << ' ' << link.start.z << ' '
               << link.end.x << ' ' << link.end.y << ' ' << link.end.z << ' '
               << link.radius << ' ' << link.costMultiplier << ' ' << link.bidirectional << ' '
               << link.enabled << ' ' << std::quoted(link.areaId) << ' '
               << std::quoted(link.agentProfileId) << '\n';
    stream << "END\n";
    output = stream.str();
    SetError(errorMessage, {});
    return true;
}

bool DecodeEditorNavigationAuthoring(std::string_view input,
    EditorNavigationAuthoringAsset& asset, std::string* errorMessage) {
    std::istringstream stream{std::string(input)};
    std::string token;
    uint32_t version = 0;
    EditorNavigationAuthoringAsset decoded;
    if (!(stream >> token >> version) || token != "NAVIGATION_DATA" ||
        version != kEditorNavigationAuthoringSchemaVersion ||
        !(stream >> token) || token != "asset" ||
        !(stream >> std::quoted(decoded.assetGuid) >> std::quoted(decoded.name))) {
        SetError(errorMessage, "Navigation Data header is invalid.");
        return false;
    }
    decoded.schemaVersion = version;
    std::size_t count = 0;
    if (!(stream >> token >> count) || token != "areas" || count > kEditorNavigationMaximumAreas) {
        SetError(errorMessage, "Navigation Data area section is invalid."); return false;
    }
    for (std::size_t index = 0; index < count; ++index) {
        EditorNavigationAreaDefinition value;
        if (!(stream >> token) || token != "area" ||
            !(stream >> std::quoted(value.id) >> value.cost >> value.debugColor.x >>
                value.debugColor.y >> value.debugColor.z >> value.enabled)) {
            SetError(errorMessage, "Navigation Data area entry is invalid."); return false;
        }
        decoded.areas.push_back(std::move(value));
    }
    if (!(stream >> token >> count) || token != "profiles" ||
        count > kEditorNavigationMaximumAgentProfiles) {
        SetError(errorMessage, "Navigation Data profile section is invalid."); return false;
    }
    for (std::size_t index = 0; index < count; ++index) {
        EditorNavigationAgentProfile value;
        if (!(stream >> token) || token != "profile" ||
            !(stream >> std::quoted(value.id) >> value.radius >> value.height >>
                value.maximumStepHeight >> value.maximumSlopeDegrees)) {
            SetError(errorMessage, "Navigation Data profile entry is invalid."); return false;
        }
        decoded.agentProfiles.push_back(std::move(value));
    }
    if (!(stream >> token >> count) || token != "links" ||
        count > kEditorNavigationMaximumOffMeshLinks) {
        SetError(errorMessage, "Navigation Data Off-Mesh Link section is invalid."); return false;
    }
    for (std::size_t index = 0; index < count; ++index) {
        EditorNavigationOffMeshLink value;
        if (!(stream >> token) || token != "link" ||
            !(stream >> std::quoted(value.id) >> value.start.x >> value.start.y >> value.start.z >>
                value.end.x >> value.end.y >> value.end.z >> value.radius >> value.costMultiplier >>
                value.bidirectional >> value.enabled >> std::quoted(value.areaId) >>
                std::quoted(value.agentProfileId))) {
            SetError(errorMessage, "Navigation Data Off-Mesh Link entry is invalid."); return false;
        }
        decoded.offMeshLinks.push_back(std::move(value));
    }
    if (!(stream >> token) || token != "END") {
        SetError(errorMessage, "Navigation Data terminator is missing."); return false;
    }
    const EditorNavigationAuthoringCompileResult compiled = CompileEditorNavigationAuthoring(decoded);
    if (!compiled.succeeded) {
        SetError(errorMessage, compiled.diagnostics.empty() ? "Navigation Data compile failed."
            : compiled.diagnostics.front().message);
        return false;
    }
    asset = std::move(decoded);
    SetError(errorMessage, {});
    return true;
}

} // namespace editor
