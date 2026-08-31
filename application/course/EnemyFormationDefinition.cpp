#include "EnemyFormationDefinition.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <utility>

namespace {
std::string Lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}
void SetError(std::string* error, const char* message) {
    if (error != nullptr) *error = message;
}
}

EnemyFormationDefinition EnemyFormationDefinition::CommercialDefault(
    std::string formationId) {
    EnemyFormationDefinition result{};
    result.definitionId = std::move(formationId);
    return result;
}

bool EnemyFormationDefinition::Validate(std::string* errorMessage) const {
    const bool finite = std::isfinite(slotSpacing) &&
        std::isfinite(verticalSpacing) && std::isfinite(cohesionResponse) &&
        std::isfinite(maximumCorrection) &&
        std::isfinite(attackStaggerSeconds) &&
        std::isfinite(entranceDurationSeconds) &&
        std::isfinite(entranceStaggerSeconds) &&
        std::isfinite(entranceForwardDistance) &&
        std::isfinite(entranceSideDistance) &&
        std::isfinite(exitDurationSeconds) &&
        std::isfinite(exitForwardDistance) &&
        std::isfinite(exitSideDistance);
    if (!finite || slotSpacing < 0.0f || verticalSpacing < 0.0f ||
        cohesionResponse < 0.0f || maximumCorrection < 0.0f ||
        attackStaggerSeconds < 0.0f || entranceDurationSeconds < 0.05f ||
        entranceStaggerSeconds < 0.0f || entranceForwardDistance < 0.0f ||
        entranceSideDistance < 0.0f || exitDurationSeconds < 0.05f ||
        exitForwardDistance < 0.0f || exitSideDistance < 0.0f) {
        SetError(errorMessage,
            "EnemyFormationDefinition contains invalid commercial values.");
        return false;
    }
    return true;
}

bool TryParseEnemyFormationPattern(
    const std::string& text, EnemyFormationPattern& pattern) noexcept {
    const std::string value = Lower(text);
    if (value == "authored") pattern = EnemyFormationPattern::Authored;
    else if (value == "v") pattern = EnemyFormationPattern::V;
    else if (value == "line" || value == "line_abreast")
        pattern = EnemyFormationPattern::LineAbreast;
    else if (value == "column") pattern = EnemyFormationPattern::Column;
    else if (value == "echelon_left") pattern = EnemyFormationPattern::EchelonLeft;
    else if (value == "echelon_right") pattern = EnemyFormationPattern::EchelonRight;
    else if (value == "ring" || value == "pinwheel")
        pattern = EnemyFormationPattern::Ring;
    else return false;
    return true;
}

bool TryParseEnemyEntranceStyle(
    const std::string& text, EnemyEntranceStyle& style) noexcept {
    const std::string value = Lower(text);
    if (value == "ahead" || value == "from_ahead") style = EnemyEntranceStyle::FromAhead;
    else if (value == "left" || value == "sweep_left") style = EnemyEntranceStyle::SweepLeft;
    else if (value == "right" || value == "sweep_right") style = EnemyEntranceStyle::SweepRight;
    else if (value == "dive") style = EnemyEntranceStyle::Dive;
    else if (value == "rise") style = EnemyEntranceStyle::Rise;
    else if (value == "rear" || value == "rear_chase") style = EnemyEntranceStyle::RearChase;
    else return false;
    return true;
}

bool TryParseEnemyExitStyle(
    const std::string& text, EnemyExitStyle& style) noexcept {
    const std::string value = Lower(text);
    if (value == "forward" || value == "forward_break") style = EnemyExitStyle::ForwardBreak;
    else if (value == "split" || value == "split_sides") style = EnemyExitStyle::SplitSides;
    else if (value == "climb") style = EnemyExitStyle::Climb;
    else if (value == "dive") style = EnemyExitStyle::Dive;
    else if (value == "rear" || value == "rear_retreat") style = EnemyExitStyle::RearRetreat;
    else return false;
    return true;
}

const char* ToString(EnemyFormationPattern pattern) noexcept {
    switch (pattern) {
    case EnemyFormationPattern::Authored: return "Authored";
    case EnemyFormationPattern::V: return "V";
    case EnemyFormationPattern::LineAbreast: return "LineAbreast";
    case EnemyFormationPattern::Column: return "Column";
    case EnemyFormationPattern::EchelonLeft: return "EchelonLeft";
    case EnemyFormationPattern::EchelonRight: return "EchelonRight";
    case EnemyFormationPattern::Ring: return "Ring";
    }
    return "Unknown";
}

const char* ToString(EnemyEntranceStyle style) noexcept {
    switch (style) {
    case EnemyEntranceStyle::FromAhead: return "FromAhead";
    case EnemyEntranceStyle::SweepLeft: return "SweepLeft";
    case EnemyEntranceStyle::SweepRight: return "SweepRight";
    case EnemyEntranceStyle::Dive: return "Dive";
    case EnemyEntranceStyle::Rise: return "Rise";
    case EnemyEntranceStyle::RearChase: return "RearChase";
    }
    return "Unknown";
}

const char* ToString(EnemyExitStyle style) noexcept {
    switch (style) {
    case EnemyExitStyle::ForwardBreak: return "ForwardBreak";
    case EnemyExitStyle::SplitSides: return "SplitSides";
    case EnemyExitStyle::Climb: return "Climb";
    case EnemyExitStyle::Dive: return "Dive";
    case EnemyExitStyle::RearRetreat: return "RearRetreat";
    }
    return "Unknown";
}
