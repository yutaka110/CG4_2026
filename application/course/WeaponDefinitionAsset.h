#pragma once

#include <cstdint>
#include <string>

#include "WeaponFireSystem.h"

inline constexpr uint32_t kWeaponDefinitionAssetSchemaVersion = 1;

struct WeaponDefinitionAsset {
    uint32_t schemaVersion = kWeaponDefinitionAssetSchemaVersion;
    std::string displayName;
    WeaponDefinition definition{};

    std::string muzzleVfxId;
    std::string tracerVfxId;
    std::string fireAudioId;
    std::string reloadAudioId;
    std::string feedbackPresetId;
    std::string aimAssistPresetId;

    float projectileRadius = 1.65f;
    float muzzleForwardOffset = 3.4f;
    float tracerForwardDistance = 30.0f;
    float muzzleRadius = 0.72f;
    float tracerRadius = 0.82f;

    bool LoadFromFile(const std::string& path, std::string* errorMessage = nullptr);
    bool Validate(std::string* errorMessage = nullptr) const;
};

const char* ToWeaponDamageTypeString(WeaponDamageType type);
