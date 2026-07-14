#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

enum class GameplayValueType {
    Bool,
    Float,
    Int,
    String,
};

struct GameplayValue {
    GameplayValueType type = GameplayValueType::Float;
    bool boolValue = false;
    float floatValue = 0.0f;
    int32_t intValue = 0;
    std::string stringValue;

    static GameplayValue Bool(bool value);
    static GameplayValue Float(float value);
    static GameplayValue Int(int32_t value);
    static GameplayValue String(std::string value);
};

struct GameplayVariableDefinition {
    std::string name;
    GameplayValue defaultValue;
};

enum class GameplayExpressionOpcode {
    ConstantBool,
    ConstantFloat,
    ConstantInt,
    ConstantString,
    LoadVariable,
    AddFloat,
    GreaterFloat,
    NotBool,
    DeltaTime,
};

struct GameplayExpression {
    GameplayExpressionOpcode opcode = GameplayExpressionOpcode::ConstantFloat;
    GameplayValueType resultType = GameplayValueType::Float;
    uint32_t left = UINT32_MAX;
    uint32_t right = UINT32_MAX;
    uint32_t variableIndex = UINT32_MAX;
    GameplayValue constant;
    std::string nodeId;
};

enum class GameplayInstructionOpcode {
    SetVariable,
    Branch,
    Print,
    EmitEvent,
    Return,
};

struct GameplayInstruction {
    GameplayInstructionOpcode opcode = GameplayInstructionOpcode::Return;
    uint32_t expression = UINT32_MAX;
    uint32_t variableIndex = UINT32_MAX;
    uint32_t next = UINT32_MAX;
    uint32_t alternate = UINT32_MAX;
    std::string eventName;
    std::string nodeId;
};

struct GameplayEventEntry {
    std::string name;
    uint32_t instruction = UINT32_MAX;
};

struct GameplayVisualScriptProgram {
    std::vector<GameplayVariableDefinition> variables;
    std::vector<GameplayExpression> expressions;
    std::vector<GameplayInstruction> instructions;
    std::vector<GameplayEventEntry> events;
    uint32_t maxInstructionsPerExecution = 4096;
    uint64_t sourceFingerprint = 0;
};

enum class GameplayExecutionStatus {
    Completed,
    EventNotFound,
    InvalidProgram,
    BudgetExceeded,
    RuntimeError,
};

struct GameplayExecutionResult {
    GameplayExecutionStatus status = GameplayExecutionStatus::InvalidProgram;
    uint32_t instructionsExecuted = 0;
    std::string error;
};

struct GameplayVisualScriptContext {
    float deltaTime = 0.0f;
    std::function<void(std::string_view)> print;
    std::function<void(std::string_view)> emitEvent;
};

class GameplayVisualScriptInstance {
public:
    bool SetProgram(const GameplayVisualScriptProgram* program, std::string* error = nullptr);
    void Reset();
    bool SetVariable(std::string_view name, const GameplayValue& value);
    const GameplayValue* GetVariable(std::string_view name) const;
    GameplayExecutionResult ExecuteEvent(
        std::string_view eventName, const GameplayVisualScriptContext& context);
    const std::vector<std::string>& Trace() const noexcept { return trace_; }

private:
    bool EvaluateExpression(uint32_t index, const GameplayVisualScriptContext& context,
        GameplayValue& output, uint32_t depth, std::string& error) const;
    const GameplayVisualScriptProgram* program_ = nullptr;
    std::vector<GameplayValue> variables_;
    std::vector<std::string> trace_;
};

const char* ToString(GameplayValueType value);
bool GameplayValueTypeFromString(std::string_view text, GameplayValueType& output);
const char* ToString(GameplayExecutionStatus value);
