#include "EditorProductionAiValidationPipeline.h"

#include "../io/EditorFileTransaction.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_map>

namespace editor {
namespace {
constexpr uint64_t kFnvOffset = 1469598103934665603ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;

void SetError(std::string* output, std::string value) {
    if (output != nullptr) *output = std::move(value);
}

template <typename T>
uint64_t HashValue(uint64_t hash, const T& value) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(&value);
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        hash ^= bytes[index];
        hash *= kFnvPrime;
    }
    return hash;
}

uint64_t HashText(uint64_t hash, std::string_view value) {
    for (const unsigned char byte : value) {
        hash ^= byte;
        hash *= kFnvPrime;
    }
    return hash;
}

uint64_t HashFrame(uint64_t hash, const EditorAiSimulationFrame& frame,
    const EditorAiFrameTelemetrySample& telemetry) {
    hash = HashValue(hash, frame.frameIndex);
    hash = HashValue(hash, frame.behaviorGeneration);
    hash = HashValue(hash, frame.worldGeneration);
    hash = HashValue(hash, frame.deltaTime);
    hash = HashValue(hash, frame.fingerprint);
    for (const auto& agent : frame.agents) {
        hash = HashText(hash, agent.entityGuid);
        hash = HashValue(hash, agent.status);
        hash = HashValue(hash, agent.executedNodes);
        for (const auto& node : agent.activeNodeTrace) hash = HashText(hash, node);
        for (const auto& stimulus : agent.perceived) {
            hash = HashText(hash, stimulus.entityGuid);
            hash = HashValue(hash, stimulus.strength);
            hash = HashValue(hash, stimulus.seen);
            hash = HashValue(hash, stimulus.heard);
        }
    }
    hash = HashValue(hash, telemetry.navigationQueries);
    hash = HashValue(hash, telemetry.navigationFailures);
    hash = HashValue(hash, telemetry.perceivedStimuli);
    hash = HashValue(hash, telemetry.crowdNeighborTests);
    hash = HashValue(hash, telemetry.eqsCandidateTests);
    for (const auto& id : telemetry.executedEqsTests) hash = HashText(hash, id);
    return hash;
}

std::string EscapeJson(std::string_view value) {
    std::string output;
    output.reserve(value.size() + 8);
    for (const char character : value) {
        switch (character) {
        case '\\': output += "\\\\"; break;
        case '"': output += "\\\""; break;
        case '\n': output += "\\n"; break;
        case '\r': output += "\\r"; break;
        case '\t': output += "\\t"; break;
        default:
            if (static_cast<unsigned char>(character) < 0x20) output += '?';
            else output += character;
            break;
        }
    }
    return output;
}

std::string SafeFileComponent(std::string_view value) {
    std::string output;
    output.reserve(value.size());
    for (const char character : value) {
        const bool valid = (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') || character == '-' || character == '_';
        output += valid ? character : '_';
    }
    return output.empty() ? "scenario" : output;
}

bool IsFailureStatus(EditorBehaviorStatus status) {
    return status == EditorBehaviorStatus::Failed ||
        status == EditorBehaviorStatus::BudgetExceeded ||
        status == EditorBehaviorStatus::InvalidProgram;
}

double PeakSimulationMilliseconds(const EditorAiValidationReport& report) {
    double peak = 0.0;
    for (const auto& run : report.runs)
        peak = (std::max)(peak, run.peakSimulationMilliseconds);
    return peak;
}
} // namespace

const char* ToString(EditorAiValidationOutcome value) noexcept {
    switch (value) {
    case EditorAiValidationOutcome::Passed: return "Passed";
    case EditorAiValidationOutcome::Failed: return "Failed";
    case EditorAiValidationOutcome::Invalid: return "Invalid";
    case EditorAiValidationOutcome::BudgetExceeded: return "BudgetExceeded";
    case EditorAiValidationOutcome::SourceError: return "SourceError";
    case EditorAiValidationOutcome::NonDeterministic: return "NonDeterministic";
    }
    return "Unknown";
}

bool EditorAiRecordingBatchSimulationSource::BeginScenario(
    const EditorAiValidationScenario& scenario, uint64_t, std::string* errorMessage) {
    if (frames_.empty()) {
        SetError(errorMessage, "AI batch recording source has no frames.");
        return false;
    }
    cursor_ = 0;
    maximumFrames_ = scenario.maximumFrames;
    active_ = true;
    SetError(errorMessage, {});
    return true;
}

bool EditorAiRecordingBatchSimulationSource::Step(EditorAiSimulationFrame& frame,
    EditorAiFrameTelemetrySample& telemetry, bool& hasFrame, bool& complete,
    std::string* errorMessage) {
    frame = {};
    telemetry = {};
    hasFrame = false;
    complete = false;
    if (!active_) {
        SetError(errorMessage, "AI batch recording source is not active.");
        return false;
    }
    if (cursor_ >= frames_.size() || cursor_ >= maximumFrames_) {
        complete = true;
        SetError(errorMessage, {});
        return true;
    }
    frame = frames_[cursor_++];
    hasFrame = true;
    complete = cursor_ >= frames_.size() || cursor_ >= maximumFrames_;
    for (const auto& agent : frame.agents) {
        telemetry.perceivedStimuli += static_cast<uint32_t>(agent.perceived.size());
        if (!agent.lastPath.empty()) ++telemetry.navigationQueries;
        if (IsFailureStatus(agent.status) && agent.lastPath.empty())
            ++telemetry.navigationFailures;
    }
    for (const auto& crowd : frame.crowd)
        telemetry.crowdNeighborTests += crowd.consideredNeighbors;
    SetError(errorMessage, {});
    return true;
}

void EditorAiRecordingBatchSimulationSource::EndScenario() {
    active_ = false;
    cursor_ = 0;
    maximumFrames_ = 0;
}

bool EditorProductionAiValidationPipeline::Initialize(
    EditorAiValidationPolicy policy, std::string* errorMessage) {
    if (policy.maximumScenarios == 0 || policy.maximumSeedsPerScenario == 0 ||
        policy.maximumRepetitions == 0 || policy.maximumRuns == 0 ||
        policy.maximumFramesPerRun == 0 || policy.maximumFailuresPerRun == 0 ||
        policy.maximumCoverageEntries == 0 || policy.maximumReportBytes < 1024) {
        SetError(errorMessage, "AI validation policy contains a zero or invalid capacity.");
        return false;
    }
    policy_ = policy;
    initialized_ = true;
    generation_ = 0;
    stats_ = {};
    report_ = {};
    SetError(errorMessage, {});
    return true;
}

void EditorProductionAiValidationPipeline::Shutdown() {
    initialized_ = false;
    report_ = {};
}

bool EditorProductionAiValidationPipeline::ValidateSuite(
    const EditorAiValidationSuite& suite, std::string* errorMessage) const {
    if (!initialized_) {
        SetError(errorMessage, "AI validation pipeline is not initialized.");
        return false;
    }
    if (suite.id.empty() || suite.scenarios.empty() ||
        suite.scenarios.size() > policy_.maximumScenarios) {
        SetError(errorMessage, "AI validation suite identity or scenario count is invalid.");
        return false;
    }
    uint64_t totalRuns = 0;
    std::vector<std::string> ids;
    for (const auto& scenario : suite.scenarios) {
        if (scenario.id.empty() || scenario.seedCount == 0 || scenario.repetitions == 0 ||
            scenario.maximumFrames == 0 ||
            scenario.seedCount > policy_.maximumSeedsPerScenario ||
            scenario.repetitions > policy_.maximumRepetitions ||
            scenario.maximumFrames > policy_.maximumFramesPerRun ||
            scenario.requiredBehaviorNodes.size() + scenario.requiredEqsTests.size() >
                policy_.maximumCoverageEntries ||
            !std::isfinite(scenario.budget.maximumSimulationMillisecondsPerFrame) ||
            scenario.budget.maximumSimulationMillisecondsPerFrame < 0.0) {
            SetError(errorMessage, "AI validation scenario policy exceeds a bounded capacity.");
            return false;
        }
        if (std::find(ids.begin(), ids.end(), scenario.id) != ids.end()) {
            SetError(errorMessage, "AI validation scenario IDs must be unique.");
            return false;
        }
        ids.push_back(scenario.id);
        totalRuns += static_cast<uint64_t>(scenario.seedCount) * scenario.repetitions;
    }
    if (totalRuns > policy_.maximumRuns) {
        SetError(errorMessage, "AI validation suite exceeds the bounded run count.");
        return false;
    }
    SetError(errorMessage, {});
    return true;
}

EditorAiValidationRunResult EditorProductionAiValidationPipeline::RunOne(
    const EditorAiValidationScenario& scenario, uint64_t seed, uint32_t repetition,
    IEditorAiBatchSimulationSource& source) {
    EditorAiValidationRunResult result;
    result.scenarioId = scenario.id;
    result.seed = seed;
    result.repetition = repetition;
    result.outcome = EditorAiValidationOutcome::Failed;
    uint64_t fingerprint = HashText(kFnvOffset, scenario.id);
    fingerprint = HashValue(fingerprint, seed);

    const auto addFailure = [&](std::string code, std::string message,
                                const EditorAiSimulationFrame* frame) {
        if (result.failures.size() < policy_.maximumFailuresPerRun) {
            EditorAiValidationFailure failure;
            failure.code = std::move(code);
            failure.message = std::move(message);
            if (frame != nullptr) {
                failure.frameIndex = frame->frameIndex;
                failure.frameFingerprint = frame->fingerprint;
                if (result.reproductionFrame.frameIndex == 0) result.reproductionFrame = *frame;
            }
            result.failures.push_back(std::move(failure));
        }
    };

    std::string sourceError;
    if (!source.BeginScenario(scenario, seed, &sourceError)) {
        addFailure("source-begin", sourceError.empty() ? "Simulation source failed to begin." : sourceError, nullptr);
        result.outcome = EditorAiValidationOutcome::SourceError;
        result.deterministicFingerprint = fingerprint;
        return result;
    }

    const auto started = std::chrono::steady_clock::now();
    bool complete = false;
    bool sourceFailed = false;
    while (!complete && result.frames < scenario.maximumFrames) {
        EditorAiSimulationFrame frame;
        EditorAiFrameTelemetrySample telemetry;
        bool hasFrame = false;
        if (!source.Step(frame, telemetry, hasFrame, complete, &sourceError)) {
            addFailure("source-step", sourceError.empty() ? "Simulation source step failed." : sourceError, nullptr);
            sourceFailed = true;
            break;
        }
        if (!hasFrame) {
            if (!complete) {
                addFailure("source-contract", "Simulation source returned neither a frame nor completion.", nullptr);
                sourceFailed = true;
            }
            continue;
        }
        ++result.frames;
        result.agentFrames += static_cast<uint32_t>(frame.agents.size());
        result.navigationQueries += telemetry.navigationQueries;
        result.navigationFailures += telemetry.navigationFailures;
        result.perceivedStimuli += telemetry.perceivedStimuli;
        result.crowdNeighborTests += telemetry.crowdNeighborTests;
        result.eqsCandidateTests += telemetry.eqsCandidateTests;
        result.peakAgents = (std::max)(result.peakAgents, static_cast<uint32_t>(frame.agents.size()));
        result.peakNavigationQueries = (std::max)(result.peakNavigationQueries, telemetry.navigationQueries);
        result.peakPerceivedStimuli = (std::max)(result.peakPerceivedStimuli, telemetry.perceivedStimuli);
        result.peakCrowdNeighborTests = (std::max)(result.peakCrowdNeighborTests, telemetry.crowdNeighborTests);
        result.peakEqsCandidateTests = (std::max)(result.peakEqsCandidateTests, telemetry.eqsCandidateTests);
        result.peakSimulationMilliseconds =
            (std::max)(result.peakSimulationMilliseconds, telemetry.simulationMilliseconds);
        fingerprint = HashFrame(fingerprint, frame, telemetry);

        for (const auto& agent : frame.agents) {
            if (IsFailureStatus(agent.status)) {
                ++result.failedAgentFrames;
                if (scenario.failOnAgentFailure)
                    addFailure("agent-status", std::string("Agent ") + agent.entityGuid +
                        " ended a frame with " + ToString(agent.status) + '.', &frame);
            }
            for (const auto& node : agent.activeNodeTrace) {
                if (result.behaviorNodeHits.size() >= policy_.maximumCoverageEntries &&
                    result.behaviorNodeHits.find(node) == result.behaviorNodeHits.end()) {
                    addFailure("coverage-budget", "Behavior coverage entry budget was exceeded.", &frame);
                    break;
                }
                ++result.behaviorNodeHits[node];
            }
        }
        for (const auto& test : telemetry.executedEqsTests) {
            if (result.eqsTestHits.size() >= policy_.maximumCoverageEntries &&
                result.eqsTestHits.find(test) == result.eqsTestHits.end()) {
                addFailure("coverage-budget", "EQS coverage entry budget was exceeded.", &frame);
                break;
            }
            ++result.eqsTestHits[test];
        }

        if (frame.agents.size() > scenario.budget.maximumAgentsPerFrame)
            addFailure("agent-budget", "Agent count exceeded the per-frame validation budget.", &frame);
        if (telemetry.navigationQueries > scenario.budget.maximumNavigationQueriesPerFrame)
            addFailure("navigation-budget", "Navigation queries exceeded the per-frame validation budget.", &frame);
        if (telemetry.perceivedStimuli > scenario.budget.maximumPerceivedStimuliPerFrame)
            addFailure("perception-budget", "Perceived stimuli exceeded the per-frame validation budget.", &frame);
        if (telemetry.crowdNeighborTests > scenario.budget.maximumCrowdNeighborTestsPerFrame)
            addFailure("crowd-budget", "Crowd neighbor tests exceeded the per-frame validation budget.", &frame);
        if (telemetry.eqsCandidateTests > scenario.budget.maximumEqsCandidateTestsPerFrame)
            addFailure("eqs-budget", "EQS candidate tests exceeded the per-frame validation budget.", &frame);
        if (scenario.budget.maximumSimulationMillisecondsPerFrame > 0.0 &&
            telemetry.simulationMilliseconds > scenario.budget.maximumSimulationMillisecondsPerFrame)
            addFailure("time-budget", "Simulation time exceeded the per-frame validation budget.", &frame);
    }
    source.EndScenario();
    result.runnerMilliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    if (!sourceFailed && !complete && result.frames >= scenario.maximumFrames)
        addFailure("frame-budget", "Scenario did not complete inside its bounded frame count.",
            result.reproductionFrame.frameIndex != 0 ? &result.reproductionFrame : nullptr);
    if (scenario.requireAgents && result.agentFrames == 0)
        addFailure("agent-coverage", "Scenario produced no AI Agent observations.", nullptr);
    for (const auto& node : scenario.requiredBehaviorNodes)
        if (result.behaviorNodeHits.find(node) == result.behaviorNodeHits.end())
            addFailure("behavior-coverage", std::string("Required Behavior node was not observed: ") + node, nullptr);
    for (const auto& test : scenario.requiredEqsTests)
        if (result.eqsTestHits.find(test) == result.eqsTestHits.end())
            addFailure("eqs-coverage", std::string("Required EQS test was not observed: ") + test, nullptr);

    result.deterministicFingerprint = fingerprint;
    if (sourceFailed) result.outcome = EditorAiValidationOutcome::SourceError;
    else if (result.failures.empty()) result.outcome = EditorAiValidationOutcome::Passed;
    else if (std::any_of(result.failures.begin(), result.failures.end(), [](const auto& failure) {
            return failure.code.find("budget") != std::string::npos;
        })) result.outcome = EditorAiValidationOutcome::BudgetExceeded;
    else result.outcome = EditorAiValidationOutcome::Failed;
    return result;
}

bool EditorProductionAiValidationPipeline::RunSuite(const EditorAiValidationSuite& suite,
    IEditorAiBatchSimulationSource& source, std::string* errorMessage) {
    if (!ValidateSuite(suite, errorMessage)) {
        ++stats_.rejectedSuites;
        return false;
    }
    ++stats_.suitesStarted;
    EditorAiValidationReport next;
    next.suiteId = suite.id;
    next.suiteName = suite.name;
    next.generation = ++generation_;
    std::unordered_map<std::string, uint64_t> repeatFingerprints;
    for (const auto& scenario : suite.scenarios) {
        for (uint32_t seedIndex = 0; seedIndex < scenario.seedCount; ++seedIndex) {
            const uint64_t seed = scenario.firstSeed + seedIndex;
            const std::string determinismKey = scenario.id + "\n" + std::to_string(seed);
            for (uint32_t repetition = 0; repetition < scenario.repetitions; ++repetition) {
                EditorAiValidationRunResult run = RunOne(scenario, seed, repetition, source);
                const auto expected = repeatFingerprints.find(determinismKey);
                if (expected == repeatFingerprints.end()) {
                    repeatFingerprints.emplace(determinismKey, run.deterministicFingerprint);
                } else if (expected->second != run.deterministicFingerprint) {
                    if (run.failures.size() < policy_.maximumFailuresPerRun)
                        run.failures.push_back({"determinism",
                            "Repeated scenario and seed produced a different deterministic fingerprint.",
                            run.reproductionFrame.frameIndex, run.reproductionFrame.fingerprint});
                    run.outcome = EditorAiValidationOutcome::NonDeterministic;
                    ++stats_.determinismFailures;
                }
                next.totalFrames += run.frames;
                if (run.Passed()) { ++next.passedRuns; ++stats_.runsPassed; }
                else { ++next.failedRuns; ++stats_.runsFailed; }
                if (run.outcome == EditorAiValidationOutcome::BudgetExceeded) ++stats_.budgetFailures;
                stats_.framesSimulated += run.frames;
                next.runs.push_back(std::move(run));
            }
        }
    }
    next.passed = next.failedRuns == 0 && !next.runs.empty();
    uint64_t reportFingerprint = HashText(kFnvOffset, next.suiteId);
    for (const auto& run : next.runs) {
        reportFingerprint = HashText(reportFingerprint, run.scenarioId);
        reportFingerprint = HashValue(reportFingerprint, run.seed);
        reportFingerprint = HashValue(reportFingerprint, run.repetition);
        reportFingerprint = HashValue(reportFingerprint, run.outcome);
        reportFingerprint = HashValue(reportFingerprint, run.deterministicFingerprint);
        for (const auto& failure : run.failures)
            reportFingerprint = HashText(HashText(reportFingerprint, failure.code), failure.message);
    }
    next.deterministicFingerprint = reportFingerprint;
    report_ = std::move(next);
    ++stats_.suitesCompleted;
    SetError(errorMessage, {});
    return true;
}

bool EditorProductionAiValidationPipeline::ExportReport(
    const std::filesystem::path& basePath, std::string* errorMessage) {
    if (!initialized_ || !HasReport() || basePath.empty()) {
        SetError(errorMessage, "AI validation report is not available for export.");
        return false;
    }
    std::filesystem::path jsonPath = basePath;
    jsonPath.replace_extension(".json");
    std::filesystem::path markdownPath = basePath;
    markdownPath.replace_extension(".md");
    const std::string json = SerializeEditorAiValidationReportJson(report_);
    const std::string markdown = SerializeEditorAiValidationReportMarkdown(report_);
    if (json.size() + markdown.size() > policy_.maximumReportBytes) {
        SetError(errorMessage, "AI validation report exceeds the bounded export size.");
        return false;
    }
    EditorFileTransaction transaction(std::filesystem::current_path());
    if (!transaction.StageTextWrite(jsonPath, json, {}, errorMessage) ||
        !transaction.StageTextWrite(markdownPath, markdown, {}, errorMessage)) return false;
    uint32_t reproductions = 0;
    const std::filesystem::path failureRoot = basePath.parent_path() /
        (basePath.stem().string() + "_failures");
    for (const auto& run : report_.runs) {
        if (run.Passed()) continue;
        const std::string stem = SafeFileComponent(run.scenarioId) + "_seed" +
            std::to_string(run.seed) + "_repeat" + std::to_string(run.repetition);
        if (!transaction.StageTextWrite(failureRoot / (stem + ".repro"),
                SerializeEditorAiFailureReproduction(report_, run), {}, errorMessage)) return false;
        if (run.reproductionFrame.frameIndex != 0) {
            std::string recording;
            if (!EncodeEditorAiSimulationRecording({run.reproductionFrame}, recording, errorMessage) ||
                !transaction.StageTextWrite(failureRoot / (stem + ".record"),
                    std::move(recording), {}, errorMessage)) return false;
        }
        ++reproductions;
    }
    if (!transaction.Execute(nullptr, errorMessage)) return false;
    ++stats_.exportedReports;
    stats_.exportedReproductions += reproductions;
    return true;
}

EditorAiValidationComparison EditorProductionAiValidationPipeline::CompareWith(
    const EditorAiValidationReport& baseline) {
    EditorAiValidationComparison comparison;
    ++stats_.comparisons;
    if (!HasReport() || baseline.schemaVersion != report_.schemaVersion ||
        baseline.suiteId != report_.suiteId) {
        comparison.diagnostics.push_back(
            "Reports must share schema version and suite identity before comparison.");
        return comparison;
    }
    comparison.comparable = true;
    comparison.passedRunDelta = static_cast<int32_t>(report_.passedRuns) -
        static_cast<int32_t>(baseline.passedRuns);
    comparison.failedRunDelta = static_cast<int32_t>(report_.failedRuns) -
        static_cast<int32_t>(baseline.failedRuns);
    comparison.frameDelta = static_cast<int64_t>(report_.totalFrames) -
        static_cast<int64_t>(baseline.totalFrames);
    comparison.peakSimulationMillisecondsDelta =
        PeakSimulationMilliseconds(report_) - PeakSimulationMilliseconds(baseline);
    comparison.regression = comparison.failedRunDelta > 0 ||
        comparison.passedRunDelta < 0 || comparison.peakSimulationMillisecondsDelta > 0.01;
    if (comparison.failedRunDelta > 0)
        comparison.diagnostics.push_back("Failed run count increased from the baseline.");
    if (comparison.passedRunDelta < 0)
        comparison.diagnostics.push_back("Passed run count decreased from the baseline.");
    if (comparison.peakSimulationMillisecondsDelta > 0.01)
        comparison.diagnostics.push_back("Peak simulation time regressed from the baseline.");
    if (comparison.regression) ++stats_.regressions;
    return comparison;
}

std::string SerializeEditorAiValidationReportJson(const EditorAiValidationReport& report) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(4);
    stream << "{\n  \"schema\": \"editor.aiValidation.v" << report.schemaVersion << "\",\n"
           << "  \"suiteId\": \"" << EscapeJson(report.suiteId) << "\",\n"
           << "  \"suiteName\": \"" << EscapeJson(report.suiteName) << "\",\n"
           << "  \"generation\": " << report.generation << ",\n"
           << "  \"result\": \"" << (report.passed ? "passed" : "failed") << "\",\n"
           << "  \"passedRuns\": " << report.passedRuns << ",\n"
           << "  \"failedRuns\": " << report.failedRuns << ",\n"
           << "  \"totalFrames\": " << report.totalFrames << ",\n"
           << "  \"fingerprint\": \"" << std::hex << std::setw(16) << std::setfill('0')
           << report.deterministicFingerprint << std::dec << std::setfill(' ') << "\",\n"
           << "  \"runs\": [\n";
    for (std::size_t index = 0; index < report.runs.size(); ++index) {
        const auto& run = report.runs[index];
        stream << "    {\"scenario\":\"" << EscapeJson(run.scenarioId)
               << "\",\"seed\":" << run.seed << ",\"repetition\":" << run.repetition
               << ",\"outcome\":\"" << ToString(run.outcome) << "\",\"frames\":" << run.frames
               << ",\"agentFrames\":" << run.agentFrames
               << ",\"failedAgentFrames\":" << run.failedAgentFrames
               << ",\"navigationQueries\":" << run.navigationQueries
               << ",\"navigationFailures\":" << run.navigationFailures
               << ",\"perceivedStimuli\":" << run.perceivedStimuli
               << ",\"crowdNeighborTests\":" << run.crowdNeighborTests
               << ",\"eqsCandidateTests\":" << run.eqsCandidateTests
               << ",\"peakSimulationMs\":" << run.peakSimulationMilliseconds
               << ",\"runnerMs\":" << run.runnerMilliseconds
               << ",\"fingerprint\":\"" << std::hex << std::setw(16) << std::setfill('0')
               << run.deterministicFingerprint << std::dec << std::setfill(' ') << "\",\"failures\":[";
        for (std::size_t failureIndex = 0; failureIndex < run.failures.size(); ++failureIndex) {
            const auto& failure = run.failures[failureIndex];
            stream << "{\"code\":\"" << EscapeJson(failure.code) << "\",\"message\":\""
                   << EscapeJson(failure.message) << "\",\"frame\":" << failure.frameIndex << '}';
            if (failureIndex + 1 != run.failures.size()) stream << ',';
        }
        stream << "],\"behaviorCoverage\":{";
        std::size_t coverageIndex = 0;
        for (const auto& [id, hits] : run.behaviorNodeHits) {
            if (coverageIndex++ != 0) stream << ',';
            stream << '"' << EscapeJson(id) << "\":" << hits;
        }
        stream << "},\"eqsCoverage\":{";
        coverageIndex = 0;
        for (const auto& [id, hits] : run.eqsTestHits) {
            if (coverageIndex++ != 0) stream << ',';
            stream << '"' << EscapeJson(id) << "\":" << hits;
        }
        stream << "}}" << (index + 1 == report.runs.size() ? "\n" : ",\n");
    }
    stream << "  ]\n}\n";
    return stream.str();
}

std::string SerializeEditorAiValidationReportMarkdown(const EditorAiValidationReport& report) {
    std::ostringstream stream;
    stream << "# AI Validation Report\n\n"
           << "- Suite: `" << report.suiteId << "`\n"
           << "- Result: **" << (report.passed ? "PASSED" : "FAILED") << "**\n"
           << "- Runs: " << report.passedRuns << " passed / " << report.failedRuns << " failed\n"
           << "- Frames: " << report.totalFrames << "\n"
           << "- Fingerprint: `" << std::hex << std::setw(16) << std::setfill('0')
           << report.deterministicFingerprint << std::dec << "`\n\n"
           << "| Scenario | Seed | Repeat | Result | Frames | Agent frames | Nav | Perception | Crowd | EQS |\n"
           << "| --- | ---: | ---: | --- | ---: | ---: | ---: | ---: | ---: | ---: |\n";
    for (const auto& run : report.runs)
        stream << "| " << run.scenarioId << " | " << run.seed << " | " << run.repetition
               << " | " << ToString(run.outcome) << " | " << run.frames << " | " << run.agentFrames
               << " | " << run.navigationQueries << " | " << run.perceivedStimuli
               << " | " << run.crowdNeighborTests << " | " << run.eqsCandidateTests << " |\n";
    for (const auto& run : report.runs) {
        if (run.failures.empty()) continue;
        stream << "\n## " << run.scenarioId << " seed " << run.seed
               << " repeat " << run.repetition << "\n\n";
        for (const auto& failure : run.failures)
            stream << "- `" << failure.code << "` frame " << failure.frameIndex
                   << ": " << failure.message << "\n";
    }
    return stream.str();
}

std::string SerializeEditorAiFailureReproduction(const EditorAiValidationReport& report,
    const EditorAiValidationRunResult& run) {
    std::ostringstream stream;
    stream << "AI_FAILURE_REPRO " << kEditorAiFailureReproductionSchemaVersion << '\n'
           << "suite " << std::quoted(report.suiteId) << '\n'
           << "scenario " << std::quoted(run.scenarioId) << '\n'
           << "seed " << run.seed << '\n'
           << "repetition " << run.repetition << '\n'
           << "outcome " << ToString(run.outcome) << '\n'
           << "runFingerprint " << run.deterministicFingerprint << '\n'
           << "reportFingerprint " << report.deterministicFingerprint << '\n'
           << "frame " << run.reproductionFrame.frameIndex << '\n'
           << "frameFingerprint " << run.reproductionFrame.fingerprint << '\n';
    for (const auto& failure : run.failures)
        stream << "failure " << std::quoted(failure.code) << ' '
               << std::quoted(failure.message) << ' ' << failure.frameIndex << '\n';
    stream << "END\n";
    return stream.str();
}

} // namespace editor
