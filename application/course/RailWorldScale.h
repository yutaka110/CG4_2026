#pragma once

// Authoritative physical-unit convention for rail-shooter gameplay and
// authoring. Runtime simulation remains in world units so existing authored
// content does not need a destructive migration; physical values shown to
// designers must pass through this conversion boundary.
struct RailWorldScale final {
    static constexpr float kMetersPerWorldUnit = 0.40f;
    static constexpr float kWorldUnitsPerMeter =
        1.0f / kMetersPerWorldUnit;

    static constexpr float ToMeters(float worldUnits) noexcept {
        return worldUnits * kMetersPerWorldUnit;
    }

    static constexpr float ToWorldUnits(float meters) noexcept {
        return meters * kWorldUnitsPerMeter;
    }

    static constexpr float ToMetersPerSecond(
        float worldUnitsPerSecond) noexcept {
        return ToMeters(worldUnitsPerSecond);
    }

    static constexpr float ToKilometersPerHour(
        float worldUnitsPerSecond) noexcept {
        return ToMetersPerSecond(worldUnitsPerSecond) * 3.6f;
    }
};

static_assert(RailWorldScale::kMetersPerWorldUnit > 0.0f);

