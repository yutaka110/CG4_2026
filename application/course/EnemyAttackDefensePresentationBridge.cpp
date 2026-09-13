#include "EnemyAttackDefensePresentationBridge.h"

#include "CourseSpawnRuntime.h"

#include <algorithm>
#include <unordered_set>

namespace {

EnemyAttackDefenseResponse ResponsesForActor(
    const CourseSpawnRuntime& runtime,
    uint32_t actorId) noexcept {
    for (const CourseEnemyActor& actor : runtime.Enemies()) {
        if (actor.actorId == actorId) {
            return actor.desc.projectileDefinition.defenseResponses;
        }
    }
    return EnemyAttackDefenseResponse::None;
}

EnemyAttackDefensePromptAction SelectPose(
    EnemyAttackDefenseResponse responses,
    float threatDirectionX) noexcept {
    if (HasDefenseResponse(responses, EnemyAttackDefenseResponse::Duck)) {
        return EnemyAttackDefensePromptAction::Duck;
    }
    const bool left = HasDefenseResponse(
        responses, EnemyAttackDefenseResponse::LeanLeft);
    const bool right = HasDefenseResponse(
        responses, EnemyAttackDefenseResponse::LeanRight);
    if (left && right) {
        return threatDirectionX >= 0.0f
            ? EnemyAttackDefensePromptAction::LeanLeft
            : EnemyAttackDefensePromptAction::LeanRight;
    }
    if (left) return EnemyAttackDefensePromptAction::LeanLeft;
    if (right) return EnemyAttackDefensePromptAction::LeanRight;
    return EnemyAttackDefensePromptAction::None;
}

EnemyAttackDefensePromptAction SelectAction(
    EnemyAttackDefenseResponse responses,
    EnemyAttackTelegraphPhase phase,
    float threatDirectionX,
    bool projectileInFlight) noexcept {
    if (projectileInFlight || phase == EnemyAttackTelegraphPhase::Fired) {
        if (HasDefenseResponse(responses, EnemyAttackDefenseResponse::ShootDown)) {
            return EnemyAttackDefensePromptAction::ShootDown;
        }
        return SelectPose(responses, threatDirectionX);
    }
    if (phase == EnemyAttackTelegraphPhase::Warming ||
        phase == EnemyAttackTelegraphPhase::Tracking) {
        if (HasDefenseResponse(responses, EnemyAttackDefenseResponse::Interrupt)) {
            return EnemyAttackDefensePromptAction::Interrupt;
        }
        if (HasDefenseResponse(responses, EnemyAttackDefenseResponse::ShootDown)) {
            return EnemyAttackDefensePromptAction::ShootDown;
        }
    }
    EnemyAttackDefensePromptAction pose = SelectPose(
        responses, threatDirectionX);
    if (pose != EnemyAttackDefensePromptAction::None) return pose;
    if (HasDefenseResponse(responses, EnemyAttackDefenseResponse::ShootDown)) {
        return EnemyAttackDefensePromptAction::ShootDown;
    }
    return EnemyAttackDefensePromptAction::None;
}

Vector4 ColorFor(EnemyAttackDefensePromptAction action) noexcept {
    switch (action) {
    case EnemyAttackDefensePromptAction::Interrupt:
        return {1.0f, 0.72f, 0.16f, 1.0f};
    case EnemyAttackDefensePromptAction::ShootDown:
        return {0.30f, 0.95f, 1.0f, 1.0f};
    case EnemyAttackDefensePromptAction::LeanLeft:
    case EnemyAttackDefensePromptAction::LeanRight:
        return {1.0f, 0.20f, 0.76f, 1.0f};
    case EnemyAttackDefensePromptAction::Duck:
        return {0.46f, 0.68f, 1.0f, 1.0f};
    default:
        return {0.72f, 0.78f, 0.82f, 1.0f};
    }
}

bool ActionSatisfied(
    EnemyAttackDefensePromptAction action,
    const RailVehicleMountedDefenseFrame* defense) noexcept {
    if (defense == nullptr || !defense->state.active) return false;
    switch (action) {
    case EnemyAttackDefensePromptAction::LeanLeft:
        return defense->state.action == RailVehicleMountedDefenseAction::LeanLeft;
    case EnemyAttackDefensePromptAction::LeanRight:
        return defense->state.action == RailVehicleMountedDefenseAction::LeanRight;
    case EnemyAttackDefensePromptAction::Duck:
        return defense->state.action == RailVehicleMountedDefenseAction::Duck;
    default:
        return false;
    }
}

EnemyAttackDefenseDecisionPhase ResolveDecisionPhase(
    EnemyAttackTelegraphPhase phase,
    bool projectileInFlight) noexcept {
    if (projectileInFlight || phase == EnemyAttackTelegraphPhase::Fired) {
        return EnemyAttackDefenseDecisionPhase::ProjectileInFlight;
    }
    if (phase == EnemyAttackTelegraphPhase::Imminent) {
        return EnemyAttackDefenseDecisionPhase::FinalCommit;
    }
    return EnemyAttackDefenseDecisionPhase::EarlyWarning;
}

EnemyAttackDefenseDecisionAvailability AvailabilityFor(
    EnemyAttackDefenseDecisionStrategy strategy,
    EnemyAttackDefenseDecisionPhase phase) noexcept {
    switch (strategy) {
    case EnemyAttackDefenseDecisionStrategy::PreventLaunch:
        return phase == EnemyAttackDefenseDecisionPhase::ProjectileInFlight
            ? EnemyAttackDefenseDecisionAvailability::Closed
            : EnemyAttackDefenseDecisionAvailability::AvailableNow;
    case EnemyAttackDefenseDecisionStrategy::DestroyProjectile:
        return phase == EnemyAttackDefenseDecisionPhase::ProjectileInFlight
            ? EnemyAttackDefenseDecisionAvailability::AvailableNow
            : EnemyAttackDefenseDecisionAvailability::AfterLaunch;
    case EnemyAttackDefenseDecisionStrategy::EvadeImpact:
        if (phase == EnemyAttackDefenseDecisionPhase::EarlyWarning) {
            return EnemyAttackDefenseDecisionAvailability::AtImpact;
        }
        return EnemyAttackDefenseDecisionAvailability::AvailableNow;
    }
    return EnemyAttackDefenseDecisionAvailability::Closed;
}

void BuildDecisionOptions(
    EnemyAttackDefensePresentationCue& cue,
    const RailVehicleMountedDefenseFrame* defense) noexcept {
    cue.decisionOptionCount = 0;
    cue.availableNowCount = 0;
    cue.decisionOptions = {};

    const auto add = [&](EnemyAttackDefensePromptAction action,
                         EnemyAttackDefenseDecisionStrategy strategy) {
        if (action == EnemyAttackDefensePromptAction::None ||
            cue.decisionOptionCount >= cue.decisionOptions.size()) {
            return;
        }
        EnemyAttackDefenseDecisionOption& option =
            cue.decisionOptions[cue.decisionOptionCount++];
        option.action = action;
        option.strategy = strategy;
        option.availability = AvailabilityFor(strategy, cue.decisionPhase);
        option.recommended = action == cue.primaryAction;
        option.actionSatisfied = ActionSatisfied(action, defense);
        if (option.availability ==
            EnemyAttackDefenseDecisionAvailability::AvailableNow) {
            ++cue.availableNowCount;
        }
    };

    if (HasDefenseResponse(
            cue.availableResponses, EnemyAttackDefenseResponse::Interrupt)) {
        add(EnemyAttackDefensePromptAction::Interrupt,
            EnemyAttackDefenseDecisionStrategy::PreventLaunch);
    }
    if (HasDefenseResponse(
            cue.availableResponses, EnemyAttackDefenseResponse::ShootDown)) {
        add(EnemyAttackDefensePromptAction::ShootDown,
            EnemyAttackDefenseDecisionStrategy::DestroyProjectile);
    }
    add(SelectPose(cue.availableResponses, cue.directionFromCenter.x),
        EnemyAttackDefenseDecisionStrategy::EvadeImpact);

    cue.hasMeaningfulChoice = cue.decisionOptionCount >= 2;
    cue.actionSatisfied = ActionSatisfied(cue.primaryAction, defense);
}

} // namespace

void EnemyAttackDefensePresentationBridge::Reset() {
    tracked_.clear();
    frame_ = {};
    revision_ = 0;
}

void EnemyAttackDefensePresentationBridge::Update(
    const EnemyAttackDefensePresentationInput& input) {
    EnemyAttackDefensePresentationFrame next{};
    next.revision = ++revision_;
    if (!input.settings.enabled || !input.gameplayActive ||
        input.telegraph == nullptr || input.runtime == nullptr) {
        tracked_.clear();
        frame_ = std::move(next);
        return;
    }
    next.sourceTelegraphRevision = input.telegraph->revision;
    if (input.projectiles != nullptr) {
        next.sourceProjectileRevision = input.projectiles->revision;
    }

    std::unordered_set<uint32_t> liveProjectileOwners;
    if (input.projectiles != nullptr) {
        for (const EnemyProjectilePresentation& projectile :
             input.projectiles->projectiles) {
            if (projectile.threat) {
                liveProjectileOwners.insert(projectile.ownerActorId);
            }
        }
    }

    std::unordered_set<uint32_t> touched;
    for (const EnemyAttackTelegraphCue& source : input.telegraph->cues) {
        const EnemyAttackDefenseResponse responses = ResponsesForActor(
            *input.runtime, source.actorId);
        const bool projectileInFlight =
            liveProjectileOwners.contains(source.actorId);
        const EnemyAttackDefensePromptAction action = SelectAction(
            responses, source.phase, source.directionFromCenter.x,
            projectileInFlight);
        if (action == EnemyAttackDefensePromptAction::None) continue;

        EnemyAttackDefensePresentationCue cue{};
        cue.actorId = source.actorId;
        cue.attackIntentSequence = source.attackIntentSequence;
        cue.primaryAction = action;
        cue.availableResponses = responses;
        cue.phase = source.phase;
        cue.decisionPhase = ResolveDecisionPhase(
            source.phase, projectileInFlight);
        cue.screenPosition = source.screenPosition;
        cue.directionFromCenter = source.directionFromCenter;
        cue.color = ColorFor(action);
        cue.timeToFire = source.timeToFire;
        cue.urgency = source.urgency;
        cue.priority = source.priority + (projectileInFlight ? 0.35f : 0.0f);
        cue.pulse = source.pulse;
        cue.onScreen = source.onScreen;
        cue.projectileInFlight = projectileInFlight;
        BuildDecisionOptions(cue, input.mountedDefense);
        tracked_[source.actorId] = {
            cue, input.settings.projectilePromptHoldSeconds};
        touched.insert(source.actorId);
    }

    for (auto it = tracked_.begin(); it != tracked_.end();) {
        TrackedCue& tracked = it->second;
        const bool projectileInFlight = liveProjectileOwners.contains(it->first);
        if (!touched.contains(it->first) && projectileInFlight) {
            tracked.cue.projectileInFlight = true;
            tracked.cue.phase = EnemyAttackTelegraphPhase::Fired;
            tracked.cue.timeToFire = 0.0f;
            tracked.cue.urgency = 1.0f;
            tracked.cue.priority = (std::max)(tracked.cue.priority, 1.25f);
            tracked.cue.primaryAction = SelectAction(
                tracked.cue.availableResponses,
                EnemyAttackTelegraphPhase::Fired,
                tracked.cue.directionFromCenter.x,
                true);
            tracked.cue.decisionPhase =
                EnemyAttackDefenseDecisionPhase::ProjectileInFlight;
            tracked.cue.color = ColorFor(tracked.cue.primaryAction);
            BuildDecisionOptions(tracked.cue, input.mountedDefense);
            ++next.projectilePrompts;
        } else if (!touched.contains(it->first)) {
            tracked.graceRemaining -= (std::clamp)(
                input.deltaTime, 0.0f, 0.25f);
            if (tracked.graceRemaining <= 0.0f) {
                it = tracked_.erase(it);
                continue;
            }
        }
        if (tracked.cue.primaryAction != EnemyAttackDefensePromptAction::None) {
            ++next.candidates;
            next.cues.push_back(tracked.cue);
        }
        ++it;
    }

    std::sort(next.cues.begin(), next.cues.end(),
        [](const auto& a, const auto& b) {
            if (a.actionSatisfied != b.actionSatisfied) {
                return !a.actionSatisfied;
            }
            if (a.priority != b.priority) return a.priority > b.priority;
            return a.actorId < b.actorId;
        });
    const size_t budget = (std::max)(
        static_cast<size_t>(1), input.settings.maximumVisiblePrompts);
    if (next.cues.size() > budget) {
        next.droppedByBudget = static_cast<uint32_t>(next.cues.size() - budget);
        next.cues.resize(budget);
    }
    frame_ = std::move(next);
}

const char* ToString(EnemyAttackDefensePromptAction action) noexcept {
    switch (action) {
    case EnemyAttackDefensePromptAction::Interrupt: return "INTERRUPT";
    case EnemyAttackDefensePromptAction::ShootDown: return "SHOOT";
    case EnemyAttackDefensePromptAction::LeanLeft: return "LEAN LEFT";
    case EnemyAttackDefensePromptAction::LeanRight: return "LEAN RIGHT";
    case EnemyAttackDefensePromptAction::Duck: return "DUCK";
    default: return "";
    }
}

const char* ToString(EnemyAttackDefenseDecisionPhase phase) noexcept {
    switch (phase) {
    case EnemyAttackDefenseDecisionPhase::EarlyWarning: return "EARLY WINDOW";
    case EnemyAttackDefenseDecisionPhase::FinalCommit: return "COMMIT WINDOW";
    case EnemyAttackDefenseDecisionPhase::ProjectileInFlight:
        return "SHOT IN FLIGHT";
    }
    return "";
}

const char* ToString(EnemyAttackDefenseDecisionStrategy strategy) noexcept {
    switch (strategy) {
    case EnemyAttackDefenseDecisionStrategy::PreventLaunch:
        return "CANCEL ATTACK";
    case EnemyAttackDefenseDecisionStrategy::DestroyProjectile:
        return "DESTROY SHOT";
    case EnemyAttackDefenseDecisionStrategy::EvadeImpact:
        return "AVOID IMPACT";
    }
    return "";
}

const char* ToString(
    EnemyAttackDefenseDecisionAvailability availability) noexcept {
    switch (availability) {
    case EnemyAttackDefenseDecisionAvailability::Closed: return "CLOSED";
    case EnemyAttackDefenseDecisionAvailability::AvailableNow: return "NOW";
    case EnemyAttackDefenseDecisionAvailability::AfterLaunch:
        return "AFTER LAUNCH";
    case EnemyAttackDefenseDecisionAvailability::AtImpact: return "AT IMPACT";
    }
    return "";
}
