#include "WeaponDefinitionAsset.h"

#include "CourseAssetParsing.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace {
constexpr uintmax_t kMaximumWeaponAssetBytes = 64u * 1024u;

std::string Lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool ParseUInt(std::string_view text, uint32_t& output) {
    if (text.empty()) {
        return false;
    }
    uint32_t parsed = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        return false;
    }
    output = parsed;
    return true;
}

bool ParseFloat(std::string_view text, float& output) {
    if (text.empty()) {
        return false;
    }
    std::string owned{text};
    char* end = nullptr;
    const float parsed = std::strtof(owned.c_str(), &end);
    if (end == owned.c_str() || *end != '\0' || !std::isfinite(parsed)) {
        return false;
    }
    output = parsed;
    return true;
}

bool ParseBool(std::string value, bool& output) {
    value = Lower(course_asset_parsing::Trim(std::move(value)));
    if (value == "1" || value == "true" || value == "yes") {
        output = true;
        return true;
    }
    if (value == "0" || value == "false" || value == "no") {
        output = false;
        return true;
    }
    return false;
}

bool ParseFireMode(const std::string& value, WeaponFireMode& output) {
    const std::string normalized = Lower(value);
    if (normalized == "semiautomatic" || normalized == "semi_automatic") {
        output = WeaponFireMode::SemiAutomatic;
    } else if (normalized == "automatic") {
        output = WeaponFireMode::Automatic;
    } else if (normalized == "burst") {
        output = WeaponFireMode::Burst;
    } else if (normalized == "chargerelease" || normalized == "charge_release") {
        output = WeaponFireMode::ChargeRelease;
    } else if (normalized == "releasevolley" || normalized == "release_volley") {
        output = WeaponFireMode::ReleaseVolley;
    } else {
        return false;
    }
    return true;
}

bool ParseDamageType(const std::string& value, WeaponDamageType& output) {
    const std::string normalized = Lower(value);
    if (normalized == "kinetic") {
        output = WeaponDamageType::Kinetic;
    } else if (normalized == "energy") {
        output = WeaponDamageType::Energy;
    } else if (normalized == "explosive") {
        output = WeaponDamageType::Explosive;
    } else if (normalized == "ice") {
        output = WeaponDamageType::Ice;
    } else {
        return false;
    }
    return true;
}

bool ValidResourceId(const std::string& value) {
    if (value.size() > 128) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '_' || c == '-' || c == '.' || c == '/';
    });
}

bool ValidWeaponId(const std::string& value) {
    return !value.empty() && value.size() <= 96 && ValidResourceId(value);
}

bool FiniteInRange(float value, float minimum, float maximum) {
    return std::isfinite(value) && value >= minimum && value <= maximum;
}
} // namespace

bool WeaponDefinitionAsset::LoadFromFile(
    const std::string& path,
    std::string* errorMessage) {
    auto reject = [errorMessage, &path](const std::string& message) {
        if (errorMessage != nullptr) {
            *errorMessage = path + ": " + message;
        }
        return false;
    };

    std::error_code fileError;
    const uintmax_t fileBytes = std::filesystem::file_size(path, fileError);
    if (fileError || fileBytes == 0 || fileBytes > kMaximumWeaponAssetBytes) {
        return reject("file is missing, empty, or exceeds the 64 KiB limit");
    }
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return reject("could not open weapon definition asset");
    }

    WeaponDefinitionAsset loaded{};
    std::unordered_map<std::string, std::string> values;
    std::string line;
    uint32_t lineNumber = 0;
    bool headerRead = false;
    while (std::getline(file, line)) {
        ++lineNumber;
        line = course_asset_parsing::Trim(std::move(line));
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (!headerRead) {
            const std::vector<std::string> header = course_asset_parsing::SplitPipe(line);
            uint32_t schema = 0;
            if (header.size() != 2 || header[0] != "WEAPON_DEFINITION" ||
                !ParseUInt(header[1], schema) || schema != kWeaponDefinitionAssetSchemaVersion) {
                return reject("unsupported or missing WEAPON_DEFINITION schema header");
            }
            loaded.schemaVersion = schema;
            headerRead = true;
            continue;
        }
        const size_t separator = line.find('=');
        if (separator == std::string::npos) {
            return reject("expected key=value at line " + std::to_string(lineNumber));
        }
        std::string key = course_asset_parsing::Trim(line.substr(0, separator));
        std::string value = course_asset_parsing::Trim(line.substr(separator + 1));
        if (key.empty() || !values.emplace(key, value).second) {
            return reject("empty or duplicate key at line " + std::to_string(lineNumber));
        }
    }
    if (!headerRead) {
        return reject("weapon definition header was not found");
    }

    const std::unordered_set<std::string> allowedKeys{
        "weaponId", "displayName", "fireMode", "damageType", "baseDamage", "range",
        "shotInterval", "projectilesPerShot", "maxProjectilesPerTrigger", "burstCount",
        "burstInterval", "magazineCapacity", "initialReserveAmmo", "reloadDuration",
        "autoReload", "heatPerProjectile", "heatCapacity", "coolingPerSecond",
        "overheatRecoveryFraction", "minimumChargeSeconds", "maximumChargeSeconds",
        "maximumChargeDamageMultiplier", "lockOnCompatible", "muzzleVfxId", "tracerVfxId",
        "fireAudioId", "reloadAudioId", "feedbackPresetId", "aimAssistPresetId",
        "projectileRadius", "muzzleForwardOffset", "tracerForwardDistance", "muzzleRadius",
        "tracerRadius"};
    for (const auto& [key, value] : values) {
        (void)value;
        if (!allowedKeys.contains(key)) {
            return reject("unknown key: " + key);
        }
    }

    auto require = [&](const char* key) -> const std::string* {
        const auto found = values.find(key);
        return found != values.end() && !found->second.empty() ? &found->second : nullptr;
    };
    auto optional = [&](const char* key) -> const std::string* {
        const auto found = values.find(key);
        return found != values.end() ? &found->second : nullptr;
    };
    const std::string* weaponId = require("weaponId");
    const std::string* fireMode = require("fireMode");
    const std::string* damageType = require("damageType");
    const std::string* baseDamage = require("baseDamage");
    const std::string* range = require("range");
    const std::string* shotInterval = require("shotInterval");
    if (weaponId == nullptr || fireMode == nullptr || damageType == nullptr ||
        baseDamage == nullptr || range == nullptr || shotInterval == nullptr) {
        return reject("weaponId, fireMode, damageType, baseDamage, range, and shotInterval are required");
    }
    loaded.definition.weaponId = *weaponId;
    loaded.displayName = optional("displayName") != nullptr
        ? *optional("displayName")
        : loaded.definition.weaponId;
    if (!ParseFireMode(*fireMode, loaded.definition.fireMode) ||
        !ParseDamageType(*damageType, loaded.definition.damageType) ||
        !ParseFloat(*baseDamage, loaded.definition.baseDamage) ||
        !ParseFloat(*range, loaded.definition.range) ||
        !ParseFloat(*shotInterval, loaded.definition.shotInterval)) {
        return reject("one or more required values are malformed");
    }

    auto parseOptionalFloat = [&](const char* key, float& target) {
        const std::string* value = optional(key);
        return value == nullptr || ParseFloat(*value, target);
    };
    auto parseOptionalUInt = [&](const char* key, uint32_t& target) {
        const std::string* value = optional(key);
        return value == nullptr || ParseUInt(*value, target);
    };
    auto parseOptionalBool = [&](const char* key, bool& target) {
        const std::string* value = optional(key);
        return value == nullptr || ParseBool(*value, target);
    };
    if (!parseOptionalUInt("projectilesPerShot", loaded.definition.projectilesPerShot) ||
        !parseOptionalUInt("maxProjectilesPerTrigger", loaded.definition.maxProjectilesPerTrigger) ||
        !parseOptionalUInt("burstCount", loaded.definition.burstCount) ||
        !parseOptionalFloat("burstInterval", loaded.definition.burstInterval) ||
        !parseOptionalUInt("magazineCapacity", loaded.definition.magazineCapacity) ||
        !parseOptionalUInt("initialReserveAmmo", loaded.definition.initialReserveAmmo) ||
        !parseOptionalFloat("reloadDuration", loaded.definition.reloadDuration) ||
        !parseOptionalBool("autoReload", loaded.definition.autoReload) ||
        !parseOptionalFloat("heatPerProjectile", loaded.definition.heatPerProjectile) ||
        !parseOptionalFloat("heatCapacity", loaded.definition.heatCapacity) ||
        !parseOptionalFloat("coolingPerSecond", loaded.definition.coolingPerSecond) ||
        !parseOptionalFloat("overheatRecoveryFraction", loaded.definition.overheatRecoveryFraction) ||
        !parseOptionalFloat("minimumChargeSeconds", loaded.definition.minimumChargeSeconds) ||
        !parseOptionalFloat("maximumChargeSeconds", loaded.definition.maximumChargeSeconds) ||
        !parseOptionalFloat("maximumChargeDamageMultiplier", loaded.definition.maximumChargeDamageMultiplier) ||
        !parseOptionalBool("lockOnCompatible", loaded.definition.lockOnCompatible) ||
        !parseOptionalFloat("projectileRadius", loaded.projectileRadius) ||
        !parseOptionalFloat("muzzleForwardOffset", loaded.muzzleForwardOffset) ||
        !parseOptionalFloat("tracerForwardDistance", loaded.tracerForwardDistance) ||
        !parseOptionalFloat("muzzleRadius", loaded.muzzleRadius) ||
        !parseOptionalFloat("tracerRadius", loaded.tracerRadius)) {
        return reject("one or more optional values are malformed");
    }

    auto assignString = [&](const char* key, std::string& target) {
        if (const std::string* value = optional(key)) {
            target = *value;
        }
    };
    assignString("muzzleVfxId", loaded.muzzleVfxId);
    assignString("tracerVfxId", loaded.tracerVfxId);
    assignString("fireAudioId", loaded.fireAudioId);
    assignString("reloadAudioId", loaded.reloadAudioId);
    assignString("feedbackPresetId", loaded.feedbackPresetId);
    assignString("aimAssistPresetId", loaded.aimAssistPresetId);

    std::string validationError;
    if (!loaded.Validate(&validationError)) {
        return reject(validationError);
    }
    *this = std::move(loaded);
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    return true;
}

bool WeaponDefinitionAsset::Validate(std::string* errorMessage) const {
    auto reject = [errorMessage](const std::string& message) {
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        return false;
    };
    if (schemaVersion != kWeaponDefinitionAssetSchemaVersion) {
        return reject("unsupported weapon asset schema version");
    }
    if (!ValidWeaponId(definition.weaponId)) {
        return reject("weaponId contains unsupported characters or exceeds its limit");
    }
    if (displayName.empty() || displayName.size() > 128) {
        return reject("displayName must contain 1-128 characters");
    }
    std::string definitionError;
    if (!ValidateWeaponDefinition(definition, &definitionError)) {
        return reject(definitionError);
    }
    if (definition.baseDamage > 100000.0f || definition.range > 100000.0f ||
        definition.shotInterval > 60.0f || definition.projectilesPerShot > 128 ||
        definition.maxProjectilesPerTrigger > 256 || definition.burstCount > 256 ||
        definition.magazineCapacity > 1000000 || definition.initialReserveAmmo > 10000000) {
        return reject("weapon definition exceeds commercial safety limits");
    }
    if (!FiniteInRange(projectileRadius, 0.01f, 100.0f) ||
        !FiniteInRange(muzzleForwardOffset, 0.0f, 1000.0f) ||
        !FiniteInRange(tracerForwardDistance, 0.0f, 100000.0f) ||
        !FiniteInRange(muzzleRadius, 0.01f, 100.0f) ||
        !FiniteInRange(tracerRadius, 0.01f, 100.0f)) {
        return reject("weapon presentation dimensions are outside safety limits");
    }
    const std::string* resourceIds[]{
        &muzzleVfxId, &tracerVfxId, &fireAudioId, &reloadAudioId,
        &feedbackPresetId, &aimAssistPresetId};
    for (const std::string* resourceId : resourceIds) {
        if (!ValidResourceId(*resourceId)) {
            return reject("presentation resource ID contains unsupported characters");
        }
    }
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    return true;
}

const char* ToWeaponDamageTypeString(WeaponDamageType type) {
    switch (type) {
    case WeaponDamageType::Kinetic: return "Kinetic";
    case WeaponDamageType::Energy: return "Energy";
    case WeaponDamageType::Explosive: return "Explosive";
    case WeaponDamageType::Ice: return "Ice";
    }
    return "Unknown";
}
