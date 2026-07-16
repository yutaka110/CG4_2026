#pragma once

#include "EditorProductionAiPipeline.h"
#include "EditorProductionAiWorldPipeline.h"
#include "../EditorViewportOverlay.h"
#include "../core/EditorExecutionService.h"
#include "../documents/EditorAiDocumentProviders.h"
#include "../play/IEditorPlayIsolationProvider.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace editor {

class EditorDocumentManager;
class EditorTransactionStack;

inline constexpr uint32_t kEditorAiSimulationRecordingSchemaVersion = 1;

struct EditorAiAuthoringPolicy {
    uint32_t maximumBreakpoints = 128;
    uint32_t maximumRecordedFrames = 600;
    uint32_t maximumAgentsPerFrame = 256;
    uint32_t maximumCrowdAgentsPerFrame = 256;
    uint32_t maximumSmartObjectSlotsPerFrame = 1024;
    uint32_t maximumOverlayCommands = 2048;
};

struct EditorAiBreakpoint {
    std::string nodeId;
    std::string agentEntityGuid;

    bool Matches(const EditorAiAgentDebugSnapshot& snapshot) const;
};

struct EditorAiSimulationFrame {
    uint64_t frameIndex = 0;
    uint64_t behaviorGeneration = 0;
    uint64_t worldGeneration = 0;
    float deltaTime = 0.0f;
    uint64_t fingerprint = 0;
    std::vector<EditorAiAgentDebugSnapshot> agents;
    std::vector<EditorCrowdAgentSnapshot> crowd;
    std::vector<EditorSmartObjectSlot> smartObjects;
};

struct EditorAiAuthoringStats {
    uint32_t behaviorMutations = 0;
    uint32_t eqsMutations = 0;
    uint32_t compileFailures = 0;
    uint32_t breakpointHits = 0;
    uint32_t liveFrames = 0;
    uint32_t recordedFrames = 0;
    uint32_t droppedRecordingFrames = 0;
    uint32_t replaySteps = 0;
    uint32_t exportedRecordings = 0;
    uint32_t importedRecordings = 0;
    uint32_t overlayCommands = 0;
    uint32_t overlayBudgetRejected = 0;
};

struct EditorAiPlayIsolationState {
    bool paused = false;
    bool recording = false;
    bool replaying = false;
    uint32_t replayFrame = 0;
    std::vector<EditorAiBreakpoint> breakpoints;
    uint64_t nextFrameIndex = 1;
    std::vector<EditorAiSimulationFrame> frames;
    EditorAiSimulationFrame liveFrame{};
};

class EditorProductionAiAuthoringPipeline final :
    public IEditorExecutionService,
    public IEditorViewportOverlayProvider,
    public IEditorPlayIsolationProvider {
public:
    static constexpr std::string_view kServiceId = "editor.ai.authoring.execution";
    static constexpr std::string_view kPlayIsolationId = "editor.playIsolation.aiDebugger";

    std::string_view ServiceId() const noexcept override { return kServiceId; }
    std::string_view Id() const noexcept override { return kPlayIsolationId; }
    EditorViewportOverlayLayerId Layer() const override {
        return EditorViewportOverlayLayerId::CourseNavigation;
    }
    void Build(const EditorViewportOverlayFrameContext& context,
        EditorViewportOverlayCommandSink& sink) const override;

    std::string_view Label() const noexcept override { return "AI Debugger"; }
    int Order() const noexcept override { return 50; }
    bool Available() const noexcept override { return initialized_; }
    bool Capture(EditorPlaySnapshot& snapshot, EditorError* error) const override;
    bool Restore(const EditorPlaySnapshot& snapshot, EditorError* error) const override;
    bool BuildRuntimeChangeSet(const EditorPlaySnapshot& snapshot,
        EditorRuntimeChangeSet& changes, EditorError* error) const override;
    uint64_t AuthoringFingerprint() const override;

    bool Initialize(EditorAiAuthoringPolicy policy = {}, std::string* errorMessage = nullptr);
    void Shutdown();
    void Bind(EditorBehaviorTreeDocumentProvider* behaviorProvider,
        EditorEqsDocumentProvider* eqsProvider, EditorTransactionStack* transactions,
        EditorDocumentManager* documents);
    void SetActiveDocument(EditorDocumentId document);
    const EditorDocumentId& ActiveDocument() const noexcept { return activeDocument_; }
    EditorBehaviorTreeAsset* ActiveBehaviorTree();
    const EditorBehaviorTreeAsset* ActiveBehaviorTree() const;
    EditorEqsAsset* ActiveEqs();
    const EditorEqsAsset* ActiveEqs() const;
    const EditorBehaviorTreeCompileResult& BehaviorCompileResult() const noexcept {
        return behaviorCompile_;
    }
    const EditorEqsCompileResult& EqsCompileResult() const noexcept { return eqsCompile_; }

    bool AddBehaviorNode(EditorBehaviorTreeNode node, std::string& errorMessage);
    bool RemoveBehaviorNode(std::string_view nodeId, std::string& errorMessage);
    bool UpdateBehaviorNode(EditorBehaviorTreeNode node, std::string& errorMessage);
    bool AddBlackboardKey(EditorBlackboardKeyDefinition key, std::string& errorMessage);
    bool RemoveBlackboardKey(std::string_view name, std::string& errorMessage);
    bool SetEqsGenerator(EditorEqsGeneratorType generator, float radius, float spacing,
        uint32_t candidateCount, std::string smartObjectType, std::string& errorMessage);
    bool AddEqsTest(EditorEqsTestDefinition test, std::string& errorMessage);
    bool RemoveEqsTest(std::string_view id, std::string& errorMessage);
    bool UpdateEqsTest(EditorEqsTestDefinition test, std::string& errorMessage);
    bool PublishBehaviorFromCommand(const EditorDocumentId& document,
        const EditorBehaviorTreeAsset& asset, std::string& errorMessage);
    bool PublishEqsFromCommand(const EditorDocumentId& document,
        const EditorEqsAsset& asset, std::string& errorMessage);

    bool SetBreakpoint(EditorAiBreakpoint breakpoint, bool enabled, std::string* errorMessage = nullptr);
    bool HasBreakpoint(std::string_view nodeId, std::string_view agentEntityGuid = {}) const;
    void ClearBreakpoints();
    const std::vector<EditorAiBreakpoint>& Breakpoints() const noexcept { return breakpoints_; }
    void Pause();
    void Resume();
    void RequestStep();
    bool Paused() const noexcept { return paused_; }
    bool ConsumeRuntimeAdvance();
    void CaptureRuntimeFrame(const EditorProductionAiPipeline& behavior,
        const EditorProductionAiWorldPipeline& world, float deltaTime);

    void BeginRecording();
    void StopRecording();
    bool Recording() const noexcept { return recording_; }
    bool BeginReplay(std::string* errorMessage = nullptr);
    void EndReplay();
    bool Replaying() const noexcept { return replaying_; }
    bool SeekReplay(uint32_t frameIndex);
    bool StepReplay(int direction);
    uint32_t ReplayFrameIndex() const noexcept { return replayFrame_; }
    const std::vector<EditorAiSimulationFrame>& RecordingFrames() const noexcept { return frames_; }
    const EditorAiSimulationFrame* DisplayFrame() const noexcept;
    bool ExportRecording(const std::filesystem::path& path, std::string* errorMessage = nullptr);
    bool ImportRecording(const std::filesystem::path& path, std::string* errorMessage = nullptr);

    const EditorAiAuthoringPolicy& Policy() const noexcept { return policy_; }
    const EditorAiAuthoringStats& Stats() const noexcept { return stats_; }
    const std::string& LastBreakpointAgent() const noexcept { return lastBreakpointAgent_; }
    const std::string& LastBreakpointNode() const noexcept { return lastBreakpointNode_; }
    void SetMutationCallback(
        std::function<void(const EditorDocumentId&, std::string_view)> callback) {
        mutationCallback_ = std::move(callback);
    }

private:
    bool CommitBehavior(std::string_view label, EditorBehaviorTreeAsset before,
        EditorBehaviorTreeAsset after, std::string& errorMessage);
    bool CommitEqs(std::string_view label, EditorEqsAsset before,
        EditorEqsAsset after, std::string& errorMessage);
    void RefreshCompileArtifacts();
    uint64_t ComputeFrameFingerprint(const EditorAiSimulationFrame& frame) const;
    void RestorePlayState(const EditorAiPlayIsolationState& state);

    EditorAiAuthoringPolicy policy_{};
    bool initialized_ = false;
    EditorBehaviorTreeDocumentProvider* behaviorProvider_ = nullptr;
    EditorEqsDocumentProvider* eqsProvider_ = nullptr;
    EditorTransactionStack* transactions_ = nullptr;
    EditorDocumentManager* documents_ = nullptr;
    EditorDocumentId activeDocument_{};
    EditorBehaviorTreeCompileResult behaviorCompile_{};
    EditorEqsCompileResult eqsCompile_{};
    std::vector<EditorAiBreakpoint> breakpoints_;
    bool paused_ = false;
    uint32_t stepBudget_ = 0;
    bool recording_ = false;
    bool replaying_ = false;
    uint32_t replayFrame_ = 0;
    uint64_t nextFrameIndex_ = 1;
    std::vector<EditorAiSimulationFrame> frames_;
    EditorAiSimulationFrame liveFrame_{};
    std::string lastBreakpointAgent_;
    std::string lastBreakpointNode_;
    mutable EditorAiAuthoringStats stats_{};
    std::function<void(const EditorDocumentId&, std::string_view)> mutationCallback_;
};

bool EncodeEditorAiSimulationRecording(const std::vector<EditorAiSimulationFrame>& frames,
    std::string& output, std::string* errorMessage = nullptr);
bool DecodeEditorAiSimulationRecording(std::string_view input,
    std::vector<EditorAiSimulationFrame>& frames, const EditorAiAuthoringPolicy& policy,
    std::string* errorMessage = nullptr);

} // namespace editor
