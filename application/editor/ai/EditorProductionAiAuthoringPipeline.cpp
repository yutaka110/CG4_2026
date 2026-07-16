#include "EditorProductionAiAuthoringPipeline.h"

#include "../EditorTransactionStack.h"
#include "../documents/EditorDocumentManager.h"
#include "../io/EditorFileTransaction.h"
#include "../play/EditorPlaySnapshot.h"
#include "../play/EditorRuntimeChangeSet.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <memory>
#include <sstream>
#include <unordered_set>

namespace editor {
namespace {
constexpr uint64_t kFnvOffset = 1469598103934665603ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;
constexpr std::size_t kMaximumRecordingBytes = 64u * 1024u * 1024u;

void SetError(std::string* output, std::string message) {
    if (output != nullptr) *output = std::move(message);
}

uint64_t HashBytes(uint64_t hash, const void* data, std::size_t bytes) {
    const auto* values = static_cast<const unsigned char*>(data);
    for (std::size_t index = 0; index < bytes; ++index) {
        hash ^= values[index];
        hash *= kFnvPrime;
    }
    return hash;
}

template <typename T>
uint64_t HashValue(uint64_t hash, const T& value) {
    return HashBytes(hash, &value, sizeof(value));
}

uint64_t HashText(uint64_t hash, std::string_view value) {
    return HashBytes(hash, value.data(), value.size());
}

bool ParseBlackboardType(std::string_view text, EditorBlackboardValueType& type) {
    for (EditorBlackboardValueType candidate : {EditorBlackboardValueType::Bool,
             EditorBlackboardValueType::Int, EditorBlackboardValueType::Float,
             EditorBlackboardValueType::Vector3, EditorBlackboardValueType::Entity,
             EditorBlackboardValueType::String}) {
        if (text == ToString(candidate)) { type = candidate; return true; }
    }
    return false;
}

bool ParseBehaviorStatus(std::string_view text, EditorBehaviorStatus& status) {
    for (EditorBehaviorStatus candidate : {EditorBehaviorStatus::Succeeded,
             EditorBehaviorStatus::Failed, EditorBehaviorStatus::Running,
             EditorBehaviorStatus::BudgetExceeded, EditorBehaviorStatus::InvalidProgram}) {
        if (text == ToString(candidate)) { status = candidate; return true; }
    }
    return false;
}

class BehaviorAuthoringUndoCommand final : public IEditorUndoCommand {
public:
    BehaviorAuthoringUndoCommand(EditorDocumentId document, EditorBehaviorTreeAsset before,
        EditorBehaviorTreeAsset after)
        : document_(std::move(document)), before_(std::move(before)), after_(std::move(after)) {}
    EditorUndoResult Apply(EditorTransactionApplyMode mode, EditorExecutionContext& context) const override {
        auto* service = dynamic_cast<EditorProductionAiAuthoringPipeline*>(context.Find(
            EditorProductionAiAuthoringPipeline::kServiceId));
        if (service == nullptr) return EditorUndoResult::Failure(EditorErrorCode::MissingService,
            "AI authoring execution service is unavailable.");
        std::string error;
        if (!service->PublishBehaviorFromCommand(document_,
                mode == EditorTransactionApplyMode::Undo ? before_ : after_, error))
            return EditorUndoResult::Failure(EditorErrorCode::ApplyFailed, std::move(error));
        return EditorUndoResult::Success();
    }
    std::size_t EstimatedBytes() const noexcept override {
        return sizeof(*this) + (before_.nodes.size() + after_.nodes.size()) * sizeof(EditorBehaviorTreeNode) +
            (before_.blackboard.size() + after_.blackboard.size()) * sizeof(EditorBlackboardKeyDefinition);
    }
    std::string_view DomainId() const noexcept override { return "ai-authoring"; }
    std::string_view TypeId() const noexcept override { return "ai-authoring.behavior-snapshot"; }
private:
    EditorDocumentId document_;
    EditorBehaviorTreeAsset before_;
    EditorBehaviorTreeAsset after_;
};

class EqsAuthoringUndoCommand final : public IEditorUndoCommand {
public:
    EqsAuthoringUndoCommand(EditorDocumentId document, EditorEqsAsset before, EditorEqsAsset after)
        : document_(std::move(document)), before_(std::move(before)), after_(std::move(after)) {}
    EditorUndoResult Apply(EditorTransactionApplyMode mode, EditorExecutionContext& context) const override {
        auto* service = dynamic_cast<EditorProductionAiAuthoringPipeline*>(context.Find(
            EditorProductionAiAuthoringPipeline::kServiceId));
        if (service == nullptr) return EditorUndoResult::Failure(EditorErrorCode::MissingService,
            "AI authoring execution service is unavailable.");
        std::string error;
        if (!service->PublishEqsFromCommand(document_,
                mode == EditorTransactionApplyMode::Undo ? before_ : after_, error))
            return EditorUndoResult::Failure(EditorErrorCode::ApplyFailed, std::move(error));
        return EditorUndoResult::Success();
    }
    std::size_t EstimatedBytes() const noexcept override {
        return sizeof(*this) + (before_.tests.size() + after_.tests.size()) * sizeof(EditorEqsTestDefinition);
    }
    std::string_view DomainId() const noexcept override { return "ai-authoring"; }
    std::string_view TypeId() const noexcept override { return "ai-authoring.eqs-snapshot"; }
private:
    EditorDocumentId document_;
    EditorEqsAsset before_;
    EditorEqsAsset after_;
};

bool SameBreakpoint(const EditorAiBreakpoint& lhs, const EditorAiBreakpoint& rhs) {
    return lhs.nodeId == rhs.nodeId && lhs.agentEntityGuid == rhs.agentEntityGuid;
}

bool IsFiniteFrame(const EditorAiSimulationFrame& frame) {
    if (!std::isfinite(frame.deltaTime) || frame.deltaTime < 0.0f) return false;
    for (const auto& agent : frame.agents) {
        for (const auto& point : agent.lastPath)
            if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) return false;
    }
    return true;
}
} // namespace

bool EditorAiBreakpoint::Matches(const EditorAiAgentDebugSnapshot& snapshot) const {
    return !nodeId.empty() && (agentEntityGuid.empty() || agentEntityGuid == snapshot.entityGuid) &&
        std::find(snapshot.activeNodeTrace.begin(), snapshot.activeNodeTrace.end(), nodeId) !=
            snapshot.activeNodeTrace.end();
}

bool EditorProductionAiAuthoringPipeline::Initialize(
    EditorAiAuthoringPolicy policy, std::string* errorMessage) {
    Shutdown();
    policy.maximumBreakpoints = (std::max)(1u, policy.maximumBreakpoints);
    policy.maximumRecordedFrames = (std::max)(1u, policy.maximumRecordedFrames);
    policy.maximumAgentsPerFrame = (std::max)(1u, policy.maximumAgentsPerFrame);
    policy.maximumCrowdAgentsPerFrame = (std::max)(1u, policy.maximumCrowdAgentsPerFrame);
    policy.maximumSmartObjectSlotsPerFrame = (std::max)(1u, policy.maximumSmartObjectSlotsPerFrame);
    policy.maximumOverlayCommands = (std::max)(1u, policy.maximumOverlayCommands);
    policy_ = policy;
    initialized_ = true;
    SetError(errorMessage, {});
    return true;
}

void EditorProductionAiAuthoringPipeline::Shutdown() {
    initialized_ = false;
    activeDocument_ = {};
    behaviorCompile_ = {};
    eqsCompile_ = {};
    breakpoints_.clear();
    frames_.clear();
    liveFrame_ = {};
    paused_ = false;
    stepBudget_ = 0;
    recording_ = false;
    replaying_ = false;
    replayFrame_ = 0;
    nextFrameIndex_ = 1;
    lastBreakpointAgent_.clear();
    lastBreakpointNode_.clear();
    stats_ = {};
}

void EditorProductionAiAuthoringPipeline::Bind(
    EditorBehaviorTreeDocumentProvider* behaviorProvider, EditorEqsDocumentProvider* eqsProvider,
    EditorTransactionStack* transactions, EditorDocumentManager* documents) {
    behaviorProvider_ = behaviorProvider;
    eqsProvider_ = eqsProvider;
    transactions_ = transactions;
    documents_ = documents;
}

void EditorProductionAiAuthoringPipeline::SetActiveDocument(EditorDocumentId document) {
    activeDocument_ = std::move(document);
    RefreshCompileArtifacts();
}

EditorBehaviorTreeAsset* EditorProductionAiAuthoringPipeline::ActiveBehaviorTree() {
    return behaviorProvider_ != nullptr && activeDocument_.type == EditorDocumentTypes::BehaviorTree
        ? behaviorProvider_->Asset(activeDocument_) : nullptr;
}
const EditorBehaviorTreeAsset* EditorProductionAiAuthoringPipeline::ActiveBehaviorTree() const {
    return const_cast<EditorProductionAiAuthoringPipeline*>(this)->ActiveBehaviorTree();
}
EditorEqsAsset* EditorProductionAiAuthoringPipeline::ActiveEqs() {
    return eqsProvider_ != nullptr && activeDocument_.type == EditorDocumentTypes::EnvironmentQuery
        ? eqsProvider_->Asset(activeDocument_) : nullptr;
}
const EditorEqsAsset* EditorProductionAiAuthoringPipeline::ActiveEqs() const {
    return const_cast<EditorProductionAiAuthoringPipeline*>(this)->ActiveEqs();
}

bool EditorProductionAiAuthoringPipeline::AddBehaviorNode(
    EditorBehaviorTreeNode node, std::string& errorMessage) {
    EditorBehaviorTreeAsset* asset = ActiveBehaviorTree();
    if (asset == nullptr || node.id.empty() || asset->nodes.size() >= kEditorBehaviorTreeMaximumNodes ||
        std::any_of(asset->nodes.begin(), asset->nodes.end(), [&](const auto& value) { return value.id == node.id; })) {
        errorMessage = "Behavior node identity, capacity, or active document is invalid.";
        return false;
    }
    auto before = *asset;
    auto after = before;
    after.nodes.push_back(std::move(node));
    return CommitBehavior("Add Behavior Node", std::move(before), std::move(after), errorMessage);
}

bool EditorProductionAiAuthoringPipeline::RemoveBehaviorNode(
    std::string_view nodeId, std::string& errorMessage) {
    EditorBehaviorTreeAsset* asset = ActiveBehaviorTree();
    if (asset == nullptr || nodeId == "root") {
        errorMessage = "Behavior node cannot be removed.";
        return false;
    }
    auto before = *asset;
    auto after = before;
    const std::size_t original = after.nodes.size();
    after.nodes.erase(std::remove_if(after.nodes.begin(), after.nodes.end(),
        [&](const auto& node) { return node.id == nodeId; }), after.nodes.end());
    if (after.nodes.size() == original) { errorMessage = "Behavior node was not found."; return false; }
    return CommitBehavior("Remove Behavior Node", std::move(before), std::move(after), errorMessage);
}

bool EditorProductionAiAuthoringPipeline::UpdateBehaviorNode(
    EditorBehaviorTreeNode node, std::string& errorMessage) {
    EditorBehaviorTreeAsset* asset = ActiveBehaviorTree();
    if (asset == nullptr || node.id.empty()) { errorMessage = "Behavior node is unavailable."; return false; }
    auto before = *asset;
    auto after = before;
    auto found = std::find_if(after.nodes.begin(), after.nodes.end(),
        [&](const auto& value) { return value.id == node.id; });
    if (found == after.nodes.end()) { errorMessage = "Behavior node was not found."; return false; }
    *found = std::move(node);
    return CommitBehavior("Edit Behavior Node", std::move(before), std::move(after), errorMessage);
}

bool EditorProductionAiAuthoringPipeline::AddBlackboardKey(
    EditorBlackboardKeyDefinition key, std::string& errorMessage) {
    EditorBehaviorTreeAsset* asset = ActiveBehaviorTree();
    if (asset == nullptr || key.name.empty() ||
        asset->blackboard.size() >= kEditorBehaviorTreeMaximumBlackboardKeys ||
        std::any_of(asset->blackboard.begin(), asset->blackboard.end(),
            [&](const auto& value) { return value.name == key.name; })) {
        errorMessage = "Blackboard key identity, capacity, or active document is invalid.";
        return false;
    }
    auto before = *asset;
    auto after = before;
    after.blackboard.push_back(std::move(key));
    return CommitBehavior("Add Blackboard Key", std::move(before), std::move(after), errorMessage);
}

bool EditorProductionAiAuthoringPipeline::RemoveBlackboardKey(
    std::string_view name, std::string& errorMessage) {
    EditorBehaviorTreeAsset* asset = ActiveBehaviorTree();
    if (asset == nullptr) { errorMessage = "Behavior Tree document is unavailable."; return false; }
    auto before = *asset;
    auto after = before;
    const std::size_t original = after.blackboard.size();
    after.blackboard.erase(std::remove_if(after.blackboard.begin(), after.blackboard.end(),
        [&](const auto& key) { return key.name == name; }), after.blackboard.end());
    if (original == after.blackboard.size()) { errorMessage = "Blackboard key was not found."; return false; }
    return CommitBehavior("Remove Blackboard Key", std::move(before), std::move(after), errorMessage);
}

bool EditorProductionAiAuthoringPipeline::SetEqsGenerator(EditorEqsGeneratorType generator,
    float radius, float spacing, uint32_t candidateCount, std::string smartObjectType,
    std::string& errorMessage) {
    EditorEqsAsset* asset = ActiveEqs();
    if (asset == nullptr) { errorMessage = "Environment Query document is unavailable."; return false; }
    auto before = *asset;
    auto after = before;
    after.generator = generator;
    after.radius = radius;
    after.spacing = spacing;
    after.candidateCount = candidateCount;
    after.smartObjectType = std::move(smartObjectType);
    return CommitEqs("Edit EQS Generator", std::move(before), std::move(after), errorMessage);
}

bool EditorProductionAiAuthoringPipeline::AddEqsTest(
    EditorEqsTestDefinition test, std::string& errorMessage) {
    EditorEqsAsset* asset = ActiveEqs();
    if (asset == nullptr || test.id.empty() || asset->tests.size() >= kEditorEqsMaximumTests ||
        std::any_of(asset->tests.begin(), asset->tests.end(), [&](const auto& value) { return value.id == test.id; })) {
        errorMessage = "EQS test identity, capacity, or active document is invalid.";
        return false;
    }
    auto before = *asset;
    auto after = before;
    after.tests.push_back(std::move(test));
    return CommitEqs("Add EQS Test", std::move(before), std::move(after), errorMessage);
}

bool EditorProductionAiAuthoringPipeline::RemoveEqsTest(
    std::string_view id, std::string& errorMessage) {
    EditorEqsAsset* asset = ActiveEqs();
    if (asset == nullptr || asset->tests.size() <= 1) { errorMessage = "EQS test cannot be removed."; return false; }
    auto before = *asset;
    auto after = before;
    const std::size_t original = after.tests.size();
    after.tests.erase(std::remove_if(after.tests.begin(), after.tests.end(),
        [&](const auto& test) { return test.id == id; }), after.tests.end());
    if (original == after.tests.size()) { errorMessage = "EQS test was not found."; return false; }
    return CommitEqs("Remove EQS Test", std::move(before), std::move(after), errorMessage);
}

bool EditorProductionAiAuthoringPipeline::UpdateEqsTest(
    EditorEqsTestDefinition test, std::string& errorMessage) {
    EditorEqsAsset* asset = ActiveEqs();
    if (asset == nullptr || test.id.empty()) { errorMessage = "EQS test is unavailable."; return false; }
    auto before = *asset;
    auto after = before;
    auto found = std::find_if(after.tests.begin(), after.tests.end(),
        [&](const auto& value) { return value.id == test.id; });
    if (found == after.tests.end()) { errorMessage = "EQS test was not found."; return false; }
    *found = std::move(test);
    return CommitEqs("Edit EQS Test", std::move(before), std::move(after), errorMessage);
}

bool EditorProductionAiAuthoringPipeline::CommitBehavior(std::string_view label,
    EditorBehaviorTreeAsset before, EditorBehaviorTreeAsset after, std::string& errorMessage) {
    const EditorBehaviorTreeCompileResult compiled = CompileEditorBehaviorTree(after);
    if (!compiled.succeeded) {
        ++stats_.compileFailures;
        errorMessage = compiled.diagnostics.empty() ? "Behavior Tree compile failed." : compiled.diagnostics.front().message;
        return false;
    }
    if (behaviorProvider_ == nullptr || transactions_ == nullptr || !activeDocument_.IsValid() ||
        !behaviorProvider_->Publish(activeDocument_, after)) {
        errorMessage = "Behavior Tree mutation services are unavailable.";
        return false;
    }
    EditorObjectHandle target{EditorDomainId::AiAuthoringNode, activeDocument_.assetGuid, 0, 0, after.name};
    EditorError error;
    if (!transactions_->PushCommand(std::string(label), std::move(target),
            std::make_shared<BehaviorAuthoringUndoCommand>(activeDocument_, before, after), &error)) {
        behaviorProvider_->Publish(activeDocument_, std::move(before));
        errorMessage = error.message;
        return false;
    }
    behaviorCompile_ = compiled;
    ++stats_.behaviorMutations;
    if (documents_ != nullptr) documents_->MarkDirty(activeDocument_, label);
    if (mutationCallback_) mutationCallback_(activeDocument_, label);
    return true;
}

bool EditorProductionAiAuthoringPipeline::CommitEqs(std::string_view label,
    EditorEqsAsset before, EditorEqsAsset after, std::string& errorMessage) {
    const EditorEqsCompileResult compiled = CompileEditorEqs(after);
    if (!compiled.succeeded) {
        ++stats_.compileFailures;
        errorMessage = compiled.diagnostics.empty() ? "Environment Query compile failed." : compiled.diagnostics.front().message;
        return false;
    }
    if (eqsProvider_ == nullptr || transactions_ == nullptr || !activeDocument_.IsValid() ||
        !eqsProvider_->Publish(activeDocument_, after)) {
        errorMessage = "Environment Query mutation services are unavailable.";
        return false;
    }
    EditorObjectHandle target{EditorDomainId::AiAuthoringNode, activeDocument_.assetGuid, 0, 0, after.name};
    EditorError error;
    if (!transactions_->PushCommand(std::string(label), std::move(target),
            std::make_shared<EqsAuthoringUndoCommand>(activeDocument_, before, after), &error)) {
        eqsProvider_->Publish(activeDocument_, std::move(before));
        errorMessage = error.message;
        return false;
    }
    eqsCompile_ = compiled;
    ++stats_.eqsMutations;
    if (documents_ != nullptr) documents_->MarkDirty(activeDocument_, label);
    if (mutationCallback_) mutationCallback_(activeDocument_, label);
    return true;
}

bool EditorProductionAiAuthoringPipeline::PublishBehaviorFromCommand(
    const EditorDocumentId& document, const EditorBehaviorTreeAsset& asset, std::string& errorMessage) {
    if (behaviorProvider_ == nullptr || !behaviorProvider_->Publish(document, asset)) {
        errorMessage = "Behavior Tree transaction snapshot could not be published.";
        return false;
    }
    if (document == activeDocument_) RefreshCompileArtifacts();
    if (documents_ != nullptr) documents_->MarkDirty(document, "AI Authoring Undo/Redo");
    if (mutationCallback_) mutationCallback_(document, "AI Authoring Undo/Redo");
    return true;
}

bool EditorProductionAiAuthoringPipeline::PublishEqsFromCommand(
    const EditorDocumentId& document, const EditorEqsAsset& asset, std::string& errorMessage) {
    if (eqsProvider_ == nullptr || !eqsProvider_->Publish(document, asset)) {
        errorMessage = "Environment Query transaction snapshot could not be published.";
        return false;
    }
    if (document == activeDocument_) RefreshCompileArtifacts();
    if (documents_ != nullptr) documents_->MarkDirty(document, "AI Authoring Undo/Redo");
    if (mutationCallback_) mutationCallback_(document, "AI Authoring Undo/Redo");
    return true;
}

void EditorProductionAiAuthoringPipeline::RefreshCompileArtifacts() {
    behaviorCompile_ = {};
    eqsCompile_ = {};
    if (const auto* asset = ActiveBehaviorTree()) behaviorCompile_ = CompileEditorBehaviorTree(*asset);
    if (const auto* asset = ActiveEqs()) eqsCompile_ = CompileEditorEqs(*asset);
}

bool EditorProductionAiAuthoringPipeline::SetBreakpoint(
    EditorAiBreakpoint breakpoint, bool enabled, std::string* errorMessage) {
    if (breakpoint.nodeId.empty()) { SetError(errorMessage, "Breakpoint node ID is empty."); return false; }
    const auto found = std::find_if(breakpoints_.begin(), breakpoints_.end(),
        [&](const auto& value) { return SameBreakpoint(value, breakpoint); });
    if (enabled) {
        if (found != breakpoints_.end()) return true;
        if (breakpoints_.size() >= policy_.maximumBreakpoints) {
            SetError(errorMessage, "Breakpoint capacity is exhausted.");
            return false;
        }
        breakpoints_.push_back(std::move(breakpoint));
        std::sort(breakpoints_.begin(), breakpoints_.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.nodeId != rhs.nodeId ? lhs.nodeId < rhs.nodeId : lhs.agentEntityGuid < rhs.agentEntityGuid;
        });
    } else if (found != breakpoints_.end()) {
        breakpoints_.erase(found);
    }
    SetError(errorMessage, {});
    return true;
}

bool EditorProductionAiAuthoringPipeline::HasBreakpoint(
    std::string_view nodeId, std::string_view agentEntityGuid) const {
    return std::any_of(breakpoints_.begin(), breakpoints_.end(), [&](const auto& value) {
        return value.nodeId == nodeId && value.agentEntityGuid == agentEntityGuid;
    });
}
void EditorProductionAiAuthoringPipeline::ClearBreakpoints() { breakpoints_.clear(); }
void EditorProductionAiAuthoringPipeline::Pause() { paused_ = true; stepBudget_ = 0; }
void EditorProductionAiAuthoringPipeline::Resume() {
    paused_ = false; stepBudget_ = 0; replaying_ = false;
    lastBreakpointAgent_.clear(); lastBreakpointNode_.clear();
}
void EditorProductionAiAuthoringPipeline::RequestStep() { paused_ = true; stepBudget_ = 1; }

bool EditorProductionAiAuthoringPipeline::ConsumeRuntimeAdvance() {
    if (!initialized_ || replaying_) return false;
    if (!paused_) return true;
    if (stepBudget_ == 0) return false;
    --stepBudget_;
    return true;
}

void EditorProductionAiAuthoringPipeline::CaptureRuntimeFrame(
    const EditorProductionAiPipeline& behavior,
    const EditorProductionAiWorldPipeline& world, float deltaTime) {
    if (!initialized_ || replaying_) return;
    liveFrame_ = {};
    liveFrame_.frameIndex = nextFrameIndex_++;
    liveFrame_.behaviorGeneration = behavior.Stats().tickGeneration;
    liveFrame_.worldGeneration = world.Stats().worldGeneration;
    liveFrame_.deltaTime = (std::max)(0.0f, deltaTime);
    const auto& agents = behavior.DebugSnapshots();
    liveFrame_.agents.assign(agents.begin(), agents.begin() +
        (std::min<std::size_t>)(agents.size(), policy_.maximumAgentsPerFrame));
    const auto& crowd = world.CrowdSnapshots();
    liveFrame_.crowd.assign(crowd.begin(), crowd.begin() +
        (std::min<std::size_t>)(crowd.size(), policy_.maximumCrowdAgentsPerFrame));
    const auto& slots = world.SmartObjectSlots();
    liveFrame_.smartObjects.assign(slots.begin(), slots.begin() +
        (std::min<std::size_t>)(slots.size(), policy_.maximumSmartObjectSlotsPerFrame));
    liveFrame_.fingerprint = ComputeFrameFingerprint(liveFrame_);
    ++stats_.liveFrames;

    for (const auto& snapshot : liveFrame_.agents) {
        const auto hit = std::find_if(breakpoints_.begin(), breakpoints_.end(),
            [&](const auto& breakpoint) { return breakpoint.Matches(snapshot); });
        if (hit != breakpoints_.end()) {
            paused_ = true;
            stepBudget_ = 0;
            lastBreakpointAgent_ = snapshot.entityGuid;
            lastBreakpointNode_ = hit->nodeId;
            ++stats_.breakpointHits;
            break;
        }
    }
    if (recording_) {
        if (frames_.size() >= policy_.maximumRecordedFrames) {
            frames_.erase(frames_.begin());
            ++stats_.droppedRecordingFrames;
        }
        frames_.push_back(liveFrame_);
        stats_.recordedFrames = static_cast<uint32_t>(frames_.size());
    }
}

void EditorProductionAiAuthoringPipeline::BeginRecording() {
    frames_.clear();
    recording_ = true;
    replaying_ = false;
    replayFrame_ = 0;
    stats_.recordedFrames = 0;
    stats_.droppedRecordingFrames = 0;
}
void EditorProductionAiAuthoringPipeline::StopRecording() { recording_ = false; }
bool EditorProductionAiAuthoringPipeline::BeginReplay(std::string* errorMessage) {
    if (frames_.empty()) { SetError(errorMessage, "AI simulation recording is empty."); return false; }
    recording_ = false;
    replaying_ = true;
    paused_ = true;
    stepBudget_ = 0;
    replayFrame_ = 0;
    SetError(errorMessage, {});
    return true;
}
void EditorProductionAiAuthoringPipeline::EndReplay() { replaying_ = false; replayFrame_ = 0; }
bool EditorProductionAiAuthoringPipeline::SeekReplay(uint32_t frameIndex) {
    if (!replaying_ || frameIndex >= frames_.size()) return false;
    replayFrame_ = frameIndex;
    return true;
}
bool EditorProductionAiAuthoringPipeline::StepReplay(int direction) {
    if (!replaying_ || frames_.empty() || direction == 0) return false;
    const int64_t next = static_cast<int64_t>(replayFrame_) + (direction > 0 ? 1 : -1);
    if (next < 0 || next >= static_cast<int64_t>(frames_.size())) return false;
    replayFrame_ = static_cast<uint32_t>(next);
    ++stats_.replaySteps;
    return true;
}
const EditorAiSimulationFrame* EditorProductionAiAuthoringPipeline::DisplayFrame() const noexcept {
    if (replaying_) return replayFrame_ < frames_.size() ? &frames_[replayFrame_] : nullptr;
    return liveFrame_.frameIndex != 0 ? &liveFrame_ : nullptr;
}

uint64_t EditorProductionAiAuthoringPipeline::ComputeFrameFingerprint(
    const EditorAiSimulationFrame& frame) const {
    uint64_t hash = kFnvOffset;
    hash = HashValue(hash, frame.frameIndex);
    hash = HashValue(hash, frame.behaviorGeneration);
    hash = HashValue(hash, frame.worldGeneration);
    hash = HashValue(hash, frame.deltaTime);
    for (const auto& agent : frame.agents) {
        hash = HashText(hash, agent.entityGuid);
        hash = HashText(hash, agent.behaviorAssetGuid);
        hash = HashValue(hash, agent.status);
        hash = HashValue(hash, agent.tickGeneration);
        for (const auto& node : agent.activeNodeTrace) hash = HashText(hash, node);
        for (const auto& value : agent.blackboard) {
            hash = HashText(hash, value.name);
            hash = HashValue(hash, value.defaultValue.type);
            hash = HashValue(hash, value.defaultValue.boolValue);
            hash = HashValue(hash, value.defaultValue.intValue);
            hash = HashValue(hash, value.defaultValue.floatValue);
            hash = HashValue(hash, value.defaultValue.vectorValue);
            hash = HashText(hash, value.defaultValue.textValue);
        }
        for (const auto& point : agent.lastPath) hash = HashValue(hash, point);
    }
    for (const auto& agent : frame.crowd) {
        hash = HashText(hash, agent.entityGuid);
        hash = HashValue(hash, agent.position);
        hash = HashValue(hash, agent.preferredVelocity);
        hash = HashValue(hash, agent.steeringVelocity);
    }
    for (const auto& slot : frame.smartObjects) {
        hash = HashText(hash, slot.entityGuid);
        hash = HashText(hash, slot.slotId);
        hash = HashText(hash, slot.type);
        hash = HashValue(hash, slot.position);
        hash = HashText(hash, slot.reservedByEntityGuid);
        hash = HashValue(hash, slot.reservationToken);
    }
    return hash;
}

bool EditorProductionAiAuthoringPipeline::ExportRecording(
    const std::filesystem::path& path, std::string* errorMessage) {
    std::string encoded;
    if (!EncodeEditorAiSimulationRecording(frames_, encoded, errorMessage)) return false;
    EditorFileTransaction transaction(std::filesystem::current_path());
    if (!transaction.StageTextWrite(path, std::move(encoded), {}, errorMessage) ||
        !transaction.Execute(nullptr, errorMessage)) return false;
    ++stats_.exportedRecordings;
    return true;
}

bool EditorProductionAiAuthoringPipeline::ImportRecording(
    const std::filesystem::path& path, std::string* errorMessage) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) { SetError(errorMessage, "AI simulation recording could not be opened."); return false; }
    std::string encoded{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    std::vector<EditorAiSimulationFrame> decoded;
    if (!DecodeEditorAiSimulationRecording(encoded, decoded, policy_, errorMessage)) return false;
    for (const EditorAiSimulationFrame& frame : decoded) {
        if (ComputeFrameFingerprint(frame) != frame.fingerprint) {
            SetError(errorMessage, "AI simulation recording fingerprint verification failed.");
            return false;
        }
    }
    frames_ = std::move(decoded);
    recording_ = false;
    replaying_ = false;
    replayFrame_ = 0;
    stats_.recordedFrames = static_cast<uint32_t>(frames_.size());
    ++stats_.importedRecordings;
    return true;
}

void EditorProductionAiAuthoringPipeline::Build(
    const EditorViewportOverlayFrameContext& context,
    EditorViewportOverlayCommandSink& sink) const {
    const EditorAiSimulationFrame* frame = DisplayFrame();
    if (frame == nullptr || context.coordinates == nullptr) return;
    uint32_t submitted = 0;
    const auto permit = [&]() {
        if (submitted >= policy_.maximumOverlayCommands) {
            ++stats_.overlayBudgetRejected;
            return false;
        }
        ++submitted;
        return true;
    };
    EditorViewportOverlayItemOptions options{};
    options.priority = 180;
    options.background = true;
    for (const auto& agent : frame->agents) {
        for (std::size_t index = 1; index < agent.lastPath.size(); ++index) {
            const auto a = context.coordinates->ProjectWorld(agent.lastPath[index - 1]);
            const auto b = context.coordinates->ProjectWorld(agent.lastPath[index]);
            if (a.valid && b.valid && !a.behind && !b.behind && permit())
                sink.Line(a.render.x, a.render.y, b.render.x, b.render.y, 0xff58d7ffu, 2.0f, options);
        }
        for (const auto& perceived : agent.perceived) {
            const auto point = context.coordinates->ProjectWorld(perceived.position);
            if (point.valid && !point.behind && permit())
                sink.Circle(point.render.x, point.render.y, 7.0f,
                    perceived.seen ? 0xff55ff7fu : 0xffffc85au, 2.0f, options);
        }
    }
    for (const auto& crowd : frame->crowd) {
        const Vector3 end{crowd.position.x + crowd.steeringVelocity.x,
            crowd.position.y + crowd.steeringVelocity.y,
            crowd.position.z + crowd.steeringVelocity.z};
        const auto a = context.coordinates->ProjectWorld(crowd.position);
        const auto b = context.coordinates->ProjectWorld(end);
        if (a.valid && b.valid && !a.behind && !b.behind && permit())
            sink.Line(a.render.x, a.render.y, b.render.x, b.render.y,
                crowd.constrained ? 0xffff7666u : 0xff7dffafu, 2.0f, options);
    }
    for (const auto& slot : frame->smartObjects) {
        const auto point = context.coordinates->ProjectWorld(slot.position);
        if (!point.valid || point.behind || !permit()) continue;
        sink.Icon(point.render.x, point.render.y, 6.0f,
            slot.reservedByEntityGuid.empty() ? 0xff76d9ffu : 0xffff6b76u, options);
        if (permit()) sink.Label(point.render.x + 8.0f, point.render.y,
            slot.type + ":" + slot.slotId, 0xffd7f2ffu, options);
    }
    stats_.overlayCommands = submitted;
}

bool EditorProductionAiAuthoringPipeline::Capture(
    EditorPlaySnapshot& snapshot, EditorError* error) const {
    if (!initialized_) {
        SetEditorError(error, EditorErrorCode::NotAvailable, "AI Debugger is unavailable for Play isolation.");
        return false;
    }
    return snapshot.Store(std::string(kPlayIsolationId),
        EditorAiPlayIsolationState{paused_, recording_, replaying_, replayFrame_, breakpoints_,
            nextFrameIndex_, frames_, liveFrame_},
        AuthoringFingerprint(), error);
}

bool EditorProductionAiAuthoringPipeline::Restore(
    const EditorPlaySnapshot& snapshot, EditorError* error) const {
    const auto* state = snapshot.Read<EditorAiPlayIsolationState>(kPlayIsolationId, error);
    if (state == nullptr) return false;
    const_cast<EditorProductionAiAuthoringPipeline*>(this)->RestorePlayState(*state);
    ClearEditorError(error);
    return true;
}

bool EditorProductionAiAuthoringPipeline::BuildRuntimeChangeSet(
    const EditorPlaySnapshot& snapshot, EditorRuntimeChangeSet& changes, EditorError* error) const {
    const EditorPlaySnapshotEntry* entry = snapshot.Find(kPlayIsolationId);
    if (entry == nullptr) {
        SetEditorError(error, EditorErrorCode::NotAvailable, "AI Debugger Play snapshot is unavailable.");
        return false;
    }
    changes.Add({std::string(kPlayIsolationId), "ai.debugger-state", "AI Debugger runtime state",
        entry->authoringFingerprint, AuthoringFingerprint(), false});
    ClearEditorError(error);
    return true;
}

uint64_t EditorProductionAiAuthoringPipeline::AuthoringFingerprint() const {
    uint64_t hash = kFnvOffset;
    hash = HashValue(hash, paused_);
    hash = HashValue(hash, recording_);
    hash = HashValue(hash, replaying_);
    hash = HashValue(hash, replayFrame_);
    for (const auto& breakpoint : breakpoints_) {
        hash = HashText(hash, breakpoint.nodeId);
        hash = HashText(hash, breakpoint.agentEntityGuid);
    }
    hash = HashValue(hash, nextFrameIndex_);
    hash = HashValue(hash, liveFrame_.fingerprint);
    for (const auto& frame : frames_) hash = HashValue(hash, frame.fingerprint);
    return hash;
}

void EditorProductionAiAuthoringPipeline::RestorePlayState(
    const EditorAiPlayIsolationState& state) {
    paused_ = state.paused;
    recording_ = state.recording;
    replaying_ = state.replaying && !frames_.empty();
    replayFrame_ = frames_.empty() ? 0u : (std::min<uint32_t>)(state.replayFrame,
        static_cast<uint32_t>(frames_.size() - 1));
    breakpoints_ = state.breakpoints;
    if (breakpoints_.size() > policy_.maximumBreakpoints)
        breakpoints_.resize(policy_.maximumBreakpoints);
    nextFrameIndex_ = state.nextFrameIndex;
    frames_ = state.frames;
    if (frames_.size() > policy_.maximumRecordedFrames)
        frames_.erase(frames_.begin(), frames_.end() - policy_.maximumRecordedFrames);
    liveFrame_ = state.liveFrame;
    stats_.recordedFrames = static_cast<uint32_t>(frames_.size());
    stepBudget_ = 0;
}

bool EncodeEditorAiSimulationRecording(const std::vector<EditorAiSimulationFrame>& frames,
    std::string& output, std::string* errorMessage) {
    std::ostringstream stream;
    stream << "AI_SIM_RECORDING " << kEditorAiSimulationRecordingSchemaVersion << ' '
           << frames.size() << '\n';
    stream << std::setprecision(17);
    for (const auto& frame : frames) {
        if (!IsFiniteFrame(frame)) { SetError(errorMessage, "AI simulation frame contains non-finite data."); return false; }
        stream << "FRAME " << frame.frameIndex << ' ' << frame.behaviorGeneration << ' '
               << frame.worldGeneration << ' ' << frame.deltaTime << ' ' << frame.fingerprint << ' '
               << frame.agents.size() << ' ' << frame.crowd.size() << ' '
               << frame.smartObjects.size() << '\n';
        for (const auto& agent : frame.agents) {
            stream << "AGENT " << std::quoted(agent.entityGuid) << ' '
                   << std::quoted(agent.behaviorAssetGuid) << ' ' << ToString(agent.status) << ' '
                   << agent.tickGeneration << ' ' << agent.perceptionGeneration << ' '
                   << agent.executedNodes << ' ' << agent.activeNodeTrace.size() << ' '
                   << agent.blackboard.size() << ' ' << agent.perceived.size() << ' '
                   << agent.lastPath.size() << '\n';
            for (const auto& node : agent.activeNodeTrace) stream << "TRACE " << std::quoted(node) << '\n';
            for (const auto& key : agent.blackboard) {
                const auto& value = key.defaultValue;
                stream << "BLACKBOARD " << std::quoted(key.name) << ' ' << ToString(value.type) << ' '
                       << (value.boolValue ? 1 : 0) << ' ' << value.intValue << ' ' << value.floatValue << ' '
                       << value.vectorValue.x << ' ' << value.vectorValue.y << ' ' << value.vectorValue.z << ' '
                       << std::quoted(value.textValue) << '\n';
            }
            for (const auto& perceived : agent.perceived)
                stream << "PERCEIVED " << std::quoted(perceived.entityGuid) << ' '
                       << perceived.position.x << ' ' << perceived.position.y << ' ' << perceived.position.z << ' '
                       << perceived.strength << ' ' << (perceived.seen ? 1 : 0) << ' '
                       << (perceived.heard ? 1 : 0) << '\n';
            for (const auto& point : agent.lastPath)
                stream << "PATH " << point.x << ' ' << point.y << ' ' << point.z << '\n';
        }
        for (const auto& crowd : frame.crowd)
            stream << "CROWD " << std::quoted(crowd.entityGuid) << ' '
                   << crowd.position.x << ' ' << crowd.position.y << ' ' << crowd.position.z << ' '
                   << crowd.preferredVelocity.x << ' ' << crowd.preferredVelocity.y << ' '
                   << crowd.preferredVelocity.z << ' ' << crowd.steeringVelocity.x << ' '
                   << crowd.steeringVelocity.y << ' ' << crowd.steeringVelocity.z << ' '
                   << crowd.radius << ' ' << crowd.maximumSpeed << ' ' << crowd.consideredNeighbors << ' '
                   << (crowd.constrained ? 1 : 0) << '\n';
        for (const auto& slot : frame.smartObjects)
            stream << "SLOT " << std::quoted(slot.entityGuid) << ' ' << std::quoted(slot.slotId) << ' '
                   << std::quoted(slot.type) << ' ' << slot.position.x << ' ' << slot.position.y << ' '
                   << slot.position.z << ' ' << slot.interactionRadius << ' ' << slot.priority << ' '
                   << slot.leaseSeconds << ' ' << std::quoted(slot.reservedByEntityGuid) << ' '
                   << slot.reservationToken << ' ' << slot.leaseExpirySeconds << '\n';
        stream << "END_FRAME\n";
    }
    stream << "END\n";
    output = stream.str();
    if (output.size() > kMaximumRecordingBytes) {
        SetError(errorMessage, "AI simulation recording exceeds 64 MiB.");
        return false;
    }
    return true;
}

bool DecodeEditorAiSimulationRecording(std::string_view input,
    std::vector<EditorAiSimulationFrame>& frames, const EditorAiAuthoringPolicy& policy,
    std::string* errorMessage) {
    if (input.empty() || input.size() > kMaximumRecordingBytes) {
        SetError(errorMessage, "AI simulation recording size is invalid.");
        return false;
    }
    std::istringstream stream{std::string(input)};
    std::string token;
    uint32_t schema = 0;
    std::size_t frameCount = 0;
    if (!(stream >> token >> schema >> frameCount) || token != "AI_SIM_RECORDING" ||
        schema != kEditorAiSimulationRecordingSchemaVersion || frameCount > policy.maximumRecordedFrames) {
        SetError(errorMessage, "AI simulation recording header or frame budget is invalid.");
        return false;
    }
    std::vector<EditorAiSimulationFrame> decoded;
    decoded.reserve(frameCount);
    for (std::size_t frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
        EditorAiSimulationFrame frame;
        std::size_t agentCount = 0, crowdCount = 0, slotCount = 0;
        if (!(stream >> token) || token != "FRAME" ||
            !(stream >> frame.frameIndex >> frame.behaviorGeneration >> frame.worldGeneration >>
              frame.deltaTime >> frame.fingerprint >> agentCount >> crowdCount >> slotCount) ||
            agentCount > policy.maximumAgentsPerFrame ||
            crowdCount > policy.maximumCrowdAgentsPerFrame ||
            slotCount > policy.maximumSmartObjectSlotsPerFrame) {
            SetError(errorMessage, "AI simulation frame header or capacity is invalid."); return false;
        }
        for (std::size_t agentIndex = 0; agentIndex < agentCount; ++agentIndex) {
            EditorAiAgentDebugSnapshot agent;
            std::string statusText;
            std::size_t traceCount = 0, blackboardCount = 0, perceivedCount = 0, pathCount = 0;
            if (!(stream >> token) || token != "AGENT" ||
                !(stream >> std::quoted(agent.entityGuid) >> std::quoted(agent.behaviorAssetGuid) >> statusText >>
                  agent.tickGeneration >> agent.perceptionGeneration >> agent.executedNodes >> traceCount >>
                  blackboardCount >> perceivedCount >> pathCount) || !ParseBehaviorStatus(statusText, agent.status) ||
                traceCount > kEditorBehaviorTreeMaximumNodes ||
                blackboardCount > kEditorBehaviorTreeMaximumBlackboardKeys || perceivedCount > 1024 || pathCount > 4096) {
                SetError(errorMessage, "AI simulation Agent record is invalid."); return false;
            }
            for (std::size_t index = 0; index < traceCount; ++index) {
                std::string node;
                if (!(stream >> token) || token != "TRACE" || !(stream >> std::quoted(node))) return false;
                agent.activeNodeTrace.push_back(std::move(node));
            }
            for (std::size_t index = 0; index < blackboardCount; ++index) {
                EditorBlackboardKeyDefinition key;
                std::string typeText;
                int boolValue = 0;
                if (!(stream >> token) || token != "BLACKBOARD" ||
                    !(stream >> std::quoted(key.name) >> typeText >> boolValue >> key.defaultValue.intValue >>
                      key.defaultValue.floatValue >> key.defaultValue.vectorValue.x >>
                      key.defaultValue.vectorValue.y >> key.defaultValue.vectorValue.z >>
                      std::quoted(key.defaultValue.textValue)) ||
                    !ParseBlackboardType(typeText, key.defaultValue.type) || (boolValue != 0 && boolValue != 1)) return false;
                key.defaultValue.boolValue = boolValue != 0;
                agent.blackboard.push_back(std::move(key));
            }
            for (std::size_t index = 0; index < perceivedCount; ++index) {
                EditorAiPerceivedStimulus perceived;
                int seen = 0, heard = 0;
                if (!(stream >> token) || token != "PERCEIVED" ||
                    !(stream >> std::quoted(perceived.entityGuid) >> perceived.position.x >> perceived.position.y >>
                      perceived.position.z >> perceived.strength >> seen >> heard) ||
                    (seen != 0 && seen != 1) || (heard != 0 && heard != 1)) return false;
                perceived.seen = seen != 0; perceived.heard = heard != 0;
                agent.perceived.push_back(std::move(perceived));
            }
            for (std::size_t index = 0; index < pathCount; ++index) {
                Vector3 point{};
                if (!(stream >> token) || token != "PATH" || !(stream >> point.x >> point.y >> point.z)) return false;
                agent.lastPath.push_back(point);
            }
            frame.agents.push_back(std::move(agent));
        }
        for (std::size_t index = 0; index < crowdCount; ++index) {
            EditorCrowdAgentSnapshot crowd;
            int constrained = 0;
            if (!(stream >> token) || token != "CROWD" ||
                !(stream >> std::quoted(crowd.entityGuid) >> crowd.position.x >> crowd.position.y >>
                  crowd.position.z >> crowd.preferredVelocity.x >> crowd.preferredVelocity.y >>
                  crowd.preferredVelocity.z >> crowd.steeringVelocity.x >> crowd.steeringVelocity.y >>
                  crowd.steeringVelocity.z >> crowd.radius >> crowd.maximumSpeed >> crowd.consideredNeighbors >>
                  constrained) || (constrained != 0 && constrained != 1)) return false;
            crowd.constrained = constrained != 0;
            frame.crowd.push_back(std::move(crowd));
        }
        for (std::size_t index = 0; index < slotCount; ++index) {
            EditorSmartObjectSlot slot;
            if (!(stream >> token) || token != "SLOT" ||
                !(stream >> std::quoted(slot.entityGuid) >> std::quoted(slot.slotId) >> std::quoted(slot.type) >>
                  slot.position.x >> slot.position.y >> slot.position.z >> slot.interactionRadius >> slot.priority >>
                  slot.leaseSeconds >> std::quoted(slot.reservedByEntityGuid) >> slot.reservationToken >>
                  slot.leaseExpirySeconds)) return false;
            frame.smartObjects.push_back(std::move(slot));
        }
        if (!(stream >> token) || token != "END_FRAME" || !IsFiniteFrame(frame)) return false;
        decoded.push_back(std::move(frame));
    }
    if (!(stream >> token) || token != "END") {
        SetError(errorMessage, "AI simulation recording terminator is missing."); return false;
    }
    frames = std::move(decoded);
    return true;
}

} // namespace editor
