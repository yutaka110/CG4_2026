#pragma once

#include <filesystem>
#include <string>

#include "TerrainGenerationSettings.h"

class TerrainPresetStore {
public:
    explicit TerrainPresetStore(std::filesystem::path path = DefaultPath());

    bool Load(TerrainGenerationSettings& settings, std::string* error = nullptr);
    bool Load(TerrainAuthoringState& authoring, std::string* error = nullptr);
    bool Save(const TerrainGenerationSettings& settings, std::string* error = nullptr);
    bool Save(const TerrainAuthoringState& authoring, std::string* error = nullptr);
    bool ReloadIfChanged(TerrainGenerationSettings& settings, std::string* error = nullptr);
    bool ReloadIfChanged(TerrainAuthoringState& authoring, std::string* error = nullptr);

    const std::filesystem::path& Path() const { return path_; }
    std::filesystem::file_time_type LastWriteTime() const { return lastWriteTime_; }

    static std::filesystem::path DefaultPath();

private:
    void TrackWriteTime();

    std::filesystem::path path_;
    std::filesystem::file_time_type lastWriteTime_{};
};
