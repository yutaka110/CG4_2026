#include "EditorProductionAiValidationPanel.h"

#include "EditorProductionAiAuthoringPipeline.h"
#include "EditorProductionAiValidationPipeline.h"
#include "../EditorNotificationCenter.h"

#include "../../../externals/imgui/imgui.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

namespace editor {
namespace {
void Notify(EditorNotificationCenter* center, const std::string& message,
    EditorNotificationSeverity severity = EditorNotificationSeverity::Error) {
    if (center != nullptr) center->Push(severity, "AI Validation", message);
}
} // namespace

void DrawEditorProductionAiValidationPanel(
    const EditorProductionAiValidationPanelContext& context) {
    if (context.validation == nullptr || context.authoring == nullptr) {
        ImGui::TextDisabled("AI validation services are unavailable.");
        return;
    }
    EditorProductionAiValidationPipeline& validation = *context.validation;
    EditorProductionAiAuthoringPipeline& authoring = *context.authoring;
    const auto& recording = authoring.RecordingFrames();

    static std::array<char, 96> suiteId{"editor-ai-production"};
    static std::array<char, 96> scenarioId{"recorded-gameplay"};
    static int firstSeed = 1;
    static int seedCount = 3;
    static int repetitions = 2;
    static int maximumFrames = 600;
    static bool requireAgents = true;
    static bool failOnAgentFailure = true;
    static bool requireActiveBehaviorCoverage = false;
    static int agentBudget = 256;
    static int navigationBudget = 256;
    static int perceptionBudget = 4096;
    static int crowdBudget = 4096;
    static int eqsBudget = 16384;
    static float timeBudgetMs = 0.0f;
    static int selectedRun = 0;
    static EditorAiValidationReport baseline;
    static bool hasBaseline = false;
    static EditorAiValidationComparison comparison;

    ImGui::Text("Recording %u frames  Validation policy %u runs / %u frames per run",
        static_cast<unsigned>(recording.size()), validation.Policy().maximumRuns,
        validation.Policy().maximumFramesPerRun);
    if (recording.empty())
        ImGui::TextColored(ImVec4(1.0f, .65f, .25f, 1.0f),
            "Record an E-16 simulation before running a recording-backed batch.");

    ImGui::SetNextItemWidth(220.0f); ImGui::InputText("Suite ID", suiteId.data(), suiteId.size());
    ImGui::SetNextItemWidth(220.0f); ImGui::InputText("Scenario ID", scenarioId.data(), scenarioId.size());
    ImGui::SetNextItemWidth(100.0f); ImGui::InputInt("First Seed", &firstSeed);
    ImGui::SameLine(); ImGui::SetNextItemWidth(90.0f); ImGui::InputInt("Seeds", &seedCount);
    ImGui::SameLine(); ImGui::SetNextItemWidth(90.0f); ImGui::InputInt("Repeats", &repetitions);
    ImGui::SameLine(); ImGui::SetNextItemWidth(110.0f); ImGui::InputInt("Max Frames", &maximumFrames);
    ImGui::Checkbox("Require Agents", &requireAgents); ImGui::SameLine();
    ImGui::Checkbox("Fail On Agent Failure", &failOnAgentFailure); ImGui::SameLine();
    ImGui::Checkbox("Require Active Behavior Coverage", &requireActiveBehaviorCoverage);

    if (ImGui::TreeNodeEx("Performance Budgets", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SetNextItemWidth(100.0f); ImGui::InputInt("Agents / frame", &agentBudget);
        ImGui::SameLine(); ImGui::SetNextItemWidth(100.0f); ImGui::InputInt("Nav queries / frame", &navigationBudget);
        ImGui::SetNextItemWidth(100.0f); ImGui::InputInt("Perception / frame", &perceptionBudget);
        ImGui::SameLine(); ImGui::SetNextItemWidth(100.0f); ImGui::InputInt("Crowd tests / frame", &crowdBudget);
        ImGui::SetNextItemWidth(100.0f); ImGui::InputInt("EQS tests / frame", &eqsBudget);
        ImGui::SameLine(); ImGui::SetNextItemWidth(100.0f); ImGui::InputFloat("Simulation ms / frame", &timeBudgetMs);
        ImGui::TextDisabled("A simulation-time budget of 0 disables timing checks for recording sources.");
        ImGui::TreePop();
    }

    ImGui::BeginDisabled(recording.empty() || !validation.Initialized());
    if (ImGui::Button("Run Headless Batch")) {
        if (validation.HasReport()) { baseline = validation.Report(); hasBaseline = true; }
        EditorAiValidationSuite suite;
        suite.id = suiteId.data();
        suite.name = "Editor AI production recording validation";
        EditorAiValidationScenario scenario;
        scenario.id = scenarioId.data();
        scenario.name = "Recorded gameplay validation";
        scenario.firstSeed = static_cast<uint64_t>((std::max)(0, firstSeed));
        scenario.seedCount = static_cast<uint32_t>((std::max)(1, seedCount));
        scenario.repetitions = static_cast<uint32_t>((std::max)(1, repetitions));
        scenario.maximumFrames = static_cast<uint32_t>((std::max)(1, maximumFrames));
        scenario.requireAgents = requireAgents;
        scenario.failOnAgentFailure = failOnAgentFailure;
        scenario.budget.maximumAgentsPerFrame = static_cast<uint32_t>((std::max)(0, agentBudget));
        scenario.budget.maximumNavigationQueriesPerFrame = static_cast<uint32_t>((std::max)(0, navigationBudget));
        scenario.budget.maximumPerceivedStimuliPerFrame = static_cast<uint32_t>((std::max)(0, perceptionBudget));
        scenario.budget.maximumCrowdNeighborTestsPerFrame = static_cast<uint32_t>((std::max)(0, crowdBudget));
        scenario.budget.maximumEqsCandidateTestsPerFrame = static_cast<uint32_t>((std::max)(0, eqsBudget));
        scenario.budget.maximumSimulationMillisecondsPerFrame = (std::max)(0.0f, timeBudgetMs);
        if (requireActiveBehaviorCoverage) {
            if (const EditorBehaviorTreeAsset* behavior = authoring.ActiveBehaviorTree())
                for (const auto& node : behavior->nodes) scenario.requiredBehaviorNodes.push_back(node.id);
        }
        suite.scenarios.push_back(std::move(scenario));
        EditorAiRecordingBatchSimulationSource source(recording);
        std::string error;
        if (!validation.RunSuite(suite, source, &error)) Notify(context.notifications, error);
        else {
            comparison = hasBaseline ? validation.CompareWith(baseline) : EditorAiValidationComparison{};
            selectedRun = 0;
            Notify(context.notifications,
                validation.Report().passed ? "AI validation batch passed." : "AI validation batch found failures.",
                validation.Report().passed ? EditorNotificationSeverity::Info : EditorNotificationSeverity::Warning);
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!validation.HasReport());
    if (ImGui::Button("Export Report + Repro")) {
        std::string error;
        if (!validation.ExportReport("logs/editor_ai_validation", &error)) Notify(context.notifications, error);
        else Notify(context.notifications,
            "AI validation JSON, Markdown, and failure reproduction artifacts exported.",
            EditorNotificationSeverity::Info);
    }
    ImGui::EndDisabled();

    if (!validation.HasReport()) return;
    const EditorAiValidationReport& report = validation.Report();
    ImGui::Separator();
    ImGui::TextColored(report.passed ? ImVec4(.35f, .9f, .5f, 1.0f) : ImVec4(1.0f, .4f, .25f, 1.0f),
        "%s  runs %u passed / %u failed  frames %u  fingerprint %016llx",
        report.passed ? "PASSED" : "FAILED", report.passedRuns, report.failedRuns,
        report.totalFrames, static_cast<unsigned long long>(report.deterministicFingerprint));
    if (comparison.comparable)
        ImGui::Text("Baseline delta: pass %+d fail %+d frames %+lld peak time %+.3f ms%s",
            comparison.passedRunDelta, comparison.failedRunDelta,
            static_cast<long long>(comparison.frameDelta), comparison.peakSimulationMillisecondsDelta,
            comparison.regression ? "  REGRESSION" : "");

    selectedRun = report.runs.empty() ? 0 : (std::clamp)(selectedRun, 0,
        static_cast<int>(report.runs.size() - 1));
    if (report.runs.empty()) return;
    const EditorAiValidationRunResult& run = report.runs[selectedRun];
    ImGui::SetNextItemWidth(250.0f);
    const std::string label = run.scenarioId + " / seed " + std::to_string(run.seed) +
        " / repeat " + std::to_string(run.repetition);
    if (ImGui::BeginCombo("Run", label.c_str())) {
        for (int index = 0; index < static_cast<int>(report.runs.size()); ++index) {
            const auto& candidate = report.runs[index];
            const std::string candidateLabel = candidate.scenarioId + " / seed " +
                std::to_string(candidate.seed) + " / repeat " + std::to_string(candidate.repetition) +
                " / " + ToString(candidate.outcome);
            if (ImGui::Selectable(candidateLabel.c_str(), index == selectedRun)) selectedRun = index;
        }
        ImGui::EndCombo();
    }
    ImGui::Text("%s  frames=%u agentFrames=%u failedAgentFrames=%u runner=%.3fms",
        ToString(run.outcome), run.frames, run.agentFrames, run.failedAgentFrames, run.runnerMilliseconds);
    ImGui::Text("Nav %u/%u failed  Perception %u  Crowd %u  EQS %u  Peak source %.3fms",
        run.navigationQueries, run.navigationFailures, run.perceivedStimuli,
        run.crowdNeighborTests, run.eqsCandidateTests, run.peakSimulationMilliseconds);
    if (ImGui::TreeNodeEx("Coverage", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Behavior nodes %u", static_cast<unsigned>(run.behaviorNodeHits.size()));
        for (const auto& [id, hits] : run.behaviorNodeHits) ImGui::BulletText("%s: %u", id.c_str(), hits);
        ImGui::Text("EQS tests %u", static_cast<unsigned>(run.eqsTestHits.size()));
        for (const auto& [id, hits] : run.eqsTestHits) ImGui::BulletText("%s: %u", id.c_str(), hits);
        ImGui::TreePop();
    }
    for (const auto& failure : run.failures)
        ImGui::TextWrapped("[%s] frame %llu: %s", failure.code.c_str(),
            static_cast<unsigned long long>(failure.frameIndex), failure.message.c_str());
}

} // namespace editor
