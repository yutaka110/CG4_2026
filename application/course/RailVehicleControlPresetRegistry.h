#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

#include "RailVehicleControlDefinitionAsset.h"

enum class RailVehicleControlReloadStatus : uint8_t {
    NoChange,
    Reloaded,
    Failed,
};

struct RailVehicleControlReloadReport final {
    RailVehicleControlReloadStatus status =
        RailVehicleControlReloadStatus::NoChange;
    uint64_t previousRevision = 0;
    uint64_t currentRevision = 0;
    uint32_t loadedPresetCount = 0;
    uint32_t fallbackPresetCount = 0;
    std::string message;
};

struct RailVehicleControlRegistryStats final {
    uint64_t revision = 0;
    uint64_t successfulReloads = 0;
    uint64_t failedReloads = 0;
    uint32_t loadedPresetCount = 0;
    uint32_t fallbackPresetCount = 0;
    bool usingFallback = true;
    std::string status = "Fallback rail vehicle control presets active";
};

// Atomically stages every .railvehicle asset. Any malformed file rejects the
// complete reload and retains the last-known-good set used by gameplay.
class RailVehicleControlPresetRegistry final {
public:
    static constexpr const char* kMineCartStandardPresetId =
        "mine_cart_standard";

    RailVehicleControlPresetRegistry();

    void Reset();
    bool LoadDirectory(
        const std::filesystem::path& directory,
        std::string* errorMessage = nullptr);
    RailVehicleControlReloadReport ReloadChangedPresets();

    const RailVehicleControlDefinitionAsset* Find(
        const std::string& presetId) const;
    bool IsUsingFallback(const std::string& presetId) const;
    const RailVehicleControlRegistryStats& Stats() const noexcept {
        return stats_;
    }
    const std::filesystem::path& Directory() const noexcept {
        return directory_;
    }

    static std::filesystem::path DefaultDirectory();

private:
    using PresetMap = std::unordered_map<
        std::string,
        RailVehicleControlDefinitionAsset>;

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
    RailVehicleControlRegistryStats stats_{};
};

const char* ToRailVehicleControlReloadStatusString(
    RailVehicleControlReloadStatus status) noexcept;
