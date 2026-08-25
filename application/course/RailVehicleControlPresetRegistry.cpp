#include "RailVehicleControlPresetRegistry.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

namespace {

constexpr uint64_t kFnvOffset = 1469598103934665603ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;

void HashBytes(uint64_t& hash, const char* data, size_t size) {
    for (size_t index = 0; index < size; ++index) {
        hash ^= static_cast<uint8_t>(data[index]);
        hash *= kFnvPrime;
    }
}

void HashString(uint64_t& hash, const std::string& value) {
    HashBytes(hash, value.data(), value.size());
    constexpr char separator = '\0';
    HashBytes(hash, &separator, 1);
}

std::string ReadAllBytes(
    const std::filesystem::path& path,
    std::string& errorMessage) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        errorMessage = "could not read " + path.generic_string();
        return {};
    }
    std::ostringstream stream;
    stream << input.rdbuf();
    if (!input.good() && !input.eof()) {
        errorMessage = "failed while reading " + path.generic_string();
        return {};
    }
    return stream.str();
}

uint32_t ActiveFallbackCount(
    const std::unordered_map<std::string, RailVehicleControlDefinitionAsset>& fallback,
    const std::unordered_map<std::string, RailVehicleControlDefinitionAsset>& loaded) {
    uint32_t count = 0;
    for (const auto& [presetId, asset] : fallback) {
        (void)asset;
        if (!loaded.contains(presetId)) ++count;
    }
    return count;
}

} // namespace

RailVehicleControlPresetRegistry::RailVehicleControlPresetRegistry() {
    Reset();
}

void RailVehicleControlPresetRegistry::Reset() {
    fallbackPresets_ = BuildFallbackPresets();
    loadedPresets_.clear();
    directory_.clear();
    loadedFingerprint_ = 0;
    stats_ = {};
    stats_.fallbackPresetCount =
        static_cast<uint32_t>(fallbackPresets_.size());
    stats_.usingFallback = true;
    stats_.status = "Fallback rail vehicle control presets active";
}

RailVehicleControlPresetRegistry::PresetMap
RailVehicleControlPresetRegistry::BuildFallbackPresets() {
    PresetMap presets;
    RailVehicleControlDefinitionAsset mineCart{};
    mineCart.presetId = kMineCartStandardPresetId;
    mineCart.displayName = "Mine Cart Standard (Fallback)";
    presets.emplace(mineCart.presetId, std::move(mineCart));
    return presets;
}

bool RailVehicleControlPresetRegistry::StageDirectory(
    const std::filesystem::path& directory,
    PresetMap& stagedPresets,
    uint64_t& fingerprint,
    std::string& errorMessage) const {
    stagedPresets.clear();
    fingerprint = kFnvOffset;
    std::error_code error;
    if (!std::filesystem::exists(directory, error) ||
        !std::filesystem::is_directory(directory, error)) {
        errorMessage = "Rail vehicle control preset directory is unavailable: " +
            directory.generic_string();
        return false;
    }
    std::vector<std::filesystem::path> paths;
    for (std::filesystem::directory_iterator iterator(directory, error), end;
         !error && iterator != end;
         iterator.increment(error)) {
        if (iterator->is_regular_file(error) &&
            iterator->path().extension() == ".railvehicle") {
            paths.push_back(iterator->path());
        }
    }
    if (error) {
        errorMessage = "Could not enumerate rail vehicle control directory: " +
            error.message();
        return false;
    }
    std::sort(paths.begin(), paths.end(), [](const auto& left, const auto& right) {
        return left.generic_string() < right.generic_string();
    });
    if (paths.empty()) {
        errorMessage = "Rail vehicle control directory contains no .railvehicle assets";
        return false;
    }
    for (const std::filesystem::path& path : paths) {
        std::string readError;
        const std::string bytes = ReadAllBytes(path, readError);
        if (!readError.empty()) {
            errorMessage = std::move(readError);
            return false;
        }
        HashString(fingerprint, path.filename().generic_string());
        HashBytes(fingerprint, bytes.data(), bytes.size());
        RailVehicleControlDefinitionAsset asset{};
        std::string loadError;
        if (!asset.LoadFromFile(path, &loadError)) {
            errorMessage = std::move(loadError);
            return false;
        }
        const std::string presetId = asset.presetId;
        if (!stagedPresets.emplace(presetId, std::move(asset)).second) {
            errorMessage = "Duplicate presetId '" + presetId + "' in " +
                directory.generic_string();
            return false;
        }
    }
    errorMessage.clear();
    return true;
}

bool RailVehicleControlPresetRegistry::LoadDirectory(
    const std::filesystem::path& directory,
    std::string* errorMessage) {
    PresetMap staged;
    uint64_t fingerprint = 0;
    std::string stagingError;
    if (!StageDirectory(directory, staged, fingerprint, stagingError)) {
        ++stats_.failedReloads;
        stats_.status = "Load failed; last-known-good vehicle controls retained: " +
            stagingError;
        if (errorMessage != nullptr) *errorMessage = stagingError;
        return false;
    }
    loadedPresets_ = std::move(staged);
    directory_ = directory;
    loadedFingerprint_ = fingerprint;
    ++stats_.revision;
    stats_.loadedPresetCount = static_cast<uint32_t>(loadedPresets_.size());
    stats_.fallbackPresetCount = ActiveFallbackCount(
        fallbackPresets_, loadedPresets_);
    stats_.usingFallback = stats_.fallbackPresetCount > 0;
    stats_.status = "Loaded " + std::to_string(loadedPresets_.size()) +
        " rail vehicle control presets from " + directory.generic_string();
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

RailVehicleControlReloadReport
RailVehicleControlPresetRegistry::ReloadChangedPresets() {
    RailVehicleControlReloadReport report{};
    report.previousRevision = stats_.revision;
    report.currentRevision = stats_.revision;
    report.loadedPresetCount = stats_.loadedPresetCount;
    report.fallbackPresetCount = stats_.fallbackPresetCount;
    if (directory_.empty()) {
        report.status = RailVehicleControlReloadStatus::Failed;
        report.message = "Rail vehicle control preset directory has not been loaded";
        ++stats_.failedReloads;
        stats_.status = report.message;
        return report;
    }
    PresetMap staged;
    uint64_t fingerprint = 0;
    std::string stagingError;
    if (!StageDirectory(directory_, staged, fingerprint, stagingError)) {
        report.status = RailVehicleControlReloadStatus::Failed;
        report.message = "Reload failed; last-known-good vehicle controls retained: " +
            stagingError;
        ++stats_.failedReloads;
        stats_.status = report.message;
        return report;
    }
    if (fingerprint == loadedFingerprint_) {
        report.message = "No rail vehicle control changes detected";
        return report;
    }
    loadedPresets_ = std::move(staged);
    loadedFingerprint_ = fingerprint;
    ++stats_.revision;
    ++stats_.successfulReloads;
    stats_.loadedPresetCount = static_cast<uint32_t>(loadedPresets_.size());
    stats_.fallbackPresetCount = ActiveFallbackCount(
        fallbackPresets_, loadedPresets_);
    stats_.usingFallback = stats_.fallbackPresetCount > 0;
    stats_.status = "Hot reloaded " + std::to_string(loadedPresets_.size()) +
        " rail vehicle control presets";
    report.status = RailVehicleControlReloadStatus::Reloaded;
    report.currentRevision = stats_.revision;
    report.loadedPresetCount = stats_.loadedPresetCount;
    report.fallbackPresetCount = stats_.fallbackPresetCount;
    report.message = stats_.status;
    return report;
}

const RailVehicleControlDefinitionAsset*
RailVehicleControlPresetRegistry::Find(const std::string& presetId) const {
    const auto loaded = loadedPresets_.find(presetId);
    if (loaded != loadedPresets_.end()) return &loaded->second;
    const auto fallback = fallbackPresets_.find(presetId);
    return fallback != fallbackPresets_.end() ? &fallback->second : nullptr;
}

bool RailVehicleControlPresetRegistry::IsUsingFallback(
    const std::string& presetId) const {
    return !loadedPresets_.contains(presetId) &&
        fallbackPresets_.contains(presetId);
}

std::filesystem::path RailVehicleControlPresetRegistry::DefaultDirectory() {
    return std::filesystem::path{"Resources"} / "rail_vehicle";
}

const char* ToRailVehicleControlReloadStatusString(
    RailVehicleControlReloadStatus status) noexcept {
    switch (status) {
    case RailVehicleControlReloadStatus::NoChange: return "No Change";
    case RailVehicleControlReloadStatus::Reloaded: return "Reloaded";
    case RailVehicleControlReloadStatus::Failed: return "Failed";
    }
    return "Unknown";
}
