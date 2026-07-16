#include "AppCommandLineRunner.h"

#include <Windows.h>
#include <shellapi.h>

#include <filesystem>
#include <fstream>
#include <string_view>
#include <vector>

#include "EffectAssetLoader.h"
#include "EffectRuntime.h"
#if defined(GE3_BUILD_EDITOR) && GE3_BUILD_EDITOR
#include "editor/EditorAutomationGate.h"
#include "editor/EditorAssetFolderIndexer.h"
#include "editor/EditorAssetImportService.h"
#include "editor/EditorCoreRegressionTests.h"
#include "editor/EditorSmokeRun.h"
#include "editor/ai/EditorProductionAiValidationPipeline.h"
#endif
#include "include/vfx/VfxRenderInputs.h"

namespace {
bool HasArgument(std::wstring_view target) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == nullptr) {
        return false;
    }

    bool found = false;
    for (int i = 1; i < argc; ++i) {
        if (target == argv[i]) {
            found = true;
            break;
        }
    }
    LocalFree(argv);
    return found;
}

#if defined(GE3_BUILD_EDITOR) && GE3_BUILD_EDITOR
int RunEditorAssetMetaMigration() {
    const std::filesystem::path resourcesRoot = "Resources";
    editor::EditorAssetRegistry registry;
    const editor::EditorAssetFolderIndexResult indexed =
        editor::IndexEditorAssetsFromFolder(registry, resourcesRoot);

    editor::EditorAssetImportOptions options{};
    options.resourcesRoot = resourcesRoot;
    editor::EditorAssetImportService importService(registry);
    const editor::EditorAssetImportResult migrated =
        importService.BatchMigrateMetadata(options);

    const std::size_t eligible = registry.CountMetadataEligibleAssets();
    const std::size_t durable = registry.CountDurableAssets();
    const std::vector<std::string> duplicates = registry.DuplicateGuids();
    const bool ok =
        migrated.succeeded && indexed.identityCollisions == 0 &&
        eligible == durable && duplicates.empty();

    std::ofstream log("editor_asset_meta_migration.log", std::ios::trunc);
    log << "Editor Asset Metadata Migration\n";
    log << "scannedFiles=" << indexed.scannedFiles << '\n';
    log << "registeredAssets=" << indexed.registeredAssets << '\n';
    log << "identityCollisions=" << indexed.identityCollisions << '\n';
    log << "migratedAssets=" << migrated.migratedCount << '\n';
    log << "skippedAssets=" << migrated.skippedCount << '\n';
    log << "eligibleAssets=" << eligible << '\n';
    log << "durableAssets=" << durable << '\n';
    log << "coveragePercent=" << registry.MetadataCoveragePercent() << '\n';
    log << "duplicateGuids=" << duplicates.size() << '\n';
    log << "result=" << (ok ? "ok" : "failed") << '\n';
    return ok ? 0 : 1;
}

int RunEditorAiValidationBatch() {
    const std::filesystem::path recordingPath = "logs/editor_ai_simulation.record";
    std::ifstream input(recordingPath, std::ios::binary);
    if (!input.is_open()) return 2;
    const std::string encoded{std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
    editor::EditorAiAuthoringPolicy recordingPolicy{};
    std::vector<editor::EditorAiSimulationFrame> frames;
    std::string error;
    if (!editor::DecodeEditorAiSimulationRecording(
            encoded, frames, recordingPolicy, &error) || frames.empty()) return 2;

    editor::EditorProductionAiValidationPipeline validation;
    if (!validation.Initialize({}, &error)) return 2;
    editor::EditorAiValidationSuite suite;
    suite.id = "editor-ai-headless";
    suite.name = "Command-line AI validation";
    editor::EditorAiValidationScenario scenario;
    scenario.id = "recording";
    scenario.name = "Imported E-16 recording";
    scenario.firstSeed = 1;
    scenario.seedCount = 3;
    scenario.repetitions = 2;
    scenario.maximumFrames = static_cast<uint32_t>(frames.size());
    scenario.requireAgents = true;
    suite.scenarios.push_back(std::move(scenario));
    editor::EditorAiRecordingBatchSimulationSource source(frames);
    if (!validation.RunSuite(suite, source, &error) ||
        !validation.ExportReport("logs/editor_ai_validation", &error)) return 2;
    return validation.Report().passed ? 0 : 1;
}
#endif

int RunEffectAuthoringSmoke() {
    constexpr const char* kAssetName = "authoring_registry_only_demo";
    constexpr const char* kTechniqueId = "AuthoringRegistryOnlySpark";
    constexpr const char* kRendererId = "AuthoringParticleRenderer";
    constexpr const char* kSimulationId = "AuthoringCpuSpawnGpuSim";

    std::ofstream log("effect_authoring_smoke.log", std::ios::trunc);
    if (!log) {
        return 2;
    }

    EffectAuthoringRegistry authoringRegistry = EffectAuthoringRegistry::Default();
    EffectAssetLoader loader;
    const std::vector<LoadedEffectAsset> loadedAssets =
        loader.LoadDirectory(std::filesystem::path{"Resources"} / "effects", authoringRegistry);

    const LoadedEffectAsset* targetAsset = nullptr;
    EffectSystem effectSystem;
    for (const LoadedEffectAsset& loaded : loadedAssets) {
        if (loaded.asset.name == kAssetName) {
            targetAsset = &loaded;
        }
        effectSystem.RegisterAsset(loaded.asset, authoringRegistry);
    }

    uint32_t infoCount = 0;
    uint32_t warningCount = 0;
    uint32_t errorCount = 0;
    if (targetAsset != nullptr) {
        for (const EffectAssetDiagnostic& diagnostic : targetAsset->diagnostics) {
            switch (diagnostic.severity) {
            case EffectAssetDiagnosticSeverity::Info:
                ++infoCount;
                break;
            case EffectAssetDiagnosticSeverity::Warning:
                ++warningCount;
                break;
            case EffectAssetDiagnosticSeverity::Error:
                ++errorCount;
                break;
            }
        }
    }

    EffectRuntime effectRuntime(&effectSystem);
    effectRuntime.AttachAuthoringRegistry(&authoringRegistry);
    const uint32_t playId = effectRuntime.PlayEffectWithParams(
        kAssetName,
        {0.0f, 0.0f, 0.0f},
        {0.9f, 0.65f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f});
    const EffectRuntimeFrame frame = effectRuntime.BuildFrame();
    const ParticleRenderFallback fallback = frame.PrimaryParticleFallback();
    const ParticleRenderInput particleInput = frame.ParticleInput(fallback);

    const char* inputRendererId =
        particleInput.primary.rendererDescriptor != nullptr
            ? particleInput.primary.rendererDescriptor->id.c_str()
            : "none";
    const char* inputSimulationId =
        particleInput.primary.simulationDescriptor != nullptr
            ? particleInput.primary.simulationDescriptor->id.c_str()
            : "none";

    const bool ok =
        targetAsset != nullptr &&
        warningCount == 0 &&
        errorCount == 0 &&
        playId != 0 &&
        frame.particleQueue.size() == 1 &&
        frame.authoring.particle.techniqueId == kTechniqueId &&
        frame.authoring.particle.rendererId == kRendererId &&
        frame.authoring.particle.rendererResolved &&
        frame.authoring.particle.rendererDescriptorId == kRendererId &&
        frame.authoring.particle.simulationId == kSimulationId &&
        frame.authoring.particle.simulationResolved &&
        frame.authoring.particle.simulationDescriptorId == kSimulationId &&
        std::string_view{inputRendererId} == kRendererId &&
        std::string_view{inputSimulationId} == kSimulationId;

    log << "asset=" << kAssetName << '\n';
    log << "assetLoaded=" << (targetAsset != nullptr ? "yes" : "no") << '\n';
    log << "diagnostics info=" << infoCount
        << " warning=" << warningCount
        << " error=" << errorCount << '\n';
    log << "playId=" << playId << '\n';
    log << "particleQueueSize=" << frame.particleQueue.size() << '\n';
    log << "runtimeQueue techniqueId=" << frame.authoring.particle.techniqueId
        << " rendererId=" << frame.authoring.particle.rendererId
        << " rendererRegistry=" << (frame.authoring.particle.rendererResolved ? "resolved" : "unregistered")
        << " rendererDescriptorId=" << frame.authoring.particle.rendererDescriptorId
        << " simulationId=" << frame.authoring.particle.simulationId
        << " simulationRegistry=" << (frame.authoring.particle.simulationResolved ? "resolved" : "unregistered")
        << " simulationDescriptorId=" << frame.authoring.particle.simulationDescriptorId
        << '\n';
    log << "inputDescriptor renderer=" << inputRendererId
        << " simulation=" << inputSimulationId << '\n';
    log << "result=" << (ok ? "ok" : "failed") << '\n';
    return ok ? 0 : 1;
}
} // namespace

AppCommandLineResult RunAppCommandLineTools() {
#if defined(GE3_BUILD_EDITOR) && GE3_BUILD_EDITOR
    if (HasArgument(L"--editor-asset-meta-migrate")) {
        return {true, RunEditorAssetMetaMigration()};
    }
    if (HasArgument(L"--editor-commercial-gates")) {
        return {true, editor::RunEditorCommercialAutomationGates(RunEffectAuthoringSmoke)};
    }
    if (HasArgument(L"--editor-ai-validation")) {
        return {true, RunEditorAiValidationBatch()};
    }
    if (HasArgument(L"--editor-smoke-run")) {
        return {true, editor::RunEditorSmokeRun(RunEffectAuthoringSmoke)};
    }
    if (HasArgument(L"--editor-core-regression")) {
        return {true, editor::RunEditorCoreRegressionTests()};
    }
#endif
    if (HasArgument(L"--effect-authoring-smoke")) {
        return {true, RunEffectAuthoringSmoke()};
    }
    return {};
}
