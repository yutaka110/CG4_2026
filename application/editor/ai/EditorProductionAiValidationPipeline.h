#pragma once

#include "EditorProductionAiAuthoringPipeline.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

inline constexpr uint32_t kEditorAiValidationReportSchemaVersion = 1;
inline constexpr uint32_t kEditorAiFailureReproductionSchemaVersion = 1;

enum class EditorAiValidationOutcome : uint8_t {
    Passed,
    Failed,
    Invalid,
    BudgetExceeded,
    SourceError,
    NonDeterministic,
};

struct EditorAiValidationPerformanceBudget {
    uint32_t maximumAgentsPerFrame = 256;
    uint32_t maximumNavigationQueriesPerFrame = 256;
    uint32_t maximumPerceivedStimuliPerFrame = 4096;
    uint32_t maximumCrowdNeighborTestsPerFrame = 4096;
    uint32_t maximumEqsCandidateTestsPerFrame = 16384;
    double maximumSimulationMillisecondsPerFrame = 0.0;
};

struct EditorAiValidationScenario {
    std::string id;
    std::string name;
    uint64_t firstSeed = 1;
    uint32_t seedCount = 1;
    uint32_t repetitions = 1;
    uint32_t maximumFrames = 600;
    bool requireAgents = true;
    bool failOnAgentFailure = true;
    std::vector<std::string> requiredBehaviorNodes;
    std::vector<std::string> requiredEqsTests;
    EditorAiValidationPerformanceBudget budget{};
};

struct EditorAiValidationSuite {
    std::string id;
    std::string name;
    std::vector<EditorAiValidationScenario> scenarios;
};

struct EditorAiFrameTelemetrySample {
    uint32_t navigationQueries = 0;
    uint32_t navigationFailures = 0;
    uint32_t perceivedStimuli = 0;
    uint32_t crowdNeighborTests = 0;
    uint32_t eqsCandidateTests = 0;
    double simulationMilliseconds = 0.0;
    std::vector<std::string> executedEqsTests;
};

struct EditorAiValidationFailure {
    std::string code;
    std::string message;
    uint64_t frameIndex = 0;
    uint64_t frameFingerprint = 0;
};

struct EditorAiValidationRunResult {
    std::string scenarioId;
    uint64_t seed = 0;
    uint32_t repetition = 0;
    EditorAiValidationOutcome outcome = EditorAiValidationOutcome::Invalid;
    uint32_t frames = 0;
    uint32_t agentFrames = 0;
    uint32_t failedAgentFrames = 0;
    uint32_t navigationQueries = 0;
    uint32_t navigationFailures = 0;
    uint32_t perceivedStimuli = 0;
    uint32_t crowdNeighborTests = 0;
    uint32_t eqsCandidateTests = 0;
    uint32_t peakAgents = 0;
    uint32_t peakNavigationQueries = 0;
    uint32_t peakPerceivedStimuli = 0;
    uint32_t peakCrowdNeighborTests = 0;
    uint32_t peakEqsCandidateTests = 0;
    double peakSimulationMilliseconds = 0.0;
    double runnerMilliseconds = 0.0;
    uint64_t deterministicFingerprint = 0;
    std::map<std::string, uint32_t> behaviorNodeHits;
    std::map<std::string, uint32_t> eqsTestHits;
    std::vector<EditorAiValidationFailure> failures;
    EditorAiSimulationFrame reproductionFrame{};

    bool Passed() const noexcept { return outcome == EditorAiValidationOutcome::Passed; }
};

struct EditorAiValidationReport {
    uint32_t schemaVersion = kEditorAiValidationReportSchemaVersion;
    std::string suiteId;
    std::string suiteName;
    uint64_t generation = 0;
    bool passed = false;
    uint32_t passedRuns = 0;
    uint32_t failedRuns = 0;
    uint32_t totalFrames = 0;
    uint64_t deterministicFingerprint = 0;
    std::vector<EditorAiValidationRunResult> runs;
};

struct EditorAiValidationComparison {
    bool comparable = false;
    bool regression = false;
    int32_t passedRunDelta = 0;
    int32_t failedRunDelta = 0;
    int64_t frameDelta = 0;
    double peakSimulationMillisecondsDelta = 0.0;
    std::vector<std::string> diagnostics;
};

struct EditorAiValidationPolicy {
    uint32_t maximumScenarios = 64;
    uint32_t maximumSeedsPerScenario = 32;
    uint32_t maximumRepetitions = 8;
    uint32_t maximumRuns = 256;
    uint32_t maximumFramesPerRun = 3600;
    uint32_t maximumFailuresPerRun = 64;
    uint32_t maximumCoverageEntries = 4096;
    std::size_t maximumReportBytes = 16u * 1024u * 1024u;
};

struct EditorAiValidationStats {
    uint32_t suitesStarted = 0;
    uint32_t suitesCompleted = 0;
    uint32_t rejectedSuites = 0;
    uint32_t runsPassed = 0;
    uint32_t runsFailed = 0;
    uint32_t framesSimulated = 0;
    uint32_t budgetFailures = 0;
    uint32_t determinismFailures = 0;
    uint32_t exportedReports = 0;
    uint32_t exportedReproductions = 0;
    uint32_t comparisons = 0;
    uint32_t regressions = 0;
};

class IEditorAiBatchSimulationSource {
public:
    virtual ~IEditorAiBatchSimulationSource() = default;
    virtual std::string_view Id() const noexcept = 0;
    virtual bool BeginScenario(const EditorAiValidationScenario& scenario,
        uint64_t seed, std::string* errorMessage) = 0;
    virtual bool Step(EditorAiSimulationFrame& frame,
        EditorAiFrameTelemetrySample& telemetry, bool& hasFrame, bool& complete,
        std::string* errorMessage) = 0;
    virtual void EndScenario() = 0;
};

class EditorAiRecordingBatchSimulationSource final : public IEditorAiBatchSimulationSource {
public:
    explicit EditorAiRecordingBatchSimulationSource(
        const std::vector<EditorAiSimulationFrame>& frames) : frames_(frames) {}
    std::string_view Id() const noexcept override { return "editor.ai.batch.recording"; }
    bool BeginScenario(const EditorAiValidationScenario& scenario,
        uint64_t seed, std::string* errorMessage) override;
    bool Step(EditorAiSimulationFrame& frame, EditorAiFrameTelemetrySample& telemetry,
        bool& hasFrame, bool& complete, std::string* errorMessage) override;
    void EndScenario() override;

private:
    const std::vector<EditorAiSimulationFrame>& frames_;
    std::size_t cursor_ = 0;
    uint32_t maximumFrames_ = 0;
    bool active_ = false;
};

class EditorProductionAiValidationPipeline {
public:
    bool Initialize(EditorAiValidationPolicy policy = {},
        std::string* errorMessage = nullptr);
    void Shutdown();
    bool RunSuite(const EditorAiValidationSuite& suite,
        IEditorAiBatchSimulationSource& source, std::string* errorMessage = nullptr);
    bool ExportReport(const std::filesystem::path& basePath,
        std::string* errorMessage = nullptr);
    EditorAiValidationComparison CompareWith(
        const EditorAiValidationReport& baseline);

    bool Initialized() const noexcept { return initialized_; }
    const EditorAiValidationPolicy& Policy() const noexcept { return policy_; }
    const EditorAiValidationStats& Stats() const noexcept { return stats_; }
    const EditorAiValidationReport& Report() const noexcept { return report_; }
    bool HasReport() const noexcept { return report_.generation != 0; }

private:
    bool ValidateSuite(const EditorAiValidationSuite& suite,
        std::string* errorMessage) const;
    EditorAiValidationRunResult RunOne(const EditorAiValidationScenario& scenario,
        uint64_t seed, uint32_t repetition, IEditorAiBatchSimulationSource& source);

    bool initialized_ = false;
    uint64_t generation_ = 0;
    EditorAiValidationPolicy policy_{};
    EditorAiValidationStats stats_{};
    EditorAiValidationReport report_{};
};

std::string SerializeEditorAiValidationReportJson(const EditorAiValidationReport& report);
std::string SerializeEditorAiValidationReportMarkdown(const EditorAiValidationReport& report);
std::string SerializeEditorAiFailureReproduction(const EditorAiValidationReport& report,
    const EditorAiValidationRunResult& run);
const char* ToString(EditorAiValidationOutcome value) noexcept;

} // namespace editor
