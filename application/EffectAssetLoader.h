#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "EffectAuthoringRegistry.h"
#include "EffectSystem.h"

enum class EffectAssetDiagnosticSeverity {
    Info,
    Warning,
    Error,
};

struct EffectAssetDiagnostic {
    EffectAssetDiagnosticSeverity severity = EffectAssetDiagnosticSeverity::Warning;
    std::string code;
    std::string scope;
    std::string field;
    std::string source;
    uint32_t lineNumber = 0;
    std::string message;
};

struct LoadedEffectAsset {
    EffectAsset asset;
    std::filesystem::file_time_type lastWriteTime{};
    std::filesystem::path path;
    std::vector<EffectAssetDiagnostic> diagnostics;
};

class EffectAssetLoader {
public:
    // Compatibility fallback for tests/tools. VfxEngine-owned paths should pass
    // EffectAuthoringRegistry explicitly.
    std::vector<LoadedEffectAsset> LoadDirectory(const std::filesystem::path& directory) const;
    std::vector<LoadedEffectAsset> LoadDirectory(
        const std::filesystem::path& directory,
        const EffectAuthoringRegistry& authoringRegistry) const;
    // Compatibility fallback for tests/tools. VfxEngine-owned paths should pass
    // EffectAuthoringRegistry explicitly.
    bool LoadFile(const std::filesystem::path& path, LoadedEffectAsset& outAsset) const;
    bool LoadFile(
        const std::filesystem::path& path,
        LoadedEffectAsset& outAsset,
        const EffectAuthoringRegistry& authoringRegistry) const;

private:
    static bool ApplyKeyValue(
        EffectAsset& asset,
        const std::string& key,
        const std::string& value,
        const std::filesystem::path& path,
        uint32_t lineNumber,
        std::vector<EffectAssetDiagnostic>& diagnostics);
};
