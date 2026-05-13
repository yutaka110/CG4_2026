#include "VfxEngine.h"

#include "AppRuntimeState.h"
#include "AppSceneResources.h"
#include "AppVfxRenderPipeline.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <utility>

namespace {
bool IsSameAuthoredComponent(
    const EffectComponentCommon* source,
    const EffectComponentCommon* destination) {
    if (source == nullptr || destination == nullptr) {
        return false;
    }
    if (source->id != 0 && source->id == destination->id) {
        return true;
    }
    return source->name == destination->name && source->type == destination->type;
}

void ApplyLiveTuningToComponent(
    const ParticleComponentAssetView& source,
    EffectParticleSettings& destination) {
    destination.depthFadeSoftness = source.settings->depthFadeSoftness;
    destination.edgeSoftness = source.settings->edgeSoftness;
}

void ApplyLiveTuningToComponent(
    const TrailComponentAssetView& source,
    EffectTrailSettings& destination) {
    destination.depthFadeSoftness = source.settings->depthFadeSoftness;
    destination.trailTailFade = source.settings->trailTailFade;
    destination.length = source.settings->length;
    destination.width = source.settings->width;
    destination.sampleDistance = source.settings->sampleDistance;
    destination.smoothing = source.settings->smoothing;
    destination.widthHead = source.settings->widthHead;
    destination.widthTail = source.settings->widthTail;
    destination.alphaTail = source.settings->alphaTail;
    destination.miterLimit = source.settings->miterLimit;
    destination.colorTail = source.settings->colorTail;
    destination.followMode = source.settings->followMode;
    destination.segmentBudget = source.settings->segmentBudget;
}

void ApplyLiveTuningToComponent(
    const DistortionComponentAssetView& source,
    EffectDistortionSettings& destination) {
    destination.depthFadeSoftness = source.settings->depthFadeSoftness;
    destination.depthAttenuation = source.settings->depthAttenuation;
}

void PreserveParticleLiveTuning(
    const EffectAsset& currentAsset,
    EffectAsset& reloadedAsset) {
    std::vector<ParticleComponentAsset> replacements;
    ForEachParticleComponent(reloadedAsset.Components().ParticleStorageView(), [&currentAsset, &replacements](const ParticleComponentAssetView& reloadedParticle) {
        bool applied = false;
        ForEachParticleComponent(currentAsset.Components().ParticleStorageView(), [&applied, &reloadedParticle, &replacements](const ParticleComponentAssetView& currentParticle) {
            if (applied) {
                return;
            }
            if (IsSameAuthoredComponent(currentParticle.common, reloadedParticle.common)) {
                ParticleComponentAsset replacement{*reloadedParticle.common, *reloadedParticle.settings};
                ApplyLiveTuningToComponent(currentParticle, replacement.settings);
                replacements.push_back(replacement);
                applied = true;
            }
        });
    });

    const MutableParticleComponentStorageView storage = reloadedAsset.MutableComponents().MutableParticleStorageView();
    for (const ParticleComponentAsset& replacement : replacements) {
        ReplaceParticleComponentAndSyncPacked(storage, replacement);
    }
}

void PreserveTrailLiveTuning(
    const EffectAsset& currentAsset,
    EffectAsset& reloadedAsset) {
    std::vector<TrailComponentAsset> replacements;
    ForEachTrailComponent(reloadedAsset.Components().TrailStorageView(), [&currentAsset, &replacements](const TrailComponentAssetView& reloadedTrail) {
        bool applied = false;
        ForEachTrailComponent(currentAsset.Components().TrailStorageView(), [&applied, &reloadedTrail, &replacements](const TrailComponentAssetView& currentTrail) {
            if (applied) {
                return;
            }
            if (IsSameAuthoredComponent(currentTrail.common, reloadedTrail.common)) {
                TrailComponentAsset replacement{*reloadedTrail.common, *reloadedTrail.settings};
                ApplyLiveTuningToComponent(currentTrail, replacement.settings);
                replacements.push_back(replacement);
                applied = true;
            }
        });
    });

    const MutableTrailComponentStorageView storage = reloadedAsset.MutableComponents().MutableTrailStorageView();
    for (const TrailComponentAsset& replacement : replacements) {
        ReplaceTrailComponentAndSyncPacked(storage, replacement);
    }
}

void PreserveDistortionLiveTuning(
    const EffectAsset& currentAsset,
    EffectAsset& reloadedAsset) {
    std::vector<DistortionComponentAsset> replacements;
    ForEachDistortionComponent(reloadedAsset.Components().DistortionStorageView(), [&currentAsset, &replacements](const DistortionComponentAssetView& reloadedDistortion) {
        bool applied = false;
        ForEachDistortionComponent(currentAsset.Components().DistortionStorageView(), [&applied, &reloadedDistortion, &replacements](const DistortionComponentAssetView& currentDistortion) {
            if (applied) {
                return;
            }
            if (IsSameAuthoredComponent(currentDistortion.common, reloadedDistortion.common)) {
                DistortionComponentAsset replacement{*reloadedDistortion.common, *reloadedDistortion.settings};
                ApplyLiveTuningToComponent(currentDistortion, replacement.settings);
                replacements.push_back(replacement);
                applied = true;
            }
        });
    });

    const MutableDistortionComponentStorageView storage = reloadedAsset.MutableComponents().MutableDistortionStorageView();
    for (const DistortionComponentAsset& replacement : replacements) {
        ReplaceDistortionComponentAndSyncPacked(storage, replacement);
    }
}

void PreserveLiveTuning(
    const EffectAsset& currentAsset,
    EffectAsset& reloadedAsset) {
    reloadedAsset.defaultParticle = currentAsset.defaultParticle;
    reloadedAsset.defaultTrail = currentAsset.defaultTrail;
    reloadedAsset.defaultBeam = currentAsset.defaultBeam;
    reloadedAsset.defaultDistortion = currentAsset.defaultDistortion;

    PreserveParticleLiveTuning(currentAsset, reloadedAsset);
    PreserveTrailLiveTuning(currentAsset, reloadedAsset);
    PreserveDistortionLiveTuning(currentAsset, reloadedAsset);
}
} // namespace

VfxEngine::VfxEngine() {
    postProcessStack_.ResetToVfxDefaults();
    RegisterBuiltInAssets();
    effectRuntime_.AttachSystem(&effectSystem_);
    effectRuntime_.AttachAuthoringRegistry(&effectAuthoringRegistry_);
    LoadEffectDirectory("Resources/effects");
}

void VfxEngine::RegisterBuiltInAssets() {
    EffectAsset additiveParticle{};
    additiveParticle.name = "particle_additive";
    additiveParticle.shader = "Particle";
    additiveParticle.texture = "default";
    additiveParticle.passState.blend = ge3::graphics::BlendMode::Additive;
    additiveParticle.passState.depth = ge3::graphics::DepthMode::ReadOnly;
    additiveParticle.layer = EffectLayer::AdditiveFx;
    additiveParticle.lifetime = 2.0f;
    additiveParticle.defaultParticle.emissive = 1.5f;
    additiveParticle.defaultBeam.emissive = additiveParticle.defaultParticle.emissive;
    effectSystem_.RegisterAsset(std::move(additiveParticle), effectAuthoringRegistry_);
}

void VfxEngine::LoadEffectDirectory(const char* directory) {
    loadedEffectAssets_ = effectAssetLoader_.LoadDirectory(
        directory,
        effectAuthoringRegistry_);
    for (const LoadedEffectAsset& loaded : loadedEffectAssets_) {
        effectSystem_.RegisterAsset(loaded.asset, effectAuthoringRegistry_);
    }
}

void VfxEngine::InitializeBeam(
    ID3D12Device* device,
    ID3D12DescriptorHeap* srvDescriptorHeap,
    uint32_t descriptorSizeSRV,
    D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandle,
    D3D12_CPU_DESCRIPTOR_HANDLE secondaryTextureSrvHandle,
    DXGI_FORMAT rtvFormat,
    DXGI_FORMAT dsvFormat) {
    beam_.Initialize(
        device,
        srvDescriptorHeap,
        descriptorSizeSRV,
        textureSrvHandle,
        secondaryTextureSrvHandle,
        rtvFormat,
        dsvFormat);
}

void VfxEngine::InitializeGpuParticles(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* commandList,
    ge3::core::DescriptorHeapSet& heaps) {
    gpuParticleSystem_.Initialize(device, commandList, heaps);
}

void VfxEngine::Shutdown() {
    beam_.Shutdown();
}

void VfxEngine::BeginFrame() {
    effectResourceCache_.BeginFrame();
}

void VfxEngine::Update(AppVfxRuntimeState& runtimeState, float deltaTime) {
    if (runtimeState.autoPlayVfxDemo) {
        runtimeState.enableParticles = true;
        runtimeState.autoPlayVfxTimer -= deltaTime;
        runtimeState.autoPlayVfxAngle += 0.9f * deltaTime;
        if (runtimeState.autoPlayVfxTimer <= 0.0f) {
            const float radius = (std::max)(0.0f, runtimeState.autoPlayVfxRadius);
            const float angle = runtimeState.autoPlayVfxAngle;
            const Vector3 effectPosition = {
                std::cos(angle) * radius,
                std::sin(angle * 1.7f) * 0.65f,
                std::sin(angle) * radius
            };
            effectRuntime_.PlayEffectWithParams(
                "warp_core",
                effectPosition,
                {1.0f, 0.8f, 0.45f, 1.0f},
                {1.15f, 1.15f, 1.15f});
            runtimeState.autoPlayVfxTimer = (std::max)(0.1f, runtimeState.autoPlayVfxInterval);
        }
    }

    beamTime_ += deltaTime;
    beam_.SetTime(beamTime_);
    effectRuntime_.Update(deltaTime);
    ReloadChangedEffectAssets();
}

void VfxEngine::ReloadChangedEffectAssets() {
    for (LoadedEffectAsset& loaded : loadedEffectAssets_) {
        if (!std::filesystem::exists(loaded.path)) {
            continue;
        }

        const std::filesystem::file_time_type lastWriteTime =
            std::filesystem::last_write_time(loaded.path);
        if (lastWriteTime == loaded.lastWriteTime) {
            continue;
        }

        LoadedEffectAsset reloaded{};
        if (effectAssetLoader_.LoadFile(
                loaded.path,
                reloaded,
                effectAuthoringRegistry_)) {
            if (const EffectAsset* currentAsset = effectSystem_.FindAsset(reloaded.asset.name)) {
                PreserveLiveTuning(*currentAsset, reloaded.asset);
            }
            effectSystem_.RegisterAsset(reloaded.asset, effectAuthoringRegistry_);
            loaded = std::move(reloaded);
        }
    }
}

void VfxEngine::RegisterDefaultTextures(const AppSceneResources& scene) {
    effectResourceCache_.RegisterTexture({"default", scene.textureSrvHandleCPU, scene.textureSrvHandleGPU, 1, 1});
    effectResourceCache_.RegisterTexture({"monsterBall", scene.textureSrvHandleCPU2, scene.textureSrvHandleGPU2, 1, 1});
    effectResourceCache_.RegisterTexture({"streakNoise", scene.textureSrvHandleCPU, scene.textureSrvHandleGPU, 1, 1});
}

void VfxEngine::RegisterRenderPasses(
    const AppFrameGraphBuilder& frameGraphBuilder,
    AppFrameGraphBuildContext graphContext,
    ID3D12Device* device,
    ID3D12GraphicsCommandList* commandList,
    const AppSceneResources& scene,
    D3D12_GPU_DESCRIPTOR_HANDLE spriteTextureHandle) {
    RegisterDefaultTextures(scene);

    frameGraphEffectRuntimeFrame_ = BuildFrame();
    const ParticleRenderQueue& particleQueue = frameGraphEffectRuntimeFrame_.particleQueue;
    const ParticleRenderFallback primaryParticleFx =
        !particleQueue.empty() ? frameGraphEffectRuntimeFrame_.PrimaryParticleFallback()
                               : FindPrimaryParticleFallback();
    const D3D12_GPU_DESCRIPTOR_HANDLE vfxTextureHandle =
        primaryParticleFx.common != nullptr
            ? ResolveTexture(primaryParticleFx.common->texture, spriteTextureHandle)
            : spriteTextureHandle;

    graphContext.vfxRenderTargets = &vfxRenderTargets_;
    graphContext.gpuParticleSystem = &gpuParticleSystem_;
    graphContext.vfxRenderers = &vfxRenderers_;
    graphContext.postProcessStack = &postProcessStack_;
    graphContext.spriteTextureHandle = spriteTextureHandle;
    graphContext.vfxTextureHandle = vfxTextureHandle;
    graphContext.effectRuntime = &frameGraphEffectRuntimeFrame_;
    graphContext.primaryParticleFx = primaryParticleFx;
    graphContext.beamTime = beamTime_;

    frameGraphBuilder.Build(
        graphContext,
        [this](const AppFrameGraphBuildContext& context) {
            AppVfxRenderPipeline{}.RegisterPasses(
                context,
                resourceResolver_.SelectPassResources(context.runtimeState->vfx));
        });
    gpuParticleSystem_.EnsureGraphBuffers(device, *graphContext.renderGraph);
    gpuParticleSystem_.InitializeDedicatedParticleResources(commandList);
}

VfxGraphResourceStats VfxEngine::PrepareGraphResources(
    ID3D12Device* device,
    ge3::core::DescriptorHeapSet& heaps,
    ge3::resources::ResourceRegistry& resourceRegistry,
    ge3::graphics::RenderGraph& renderGraph,
    uint32_t width,
    uint32_t height) {
    const std::vector<ge3::graphics::TransientRenderTargetDesc> transientRenderTargetPlan =
        renderGraph.BuildTransientRenderTargetPlan();
    const std::vector<ge3::graphics::TransientBufferDesc> transientBufferPlan =
        renderGraph.BuildTransientBufferPlan();

    std::unordered_set<std::string> transientTargetStorages;
    std::unordered_set<std::string> transientBufferStorages;
    for (const auto& target : transientRenderTargetPlan) {
        if (target.transient) {
            transientTargetStorages.insert(target.storageName);
        }
    }
    for (const auto& buffer : transientBufferPlan) {
        if (buffer.transient) {
            transientBufferStorages.insert(buffer.storageName);
        }
    }

    vfxRenderTargets_.ResetRequests();
    for (const auto& renderTarget : transientRenderTargetPlan) {
        if (renderTarget.transient) {
            vfxRenderTargets_.RequestTransientTarget(
                renderTarget,
                renderTarget.clearColor,
                renderTarget.initialState,
                renderTarget.name.find("PostColor") == 0);
            continue;
        }

        vfxRenderTargets_.RequestTarget(
            renderTarget.name,
            renderTarget.resolutionScale,
            renderTarget.format,
            renderTarget.clearColor,
            renderTarget.initialState);
    }

    vfxRenderTargets_.Initialize(device, heaps, width, height);
    vfxRenderTargets_.Register(resourceRegistry);

    return {
        static_cast<uint32_t>(std::count_if(
            transientRenderTargetPlan.begin(),
            transientRenderTargetPlan.end(),
            [](const auto& target) { return target.transient; })),
        static_cast<uint32_t>(transientTargetStorages.size()),
        static_cast<uint32_t>(std::count_if(
            transientBufferPlan.begin(),
            transientBufferPlan.end(),
            [](const auto& buffer) { return buffer.transient; })),
        static_cast<uint32_t>(transientBufferStorages.size())
    };
}

void VfxEngine::BeginScene(
    ID3D12GraphicsCommandList* commandList,
    D3D12_CPU_DESCRIPTOR_HANDLE dsv) {
    vfxRenderTargets_.BeginScene(commandList, dsv);
}

void VfxEngine::RegisterGraphResources(
    ge3::graphics::RenderGraph& renderGraph,
    D3D12_CPU_DESCRIPTOR_HANDLE dsv,
    std::function<void(std::string_view, D3D12_RESOURCE_STATES)> onResourceStateChanged) {
    vfxRenderTargets_.RegisterGraphResources(renderGraph);
    vfxRenderTargets_.RegisterDepthBinding(renderGraph, dsv);
    gpuParticleSystem_.RegisterGraphResources(renderGraph);
    renderGraph.SetResourceStateChangedCallback(
        [this, onResourceStateChanged](std::string_view name, D3D12_RESOURCE_STATES state) {
            if (onResourceStateChanged) {
                onResourceStateChanged(name, state);
            }
            vfxRenderTargets_.SetResourceState(name, state);
            gpuParticleSystem_.SetResourceState(name, state);
        });
}

void VfxEngine::CaptureFrameTelemetry(ID3D12GraphicsCommandList* commandList) {
    gpuParticleSystem_.CaptureTrailMeshStreamTelemetry(commandList);
    gpuParticleSystem_.CaptureParticleDedicatedReadbackTelemetry(commandList);
    gpuParticleSystem_.CaptureDistortionDedicatedReadbackTelemetry(commandList);
    gpuParticleSystem_.CaptureBeamDedicatedReadbackTelemetry(commandList);
}

void VfxEngine::ResolveFrameTelemetry() {
    gpuParticleSystem_.ResolveTrailMeshStreamTelemetry();
    gpuParticleSystem_.ResolveParticleDedicatedReadbackTelemetry();
    gpuParticleSystem_.ResolveDistortionDedicatedReadbackTelemetry();
    gpuParticleSystem_.ResolveBeamDedicatedReadbackTelemetry();
}

EffectRuntimeFrame VfxEngine::BuildFrame() const {
    return effectRuntime_.BuildFrame();
}

ParticleRenderFallback VfxEngine::FindPrimaryParticleFallback() const {
    return effectRuntime_.FindPrimaryParticleFallback();
}

D3D12_GPU_DESCRIPTOR_HANDLE VfxEngine::ResolveTexture(
    std::string_view textureName,
    D3D12_GPU_DESCRIPTOR_HANDLE fallback) const {
    return effectResourceCache_.ResolveTexture(textureName, fallback);
}
