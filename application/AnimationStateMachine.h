#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

enum class AnimationParameterType {
    Bool,
    Float,
    Int,
    Trigger,
};

enum class AnimationConditionOperator {
    Always,
    BoolTrue,
    BoolFalse,
    Greater,
    Less,
    Equals,
    Triggered,
};

struct AnimationStateMachineParameter {
    std::string name;
    AnimationParameterType type = AnimationParameterType::Bool;
    float defaultValue = 0.0f;
};

struct AnimationStateMachineState {
    std::string id;
    std::string name;
    std::string sourceAssetGuid;
    std::string clipName;
    float speed = 1.0f;
    bool loop = true;
};

struct AnimationStateMachineTransition {
    std::string id;
    uint32_t sourceState = 0;
    uint32_t targetState = 0;
    std::string parameter;
    AnimationConditionOperator condition = AnimationConditionOperator::Always;
    float threshold = 0.0f;
    float exitTimeNormalized = 0.0f;
    float blendDuration = 0.2f;
    int32_t priority = 0;
};

struct AnimationStateMachineProgram {
    uint32_t entryState = 0;
    std::vector<AnimationStateMachineParameter> parameters;
    std::vector<AnimationStateMachineState> states;
    std::vector<AnimationStateMachineTransition> transitions;
    uint64_t fingerprint = 0;
};

struct AnimationStateMachineSample {
    bool valid = false;
    uint32_t currentState = 0;
    uint32_t nextState = 0;
    float currentTime = 0.0f;
    float nextTime = 0.0f;
    float blendAlpha = 0.0f;
    bool blending = false;
};

class AnimationStateMachineInstance {
public:
    using DurationResolver =
        std::function<float(std::string_view sourceAssetGuid, std::string_view clipName)>;

    bool SetProgram(const AnimationStateMachineProgram* program);
    void Reset();
    bool SetBool(std::string_view name, bool value);
    bool SetFloat(std::string_view name, float value);
    bool SetInt(std::string_view name, int32_t value);
    bool FireTrigger(std::string_view name);
    void Update(float deltaTime, const DurationResolver& durationResolver);

    const AnimationStateMachineProgram* Program() const { return program_; }
    const AnimationStateMachineSample& Sample() const { return sample_; }

private:
    struct ParameterValue {
        AnimationParameterType type = AnimationParameterType::Bool;
        float value = 0.0f;
        bool triggered = false;
    };

    float StateDuration(uint32_t state, const DurationResolver& resolver) const;
    void AdvanceStateTime(uint32_t state, float deltaTime, float& time,
        const DurationResolver& resolver) const;
    bool ConditionPassed(const AnimationStateMachineTransition& transition,
        float normalizedTime) const;
    void ConsumeTrigger(const AnimationStateMachineTransition& transition);

    const AnimationStateMachineProgram* program_ = nullptr;
    std::unordered_map<std::string, ParameterValue> parameters_;
    AnimationStateMachineSample sample_{};
    uint32_t activeTransition_ = UINT32_MAX;
    float blendElapsed_ = 0.0f;
};

const char* ToString(AnimationParameterType value);
const char* ToString(AnimationConditionOperator value);
bool AnimationParameterTypeFromString(std::string_view value, AnimationParameterType& output);
bool AnimationConditionOperatorFromString(
    std::string_view value, AnimationConditionOperator& output);
