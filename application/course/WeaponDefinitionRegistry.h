#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

#include "WeaponDefinitionAsset.h"

enum class WeaponDefinitionReloadStatus : uint8_t {
    NoChange,
    Reloaded,
    Failed,
};

struct WeaponDefinitionReloadReport {
    WeaponDefinitionReloadStatus status = WeaponDefinitionReloadStatus::NoChange;
    uint64_t previousRevision = 0;
    uint64_t currentRevision = 0;
    uint32_t loadedAssetCount = 0;
    uint32_t fallbackAssetCount = 0;
    std::string message;
};

struct WeaponDefinitionRegistryStats {
    uint64_t revision = 0;
    uint64_t successfulReloads = 0;
    uint64_t failedReloads = 0;
    uint32_t loadedAssetCount = 0;
    uint32_t fallbackAssetCount = 0;
    bool usingFallback = true;
    std::string status = "Fallback definitions active";
};

// Atomically loads every .weapon asset in a directory. A failed reload keeps
// the last-known-good registry and WeaponRuntimeState remains owned by the fire
// system when definitions are synchronized.
class WeaponDefinitionRegistry {
public:
    WeaponDefinitionRegistry();

    void Reset();
    bool LoadDirectory(
        const std::filesystem::path& directory,
        WeaponFireSystem* fireSystem = nullptr,
        std::string* errorMessage = nullptr);
    WeaponDefinitionReloadReport ReloadChangedAssets(
        WeaponFireSystem* fireSystem = nullptr);
    bool SynchronizeFireSystem(
        WeaponFireSystem& fireSystem,
        std::string* errorMessage = nullptr) const;

    const WeaponDefinitionAsset* Find(const std::string& weaponId) const;
    bool IsUsingFallback(const std::string& weaponId) const;
    const WeaponDefinitionRegistryStats& Stats() const { return stats_; }
    const std::filesystem::path& Directory() const { return directory_; }

    static std::filesystem::path DefaultDirectory();

private:
    using AssetMap = std::unordered_map<std::string, WeaponDefinitionAsset>;

    static AssetMap BuildFallbackAssets();
    bool StageDirectory(
        const std::filesystem::path& directory,
        AssetMap& stagedAssets,
        uint64_t& fingerprint,
        std::string& errorMessage) const;
    bool SynchronizeAssetSet(
        const AssetMap& loadedAssets,
        WeaponFireSystem& fireSystem,
        std::string* errorMessage) const;

    AssetMap fallbackAssets_;
    AssetMap loadedAssets_;
    std::filesystem::path directory_;
    uint64_t loadedFingerprint_ = 0;
    WeaponDefinitionRegistryStats stats_{};
};

const char* ToWeaponDefinitionReloadStatusString(WeaponDefinitionReloadStatus status);
