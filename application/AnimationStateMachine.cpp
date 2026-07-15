#include "AnimationStateMachine.h"

#include <algorithm>
#include <cmath>

bool AnimationStateMachineInstance::SetProgram(const AnimationStateMachineProgram* program) {
    if (program == nullptr || program->states.empty() || program->entryState >= program->states.size()) {
        program_ = nullptr;
        parameters_.clear();
        sample_ = {};
        return false;
    }
    program_ = program;
    Reset();
    return true;
}

void AnimationStateMachineInstance::Reset() {
    parameters_.clear();
    sample_ = {};
    activeTransition_ = UINT32_MAX;
    blendElapsed_ = 0.0f;
    if (program_ == nullptr || program_->entryState >= program_->states.size()) return;
    for (const AnimationStateMachineParameter& parameter : program_->parameters) {
        parameters_[parameter.name] = {parameter.type, parameter.defaultValue, false};
    }
    sample_.valid = true;
    sample_.currentState = program_->entryState;
    sample_.nextState = program_->entryState;
}

bool AnimationStateMachineInstance::SetBool(std::string_view name, bool value) {
    const auto found = parameters_.find(std::string(name));
    if (found == parameters_.end() || found->second.type != AnimationParameterType::Bool) return false;
    found->second.value = value ? 1.0f : 0.0f;
    return true;
}

bool AnimationStateMachineInstance::SetFloat(std::string_view name, float value) {
    const auto found = parameters_.find(std::string(name));
    if (found == parameters_.end() || found->second.type != AnimationParameterType::Float ||
        !std::isfinite(value)) return false;
    found->second.value = value;
    return true;
}

bool AnimationStateMachineInstance::SetInt(std::string_view name, int32_t value) {
    const auto found = parameters_.find(std::string(name));
    if (found == parameters_.end() || found->second.type != AnimationParameterType::Int) return false;
    found->second.value = static_cast<float>(value);
    return true;
}

bool AnimationStateMachineInstance::FireTrigger(std::string_view name) {
    const auto found = parameters_.find(std::string(name));
    if (found == parameters_.end() || found->second.type != AnimationParameterType::Trigger) return false;
    found->second.triggered = true;
    return true;
}

float AnimationStateMachineInstance::StateDuration(
    uint32_t state, const DurationResolver& resolver) const {
    if (program_ == nullptr || state >= program_->states.size() || !resolver) return 1.0f;
    const AnimationStateMachineState& definition = program_->states[state];
    const float duration = resolver(definition.sourceAssetGuid, definition.clipName);
    return std::isfinite(duration) && duration > 0.0f ? duration : 1.0f;
}

void AnimationStateMachineInstance::AdvanceStateTime(uint32_t state, float deltaTime,
    float& time, const DurationResolver& resolver) const {
    if (program_ == nullptr || state >= program_->states.size()) return;
    const AnimationStateMachineState& definition = program_->states[state];
    const float duration = StateDuration(state, resolver);
    time += deltaTime * definition.speed;
    if (definition.loop) {
        time = std::fmod(time, duration);
        if (time < 0.0f) time += duration;
    } else {
        time = (std::clamp)(time, 0.0f, duration);
    }
}

bool AnimationStateMachineInstance::ConditionPassed(
    const AnimationStateMachineTransition& transition, float normalizedTime) const {
    if (normalizedTime + 0.00001f < transition.exitTimeNormalized) return false;
    if (transition.condition == AnimationConditionOperator::Always) return true;
    const auto found = parameters_.find(transition.parameter);
    if (found == parameters_.end()) return false;
    const ParameterValue& value = found->second;
    switch (transition.condition) {
    case AnimationConditionOperator::Always: return true;
    case AnimationConditionOperator::BoolTrue:
        return value.type == AnimationParameterType::Bool && value.value != 0.0f;
    case AnimationConditionOperator::BoolFalse:
        return value.type == AnimationParameterType::Bool && value.value == 0.0f;
    case AnimationConditionOperator::Greater:
        return (value.type == AnimationParameterType::Float || value.type == AnimationParameterType::Int) &&
            value.value > transition.threshold;
    case AnimationConditionOperator::Less:
        return (value.type == AnimationParameterType::Float || value.type == AnimationParameterType::Int) &&
            value.value < transition.threshold;
    case AnimationConditionOperator::Equals:
        return (value.type == AnimationParameterType::Float || value.type == AnimationParameterType::Int) &&
            std::fabs(value.value - transition.threshold) <= 0.0001f;
    case AnimationConditionOperator::Triggered:
        return value.type == AnimationParameterType::Trigger && value.triggered;
    }
    return false;
}

void AnimationStateMachineInstance::ConsumeTrigger(
    const AnimationStateMachineTransition& transition) {
    if (transition.condition != AnimationConditionOperator::Triggered) return;
    const auto found = parameters_.find(transition.parameter);
    if (found != parameters_.end()) found->second.triggered = false;
}

void AnimationStateMachineInstance::Update(float deltaTime,
    const DurationResolver& durationResolver) {
    if (program_ == nullptr || !sample_.valid || !std::isfinite(deltaTime) || deltaTime < 0.0f) return;
    AdvanceStateTime(sample_.currentState, deltaTime, sample_.currentTime, durationResolver);
    if (sample_.blending) {
        AdvanceStateTime(sample_.nextState, deltaTime, sample_.nextTime, durationResolver);
        const AnimationStateMachineTransition& transition = program_->transitions[activeTransition_];
        blendElapsed_ += deltaTime;
        sample_.blendAlpha = transition.blendDuration <= 0.0f
            ? 1.0f : (std::min)(1.0f, blendElapsed_ / transition.blendDuration);
        if (sample_.blendAlpha >= 1.0f) {
            sample_.currentState = sample_.nextState;
            sample_.currentTime = sample_.nextTime;
            sample_.blending = false;
            sample_.blendAlpha = 0.0f;
            activeTransition_ = UINT32_MAX;
            blendElapsed_ = 0.0f;
        }
        return;
    }

    const float duration = StateDuration(sample_.currentState, durationResolver);
    const float normalizedTime = duration > 0.0f ? sample_.currentTime / duration : 0.0f;
    std::vector<uint32_t> candidates;
    for (uint32_t index = 0; index < program_->transitions.size(); ++index) {
        const AnimationStateMachineTransition& transition = program_->transitions[index];
        if (transition.sourceState == sample_.currentState && ConditionPassed(transition, normalizedTime)) {
            candidates.push_back(index);
        }
    }
    if (candidates.empty()) return;
    std::stable_sort(candidates.begin(), candidates.end(), [&](uint32_t lhs, uint32_t rhs) {
        return program_->transitions[lhs].priority > program_->transitions[rhs].priority;
    });
    activeTransition_ = candidates.front();
    const AnimationStateMachineTransition& transition = program_->transitions[activeTransition_];
    ConsumeTrigger(transition);
    sample_.nextState = transition.targetState;
    sample_.nextTime = 0.0f;
    sample_.blending = transition.blendDuration > 0.0f;
    sample_.blendAlpha = sample_.blending ? 0.0f : 1.0f;
    if (!sample_.blending) {
        sample_.currentState = sample_.nextState;
        sample_.currentTime = 0.0f;
        sample_.blendAlpha = 0.0f;
        activeTransition_ = UINT32_MAX;
    }
}

const char* ToString(AnimationParameterType value) {
    switch (value) {
    case AnimationParameterType::Bool: return "Bool";
    case AnimationParameterType::Float: return "Float";
    case AnimationParameterType::Int: return "Int";
    case AnimationParameterType::Trigger: return "Trigger";
    }
    return "Bool";
}

const char* ToString(AnimationConditionOperator value) {
    switch (value) {
    case AnimationConditionOperator::Always: return "Always";
    case AnimationConditionOperator::BoolTrue: return "BoolTrue";
    case AnimationConditionOperator::BoolFalse: return "BoolFalse";
    case AnimationConditionOperator::Greater: return "Greater";
    case AnimationConditionOperator::Less: return "Less";
    case AnimationConditionOperator::Equals: return "Equals";
    case AnimationConditionOperator::Triggered: return "Triggered";
    }
    return "Always";
}

bool AnimationParameterTypeFromString(std::string_view value, AnimationParameterType& output) {
    if (value == "Bool") output = AnimationParameterType::Bool;
    else if (value == "Float") output = AnimationParameterType::Float;
    else if (value == "Int") output = AnimationParameterType::Int;
    else if (value == "Trigger") output = AnimationParameterType::Trigger;
    else return false;
    return true;
}

bool AnimationConditionOperatorFromString(
    std::string_view value, AnimationConditionOperator& output) {
    if (value == "Always") output = AnimationConditionOperator::Always;
    else if (value == "BoolTrue") output = AnimationConditionOperator::BoolTrue;
    else if (value == "BoolFalse") output = AnimationConditionOperator::BoolFalse;
    else if (value == "Greater") output = AnimationConditionOperator::Greater;
    else if (value == "Less") output = AnimationConditionOperator::Less;
    else if (value == "Equals") output = AnimationConditionOperator::Equals;
    else if (value == "Triggered") output = AnimationConditionOperator::Triggered;
    else return false;
    return true;
}
