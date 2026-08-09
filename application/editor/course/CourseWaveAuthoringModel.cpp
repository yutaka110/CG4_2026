#include "CourseWaveAuthoringModel.h"

#include "CourseRailAuthoringModel.h"
#include "../world/EditorWorldObjectRecord.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace editor {
namespace {

bool ValidDslText(std::string_view value, bool allowEmpty) {
    if (!allowEmpty && value.empty()) return false;
    return value.find('|') == std::string_view::npos &&
        value.find('\n') == std::string_view::npos &&
        value.find('\r') == std::string_view::npos;
}

std::string WaveLegacyKey(const CourseWaveDefinition& wave, std::size_t index) {
    return wave.displayName + ":" + std::to_string(index) + ":" +
        std::to_string(wave.triggerRailDistance);
}

} // namespace

CourseWaveAuthoringModel::CourseWaveAuthoringModel(const CourseAsset& course)
    : course_(&course) {
    const CourseRailAuthoringModel rail(course);
    if (!rail.IsValid()) {
        validationError_ = rail.ValidationError();
        return;
    }

    std::unordered_map<std::string, std::size_t> waveIndices;
    waveIndices.reserve(course.waveDefinitions.size());
    for (std::size_t index = 0; index < course.waveDefinitions.size(); ++index) {
        const CourseWaveDefinition& wave = course.waveDefinitions[index];
        if (!ValidDslText(wave.editorGuid, false) ||
            !waveIndices.emplace(wave.editorGuid, index).second) {
            validationError_ = "Course wave GUIDs must be non-empty, valid and unique.";
            return;
        }
        if (!ValidDslText(wave.displayName, false) ||
            !ValidDslText(wave.nextWaveGuid, true) ||
            !ValidDslText(wave.triggerEventId, true)) {
            validationError_ = "Course wave text fields contain invalid DSL characters.";
            return;
        }
        if (!std::isfinite(wave.triggerRailDistance) ||
            !std::isfinite(wave.prewarmDistance) ||
            !std::isfinite(wave.timeoutSeconds) ||
            wave.triggerRailDistance < 0.0f || wave.prewarmDistance < 0.0f ||
            wave.timeoutSeconds < 0.0f ||
            wave.triggerRailDistance > rail.Length() + 0.001f) {
            validationError_ =
                "Course wave distances and timeout must be finite and within the rail.";
            return;
        }
        if (wave.nextWaveGuid == wave.editorGuid) {
            validationError_ = "Course wave cannot transition to itself.";
            return;
        }
        if (wave.completionCondition ==
                CourseWaveCompletionCondition::ScriptedEvent &&
            wave.triggerEventId.empty()) {
            validationError_ =
                "Scripted-event wave completion requires a Trigger Event ID.";
            return;
        }
    }

    std::unordered_set<std::string> eventIds;
    for (const CourseEventMarker& event : course.events) {
        if (!event.id.empty()) eventIds.insert(event.id);
    }
    for (const CourseWaveDefinition& wave : course.waveDefinitions) {
        if (!wave.nextWaveGuid.empty() &&
            waveIndices.find(wave.nextWaveGuid) == waveIndices.end()) {
            validationError_ = "Course wave transition references an unknown Wave GUID.";
            return;
        }
        if (!wave.triggerEventId.empty() &&
            eventIds.find(wave.triggerEventId) == eventIds.end()) {
            validationError_ = "Course wave references an unknown Course Event ID.";
            return;
        }
    }
    for (const CourseEnemyPlacement& placement : course.enemyPlacements) {
        if (!placement.waveGroupGuid.empty() &&
            waveIndices.find(placement.waveGroupGuid) == waveIndices.end()) {
            validationError_ =
                "Enemy placement references an unknown schema-v7 Wave GUID.";
            return;
        }
    }

    std::vector<uint8_t> visit(course.waveDefinitions.size(), 0);
    std::function<bool(std::size_t)> visitWave = [&](std::size_t index) {
        if (visit[index] == 1) return false;
        if (visit[index] == 2) return true;
        visit[index] = 1;
        const std::string& next = course.waveDefinitions[index].nextWaveGuid;
        if (!next.empty()) {
            const auto found = waveIndices.find(next);
            if (found != waveIndices.end() && !visitWave(found->second)) return false;
        }
        visit[index] = 2;
        return true;
    };
    for (std::size_t index = 0; index < course.waveDefinitions.size(); ++index) {
        if (!visitWave(index)) {
            validationError_ = "Course wave transition graph contains a cycle.";
            return;
        }
    }
}

std::size_t CourseWaveAuthoringModel::EnsureStableIdentity(
    CourseAsset& course,
    std::string_view courseIdentity) {
    const std::string_view nameSpace = courseIdentity.empty()
        ? std::string_view("course") : courseIdentity;
    std::unordered_set<std::string> used;
    std::size_t assigned = 0;
    for (std::size_t index = 0; index < course.waveDefinitions.size(); ++index) {
        CourseWaveDefinition& wave = course.waveDefinitions[index];
        if (!wave.editorGuid.empty() && used.insert(wave.editorGuid).second) continue;
        uint64_t salt = static_cast<uint64_t>(index);
        do {
            wave.editorGuid = MakeDeterministicEditorWorldGuid(
                nameSpace, "course-wave", WaveLegacyKey(wave, index), salt++);
        } while (!used.insert(wave.editorGuid).second);
        ++assigned;
    }
    return assigned;
}

CourseWaveLegacyUpgradeResult CourseWaveAuthoringModel::UpgradeLegacyWaveGroups(
    CourseAsset& course,
    std::string_view courseIdentity) {
    CourseWaveLegacyUpgradeResult result{};
    result.assignedWaveGuids = EnsureStableIdentity(course, courseIdentity);

    std::unordered_map<std::string, std::string> references;
    std::unordered_set<std::string> usedGuids;
    for (const CourseWaveDefinition& wave : course.waveDefinitions) {
        usedGuids.insert(wave.editorGuid);
        references.emplace(wave.editorGuid, wave.editorGuid);
        references.emplace(wave.displayName, wave.editorGuid);
    }

    const CourseRailAuthoringModel rail(course);
    const auto makeGuid = [&](std::string_view legacyName) {
        uint64_t salt = 0;
        std::string guid;
        do {
            guid = MakeDeterministicEditorWorldGuid(
                courseIdentity.empty() ? std::string_view("course") : courseIdentity,
                "course-wave", legacyName, salt++);
        } while (!usedGuids.insert(guid).second);
        return guid;
    };

    for (CourseEnemyPlacement& placement : course.enemyPlacements) {
        if (placement.waveGroupGuid.empty()) continue;
        const std::string legacyReference = placement.waveGroupGuid;
        auto mapped = references.find(legacyReference);
        if (mapped == references.end()) {
            CourseWaveDefinition wave{};
            wave.editorGuid = makeGuid(legacyReference);
            wave.displayName = legacyReference;
            if (rail.IsValid()) {
                const RailAnchorResolution resolved = rail.Resolve(placement.railAnchor);
                if (resolved.valid) {
                    wave.triggerRailDistance = (std::clamp)(
                        resolved.railSample.distance +
                            placement.railAnchor.forwardOffset -
                            placement.activationLeadDistance,
                        0.0f,
                        rail.Length());
                }
            }
            references.emplace(legacyReference, wave.editorGuid);
            references.emplace(wave.editorGuid, wave.editorGuid);
            course.waveDefinitions.push_back(std::move(wave));
            mapped = references.find(legacyReference);
            ++result.createdWaveDefinitions;
        }
        if (placement.waveGroupGuid != mapped->second) {
            placement.waveGroupGuid = mapped->second;
            ++result.remappedEnemyReferences;
        }
    }

    for (CourseWaveDefinition& wave : course.waveDefinitions) {
        if (wave.nextWaveGuid.empty() || usedGuids.contains(wave.nextWaveGuid)) continue;
        const auto mapped = references.find(wave.nextWaveGuid);
        if (mapped != references.end()) {
            wave.nextWaveGuid = mapped->second;
            ++result.remappedWaveTransitions;
        }
    }
    return result;
}

const std::vector<CourseWaveDefinition>&
CourseWaveAuthoringModel::Waves() const noexcept {
    static const std::vector<CourseWaveDefinition> empty;
    return course_ != nullptr ? course_->waveDefinitions : empty;
}

const CourseWaveDefinition* CourseWaveAuthoringModel::Find(
    std::string_view waveGuid) const {
    const std::optional<std::size_t> index = FindIndex(waveGuid);
    return index.has_value() ? &course_->waveDefinitions[*index] : nullptr;
}

const CourseWaveDefinition* CourseWaveAuthoringModel::FindByDisplayName(
    std::string_view displayName) const {
    if (course_ == nullptr) return nullptr;
    const auto found = std::find_if(
        course_->waveDefinitions.begin(), course_->waveDefinitions.end(),
        [displayName](const CourseWaveDefinition& wave) {
            return wave.displayName == displayName;
        });
    return found == course_->waveDefinitions.end() ? nullptr : &*found;
}

std::optional<std::size_t> CourseWaveAuthoringModel::FindIndex(
    std::string_view waveGuid) const {
    if (course_ == nullptr) return std::nullopt;
    const auto found = std::find_if(
        course_->waveDefinitions.begin(), course_->waveDefinitions.end(),
        [waveGuid](const CourseWaveDefinition& wave) {
            return wave.editorGuid == waveGuid;
        });
    if (found == course_->waveDefinitions.end()) return std::nullopt;
    return static_cast<std::size_t>(found - course_->waveDefinitions.begin());
}

std::vector<const CourseEnemyPlacement*> CourseWaveAuthoringModel::Members(
    std::string_view waveGuid) const {
    std::vector<const CourseEnemyPlacement*> result;
    if (course_ == nullptr || waveGuid.empty()) return result;
    for (const CourseEnemyPlacement& placement : course_->enemyPlacements) {
        if (placement.waveGroupGuid == waveGuid) result.push_back(&placement);
    }
    return result;
}

CourseWaveResolution CourseWaveAuthoringModel::Resolve(
    std::string_view waveGuid) const {
    CourseWaveResolution result{};
    if (!IsValid()) return result;
    result.wave = Find(waveGuid);
    if (result.wave == nullptr) return result;
    result.members = Members(waveGuid);
    for (const CourseWaveDefinition& candidate : course_->waveDefinitions) {
        if (candidate.nextWaveGuid == waveGuid) {
            result.incomingTransitions.push_back(&candidate);
        }
    }
    result.valid = true;
    return result;
}

} // namespace editor
