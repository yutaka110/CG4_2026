#pragma once

#include "../EditorAssetRegistry.h"
#include "../navigation/EditorProductionNavigationPipeline.h"
#include "../scene/EditorProductionScenePipeline.h"
#include "../scene/EditorScene.h"
#include "../streaming/EditorWorldPartitionPipeline.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace editor {

inline constexpr uint32_t kEditorBehaviorTreeSchemaVersion = 1;
inline constexpr uint32_t kEditorBehaviorTreeMaximumNodes = 2048;
inline constexpr uint32_t kEditorBehaviorTreeMaximumBlackboardKeys = 256;

enum class EditorBlackboardValueType : uint8_t {
    Bool,
    Int,
    Float,
    Vector3,
    Entity,
    String,
};

struct EditorBlackboardValue {
    EditorBlackboardValueType type = EditorBlackboardValueType::Bool;
    bool boolValue = false;
    int64_t intValue = 0;
    double floatValue = 0.0;
    Vector3 vectorValue{};
    std::string textValue;
};

struct EditorBlackboardKeyDefinition {
    std::string name;
    EditorBlackboardValue defaultValue{};
};

enum class EditorBehaviorNodeType : uint8_t {
    Root,
    Selector,
    Sequence,
    Condition,
    SetBlackboard,
    Wait,
    MoveTo,
    Succeed,
    Fail,
};

struct EditorBehaviorTreeNode {
    std::string id;
    EditorBehaviorNodeType type = EditorBehaviorNodeType::Succeed;
    std::string parentId;
    uint32_t order = 0;
    std::string blackboardKey;
    std::string operation;
    std::string value;
    float durationSeconds = 0.0f;
};

struct EditorBehaviorTreeAsset {
    uint32_t schemaVersion = kEditorBehaviorTreeSchemaVersion;
    std::string assetGuid;
    std::string name;
    std::vector<EditorBlackboardKeyDefinition> blackboard;
    std::vector<EditorBehaviorTreeNode> nodes;
};

struct EditorBehaviorTreeDiagnostic {
    std::string code;
    std::string nodeId;
    std::string message;
};

struct EditorBehaviorTreeProgram {
    uint64_t sourceFingerprint = 0;
    std::string rootNodeId;
    std::vector<EditorBlackboardKeyDefinition> blackboard;
    std::vector<EditorBehaviorTreeNode> nodes;
    std::unordered_map<std::string, std::vector<uint32_t>> children;
};

struct EditorBehaviorTreeCompileResult {
    bool succeeded = false;
    EditorBehaviorTreeProgram program{};
    std::vector<EditorBehaviorTreeDiagnostic> diagnostics;
};

EditorBehaviorTreeAsset MakeDefaultEditorBehaviorTree(
    std::string assetGuid, std::string name);
EditorBehaviorTreeCompileResult CompileEditorBehaviorTree(
    const EditorBehaviorTreeAsset& asset);
bool EncodeEditorBehaviorTree(
    const EditorBehaviorTreeAsset& asset,
    std::string& output,
    std::string* errorMessage = nullptr);
bool DecodeEditorBehaviorTree(
    std::string_view input,
    EditorBehaviorTreeAsset& asset,
    std::string* errorMessage = nullptr);

enum class EditorBehaviorStatus : uint8_t {
    Succeeded,
    Failed,
    Running,
    BudgetExceeded,
    InvalidProgram,
};

struct EditorAiPerceivedStimulus {
    std::string entityGuid;
    Vector3 position{};
    float strength = 0.0f;
    bool seen = false;
    bool heard = false;
};

struct EditorAiAgentDebugSnapshot {
    std::string entityGuid;
    std::string behaviorAssetGuid;
    EditorBehaviorStatus status = EditorBehaviorStatus::InvalidProgram;
    uint64_t tickGeneration = 0;
    uint64_t perceptionGeneration = 0;
    std::vector<std::string> activeNodeTrace;
    std::vector<EditorBlackboardKeyDefinition> blackboard;
    std::vector<EditorAiPerceivedStimulus> perceived;
    std::vector<Vector3> lastPath;
    uint32_t executedNodes = 0;
};

struct EditorProductionAiPolicy {
    uint32_t maximumAgents = 256;
    uint32_t maximumStimuli = 1024;
    uint32_t maximumPerceivedPerAgent = 64;
    uint32_t maximumNodeExecutionsPerTick = 128;
    float minimumTickInterval = 1.0f / 60.0f;
    float maximumTickInterval = 1.0f;
};

struct EditorProductionAiStats {
    uint32_t submittedAgents = 0;
    uint32_t activeAgents = 0;
    uint32_t rejectedAgents = 0;
    uint32_t submittedStimuli = 0;
    uint32_t rejectedStimuli = 0;
    uint32_t sightTests = 0;
    uint32_t sightHits = 0;
    uint32_t hearingTests = 0;
    uint32_t hearingHits = 0;
    uint32_t behaviorTicks = 0;
    uint32_t successfulTicks = 0;
    uint32_t failedTicks = 0;
    uint32_t runningTicks = 0;
    uint32_t budgetFailures = 0;
    uint32_t navigationQueries = 0;
    uint32_t navigationFailures = 0;
    uint32_t loadedPrograms = 0;
    uint32_t hotReloads = 0;
    uint64_t tickGeneration = 0;
    uint64_t perceptionGeneration = 0;
};

class EditorProductionAiPipeline {
public:
    bool Initialize(EditorProductionAiPolicy policy = {},
        std::string* errorMessage = nullptr);
    void Shutdown();

    bool Sync(
        const EditorScene& scene,
        const EditorAssetRegistry& registry,
        const EditorProductionScenePipeline& productionScene,
        const EditorWorldPartitionPipeline& worldPartition,
        EditorProductionNavigationPipeline& navigation,
        float deltaTime,
        std::string* errorMessage = nullptr);

    const EditorAiAgentDebugSnapshot* DebugSnapshot(std::string_view entityGuid) const;
    const std::vector<EditorAiAgentDebugSnapshot>& DebugSnapshots() const noexcept {
        return debugSnapshots_;
    }
    const EditorProductionAiPolicy& Policy() const noexcept { return policy_; }
    const EditorProductionAiStats& Stats() const noexcept { return stats_; }
    const std::vector<std::string>& Diagnostics() const noexcept { return diagnostics_; }

private:
    struct ResidentProgram {
        EditorBehaviorTreeProgram program{};
        uint64_t sourceTimestamp = 0;
        uint64_t fileStamp = 0;
    };
    struct AgentRuntime {
        std::string behaviorAssetGuid;
        uint64_t programFingerprint = 0;
        uint64_t tickGeneration = 0;
        float accumulator = 0.0f;
        std::unordered_map<std::string, EditorBlackboardValue> blackboard;
        std::unordered_map<std::string, float> waitElapsed;
        EditorBehaviorStatus status = EditorBehaviorStatus::InvalidProgram;
        std::vector<std::string> trace;
        std::vector<EditorAiPerceivedStimulus> perceived;
        std::vector<Vector3> lastPath;
        uint32_t executedNodes = 0;
    };

    bool ResolveProgram(
        const EditorAssetRecord& record,
        const ResidentProgram*& output,
        std::string* errorMessage);

    EditorProductionAiPolicy policy_{};
    bool initialized_ = false;
    uint64_t tickGeneration_ = 0;
    uint64_t perceptionGeneration_ = 0;
    std::unordered_map<std::string, ResidentProgram> programs_;
    std::unordered_map<std::string, AgentRuntime> agents_;
    std::vector<EditorAiAgentDebugSnapshot> debugSnapshots_;
    EditorProductionAiStats stats_{};
    std::vector<std::string> diagnostics_;
};

const char* ToString(EditorBlackboardValueType type) noexcept;
const char* ToString(EditorBehaviorNodeType type) noexcept;
const char* ToString(EditorBehaviorStatus status) noexcept;

} // namespace editor
