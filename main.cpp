
#include <Windows.h>
#include <shellapi.h>

#include <filesystem>
#include <fstream>
#include <string_view>

#include "application/AppMain.h"
#include "application/EffectAssetLoader.h"
#include "application/EffectRuntime.h"
#include "application/include/vfx/VfxRenderInputs.h"

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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    if (HasArgument(L"--effect-authoring-smoke")) {
        return RunEffectAuthoringSmoke();
    }

    AppMain app;
    if (!app.Initialize(hInstance)) { return -1; }
    return app.Run();
}
