#include "GameplayVisualScript.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr uint32_t kInvalid = UINT32_MAX;
constexpr uint32_t kMaxExpressionDepth = 64;
}

GameplayValue GameplayValue::Bool(bool value) {
    GameplayValue result; result.type = GameplayValueType::Bool; result.boolValue = value; return result;
}
GameplayValue GameplayValue::Float(float value) {
    GameplayValue result; result.type = GameplayValueType::Float; result.floatValue = value; return result;
}
GameplayValue GameplayValue::Int(int32_t value) {
    GameplayValue result; result.type = GameplayValueType::Int; result.intValue = value; return result;
}
GameplayValue GameplayValue::String(std::string value) {
    GameplayValue result; result.type = GameplayValueType::String; result.stringValue = std::move(value); return result;
}

bool GameplayVisualScriptInstance::SetProgram(
    const GameplayVisualScriptProgram* program, std::string* error) {
    if (program == nullptr || program->maxInstructionsPerExecution == 0) {
        if (error != nullptr) *error = "Gameplay Visual Script program is invalid.";
        return false;
    }
    for (const GameplayEventEntry& event : program->events) {
        if (event.instruction != kInvalid && event.instruction >= program->instructions.size()) {
            if (error != nullptr) *error = "Gameplay event entry points outside the program.";
            return false;
        }
    }
    program_ = program;
    Reset();
    return true;
}

void GameplayVisualScriptInstance::Reset() {
    variables_.clear();
    trace_.clear();
    if (program_ == nullptr) return;
    variables_.reserve(program_->variables.size());
    for (const GameplayVariableDefinition& variable : program_->variables) {
        variables_.push_back(variable.defaultValue);
    }
}

bool GameplayVisualScriptInstance::SetVariable(
    std::string_view name, const GameplayValue& value) {
    if (program_ == nullptr) return false;
    for (std::size_t index = 0; index < program_->variables.size(); ++index) {
        if (program_->variables[index].name == name &&
            program_->variables[index].defaultValue.type == value.type) {
            variables_[index] = value;
            return true;
        }
    }
    return false;
}

const GameplayValue* GameplayVisualScriptInstance::GetVariable(std::string_view name) const {
    if (program_ == nullptr) return nullptr;
    for (std::size_t index = 0; index < program_->variables.size(); ++index) {
        if (program_->variables[index].name == name) return &variables_[index];
    }
    return nullptr;
}

bool GameplayVisualScriptInstance::EvaluateExpression(uint32_t index,
    const GameplayVisualScriptContext& context, GameplayValue& output,
    uint32_t depth, std::string& error) const {
    if (program_ == nullptr || index >= program_->expressions.size() || depth >= kMaxExpressionDepth) {
        error = "Gameplay expression is invalid or exceeded the evaluation depth limit.";
        return false;
    }
    const GameplayExpression& expression = program_->expressions[index];
    GameplayValue left;
    GameplayValue right;
    switch (expression.opcode) {
    case GameplayExpressionOpcode::ConstantBool:
    case GameplayExpressionOpcode::ConstantFloat:
    case GameplayExpressionOpcode::ConstantInt:
    case GameplayExpressionOpcode::ConstantString:
        output = expression.constant;
        return output.type == expression.resultType;
    case GameplayExpressionOpcode::LoadVariable:
        if (expression.variableIndex >= variables_.size()) break;
        output = variables_[expression.variableIndex];
        return output.type == expression.resultType;
    case GameplayExpressionOpcode::DeltaTime:
        output = GameplayValue::Float(context.deltaTime);
        return true;
    case GameplayExpressionOpcode::AddFloat:
        if (EvaluateExpression(expression.left, context, left, depth + 1, error) &&
            EvaluateExpression(expression.right, context, right, depth + 1, error) &&
            left.type == GameplayValueType::Float && right.type == GameplayValueType::Float) {
            output = GameplayValue::Float(left.floatValue + right.floatValue);
            return std::isfinite(output.floatValue);
        }
        return false;
    case GameplayExpressionOpcode::GreaterFloat:
        if (EvaluateExpression(expression.left, context, left, depth + 1, error) &&
            EvaluateExpression(expression.right, context, right, depth + 1, error) &&
            left.type == GameplayValueType::Float && right.type == GameplayValueType::Float) {
            output = GameplayValue::Bool(left.floatValue > right.floatValue);
            return true;
        }
        return false;
    case GameplayExpressionOpcode::NotBool:
        if (EvaluateExpression(expression.left, context, left, depth + 1, error) &&
            left.type == GameplayValueType::Bool) {
            output = GameplayValue::Bool(!left.boolValue);
            return true;
        }
        return false;
    }
    error = "Gameplay expression operand or type is invalid.";
    return false;
}

GameplayExecutionResult GameplayVisualScriptInstance::ExecuteEvent(
    std::string_view eventName, const GameplayVisualScriptContext& context) {
    GameplayExecutionResult result;
    trace_.clear();
    if (program_ == nullptr) { result.error = "No Gameplay Visual Script program is bound."; return result; }
    const auto found = std::find_if(program_->events.begin(), program_->events.end(),
        [&](const GameplayEventEntry& event) { return event.name == eventName; });
    if (found == program_->events.end()) {
        result.status = GameplayExecutionStatus::EventNotFound;
        result.error = "Gameplay event is not defined: " + std::string(eventName);
        return result;
    }
    uint32_t instructionIndex = found->instruction;
    while (instructionIndex != kInvalid) {
        if (instructionIndex >= program_->instructions.size()) {
            result.status = GameplayExecutionStatus::InvalidProgram;
            result.error = "Gameplay instruction pointer is outside the program.";
            return result;
        }
        if (result.instructionsExecuted >= program_->maxInstructionsPerExecution) {
            result.status = GameplayExecutionStatus::BudgetExceeded;
            result.error = "Gameplay instruction budget exceeded; a flow cycle may be unbounded.";
            return result;
        }
        const GameplayInstruction& instruction = program_->instructions[instructionIndex];
        ++result.instructionsExecuted;
        trace_.push_back(instruction.nodeId);
        GameplayValue value;
        std::string expressionError;
        switch (instruction.opcode) {
        case GameplayInstructionOpcode::SetVariable:
            if (instruction.variableIndex >= variables_.size() ||
                !EvaluateExpression(instruction.expression, context, value, 0, expressionError) ||
                value.type != variables_[instruction.variableIndex].type) {
                result.status = GameplayExecutionStatus::RuntimeError;
                result.error = expressionError.empty() ? "Set Variable type mismatch." : expressionError;
                return result;
            }
            variables_[instruction.variableIndex] = std::move(value);
            instructionIndex = instruction.next;
            break;
        case GameplayInstructionOpcode::Branch:
            if (!EvaluateExpression(instruction.expression, context, value, 0, expressionError) ||
                value.type != GameplayValueType::Bool) {
                result.status = GameplayExecutionStatus::RuntimeError;
                result.error = expressionError.empty() ? "Branch condition is not Bool." : expressionError;
                return result;
            }
            instructionIndex = value.boolValue ? instruction.next : instruction.alternate;
            break;
        case GameplayInstructionOpcode::Print:
            if (!EvaluateExpression(instruction.expression, context, value, 0, expressionError) ||
                value.type != GameplayValueType::String) {
                result.status = GameplayExecutionStatus::RuntimeError;
                result.error = expressionError.empty() ? "Print input is not String." : expressionError;
                return result;
            }
            if (context.print) context.print(value.stringValue);
            instructionIndex = instruction.next;
            break;
        case GameplayInstructionOpcode::EmitEvent:
            if (context.emitEvent) context.emitEvent(instruction.eventName);
            instructionIndex = instruction.next;
            break;
        case GameplayInstructionOpcode::Return:
            instructionIndex = kInvalid;
            break;
        }
    }
    result.status = GameplayExecutionStatus::Completed;
    return result;
}

const char* ToString(GameplayValueType value) {
    switch (value) {
    case GameplayValueType::Bool: return "Bool";
    case GameplayValueType::Float: return "Float";
    case GameplayValueType::Int: return "Int";
    case GameplayValueType::String: return "String";
    }
    return "Float";
}

bool GameplayValueTypeFromString(std::string_view text, GameplayValueType& output) {
    if (text == "Bool") output = GameplayValueType::Bool;
    else if (text == "Float") output = GameplayValueType::Float;
    else if (text == "Int") output = GameplayValueType::Int;
    else if (text == "String") output = GameplayValueType::String;
    else return false;
    return true;
}

const char* ToString(GameplayExecutionStatus value) {
    switch (value) {
    case GameplayExecutionStatus::Completed: return "Completed";
    case GameplayExecutionStatus::EventNotFound: return "EventNotFound";
    case GameplayExecutionStatus::InvalidProgram: return "InvalidProgram";
    case GameplayExecutionStatus::BudgetExceeded: return "BudgetExceeded";
    case GameplayExecutionStatus::RuntimeError: return "RuntimeError";
    }
    return "InvalidProgram";
}
