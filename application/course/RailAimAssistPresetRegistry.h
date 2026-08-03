#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

#include "RailAimAssistSystem.h"

inline constexpr uint32_t kRailAimAssistPresetSchemaVersion = 1;

struct RailAimAssistPreset {
    uint32_t schemaVersion = kRailAimAssistPresetSchemaVersion;
    std::string presetId;
    std::string displayName;
    RailAimAssistSettings settings{};

    bool LoadFromFile(
        const std::filesystem::path& path,
        std::string* errorMessage = nullptr);
    bool Validate(std::string* errorMessage = nullptr) const;
};

enum class RailAimAssistPresetReloadStatus : uint8_t {
    NoChange,
    Reloaded,
    Failed,
};

struct RailAimAssistPresetReloadReport {
    RailAimAssistPresetReloadStatus status =
        RailAimAssistPresetReloadStatus::NoChange;
    uint64_t previousRevision = 0;
    uint64_t currentRevision = 0;
    uint32_t loadedPresetCount = 0;
    uint32_t fallbackPresetCount = 0;
    std::string message;
};

struct RailAimAssistPresetRegistryStats {
    uint64_t revision = 0;
    uint64_t successfulReloads = 0;
    uint64_t failedReloads = 0;
    uint32_t loadedPresetCount = 0;
    uint32_t fallbackPresetCount = 0;
    bool usingFallback = true;
    std::string status = "Fallback aim-assist presets active";
};

// Data-driven, atomically hot-reloaded aim-assist tuning. Invalid edits never
// replace the last-known-good preset set used by gameplay.
class RailAimAssistPresetRegistry {
public:
    RailAimAssistPresetRegistry();

    void Reset();
    bool LoadDirectory(
        const std::filesystem::path& directory,
        std::string* errorMessage = nullptr);
    RailAimAssistPresetReloadReport ReloadChangedPresets();

    const RailAimAssistPreset* Find(const std::string& presetId) const;
    bool IsUsingFallback(const std::string& presetId) const;
    const RailAimAssistPresetRegistryStats& Stats() const { return stats_; }
    const std::filesystem::path& Directory() const { return directory_; }

    static std::filesystem::path DefaultDirectory();

private:
    using PresetMap = std::unordered_map<std::string, RailAimAssistPreset>;

    static PresetMap BuildFallbackPresets();
    bool StageDirectory(
        const std::filesystem::path& directory,
        PresetMap& stagedPresets,
        uint64_t& fingerprint,
        std::string& errorMessage) const;

    PresetMap fallbackPresets_;
    PresetMap loadedPresets_;
    std::filesystem::path directory_;
    uint64_t loadedFingerprint_ = 0;
    RailAimAssistPresetRegistryStats stats_{};
};

const char* ToRailAimAssistPresetReloadStatusString(
    RailAimAssistPresetReloadStatus status);
