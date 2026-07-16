#pragma once

#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace editor {

using EditorDocumentTypeId = std::string;

namespace EditorDocumentTypes {
inline constexpr std::string_view Course = "course";
inline constexpr std::string_view Scene = "scene";
inline constexpr std::string_view Prefab = "prefab";
inline constexpr std::string_view MaterialGraph = "material-graph";
inline constexpr std::string_view VfxGraph = "vfx-graph";
inline constexpr std::string_view AnimationStateMachine = "animation-state-machine";
inline constexpr std::string_view GameplayVisualScript = "gameplay-visual-script";
inline constexpr std::string_view BehaviorTree = "behavior-tree";
inline constexpr std::string_view EnvironmentQuery = "environment-query";
inline constexpr std::string_view NavigationData = "navigation-data";
inline constexpr std::string_view Effect = "effect";
inline constexpr std::string_view RenderPreset = "render-preset";
inline constexpr std::string_view ProjectSettings = "project-settings";
} // namespace EditorDocumentTypes

struct EditorDocumentId {
    std::string assetGuid;
    EditorDocumentTypeId type;

    bool IsValid() const noexcept { return !assetGuid.empty() && !type.empty(); }
    std::string Key() const { return type + ":" + assetGuid; }

    friend bool operator==(const EditorDocumentId& lhs, const EditorDocumentId& rhs) noexcept {
        return lhs.assetGuid == rhs.assetGuid && lhs.type == rhs.type;
    }
    friend bool operator!=(const EditorDocumentId& lhs, const EditorDocumentId& rhs) noexcept {
        return !(lhs == rhs);
    }
};

inline uint64_t EditorDocumentHash64(std::string_view value, uint64_t seed) noexcept {
    uint64_t hash = seed;
    for (const unsigned char byte : value) {
        hash ^= static_cast<uint64_t>(byte);
        hash *= 1099511628211ull;
    }
    return hash;
}

inline std::string MakeEditorDocumentGuid(
    std::string_view type,
    const std::filesystem::path& path) {
    std::string normalized = path.lexically_normal().generic_string();
    for (char& value : normalized) {
        if (value >= 'A' && value <= 'Z') value = static_cast<char>(value - 'A' + 'a');
    }
    const std::string identity = std::string(type) + "|" + normalized;
    const uint64_t high = EditorDocumentHash64(identity, 1469598103934665603ull);
    const uint64_t low = EditorDocumentHash64(identity, 1099511628211ull);
    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(16) << high << std::setw(16) << low;
    return stream.str();
}

} // namespace editor
