#include "EnemyProjectileVfxRenderer.h"

#include "../EffectRuntime.h"
#include "../diagnostics/DebugDrawSystem.h"

#include <algorithm>
#include <cmath>
#include <system_error>
#include <unordered_set>

namespace {
Vector3 Add(Vector3 a, Vector3 b) noexcept {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
Vector3 Subtract(Vector3 a, Vector3 b) noexcept {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
Vector3 Scale(Vector3 value, float scale) noexcept {
    return {value.x * scale, value.y * scale, value.z * scale};
}
float Length(Vector3 value) noexcept {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}
Vector3 NormalizeOr(Vector3 value, Vector3 fallback) noexcept {
    const float length = Length(value);
    return length > 0.00001f ? Scale(value, 1.0f / length) : fallback;
}

void UpdateEffectInstance(
    EffectRuntime& runtime,
    uint32_t instanceId,
    const Vector3& position,
    const Vector4& color,
    float radius) {
    EffectInstance* instance = runtime.FindInstance(instanceId);
    if (instance == nullptr) return;
    instance->transform.translate = position;
    instance->transform.scale = {radius, radius, radius};
    instance->color = color;
    instance->attached = true;
    instance->previewLoop = true;
}

bool IsRenderableEffectInstance(
    const EffectRuntime* runtime,
    uint32_t instanceId) {
    if (runtime == nullptr || instanceId == 0) return false;
    const EffectInstance* instance = runtime->FindInstance(instanceId);
    if (instance == nullptr || instance->asset == nullptr) return false;

    for (const EffectComponentInstance& componentInstance : instance->components) {
        if (!componentInstance.active) continue;
        bool renderable = false;
        instance->asset->Components().ForEachComponentCommon(
            [&](const EffectComponentCommon& common) {
                if (renderable || common.id != componentInstance.componentId ||
                    common.duration <= 0.0f) {
                    return;
                }
                if (common.type != EffectComponentType::Particle) {
                    renderable = true;
                    return;
                }
                const ParticleComponentAssetView particle =
                    FindParticleComponent(
                        instance->asset->Components().ParticleStorageView(),
                        common.id);
                renderable = particle && particle.settings->spawnCount > 0.0f;
            });
        if (renderable) return true;
    }
    return false;
}

void AppendProjectilePrimitive(
    const EnemyProjectileVfxProxy& proxy,
    ge3::debug::DebugDrawSystem& draw) {
    draw.AddLine(
        proxy.trailStart,
        proxy.worldPosition,
        proxy.trailColor,
        proxy.haloColor);
    draw.AddPoint(
        proxy.worldPosition,
        proxy.coreRadius,
        proxy.coreColor);
    draw.AddCircle(
        proxy.worldPosition,
        proxy.cameraRight,
        proxy.cameraUp,
        proxy.haloRadius,
        proxy.haloColor,
        24);
    draw.AddCircle(
        proxy.worldPosition,
        proxy.cameraRight,
        proxy.cameraUp,
        proxy.coreRadius,
        proxy.coreColor,
        18);
}
} // namespace

EnemyProjectileVfxRenderer::EnemyProjectileVfxRenderer()
    : fallbackDirect_(EnemyProjectileVisualDefinitionAsset::CommercialDefault(
          EnemyProjectileTrajectory::Direct)),
      fallbackPredictive_(EnemyProjectileVisualDefinitionAsset::CommercialDefault(
          EnemyProjectileTrajectory::Predictive)),
      fallbackHoming_(EnemyProjectileVisualDefinitionAsset::CommercialDefault(
          EnemyProjectileTrajectory::Homing)),
      fallbackArc_(EnemyProjectileVisualDefinitionAsset::CommercialDefault(
          EnemyProjectileTrajectory::Arc)) {}

bool EnemyProjectileVfxRenderer::LoadDirectory(
    const std::filesystem::path& directory,
    std::string* errorMessage) {
    std::error_code ec;
    if (!std::filesystem::exists(directory, ec) || ec ||
        !std::filesystem::is_directory(directory, ec) || ec) {
        if (errorMessage != nullptr) {
            *errorMessage = "Enemy projectile visual directory is unavailable: " +
                directory.generic_string();
        }
        return false;
    }

    std::vector<std::filesystem::path> files;
    for (std::filesystem::directory_iterator it(directory, ec), end;
         !ec && it != end;
         it.increment(ec)) {
        if (it->is_regular_file(ec) &&
            it->path().extension() == ".projectilevisual") {
            files.push_back(it->path());
        }
    }
    if (ec) {
        if (errorMessage != nullptr) {
            *errorMessage = "Could not enumerate projectile visual directory: " +
                ec.message();
        }
        return false;
    }
    std::sort(files.begin(), files.end());

    std::vector<EnemyProjectileVisualDefinitionAsset> staged;
    std::unordered_map<std::string, size_t> stagedLookup;
    for (const std::filesystem::path& file : files) {
        EnemyProjectileVisualDefinitionAsset asset{};
        std::string assetError;
        if (!asset.LoadFromFile(file.string(), &assetError)) {
            if (errorMessage != nullptr) *errorMessage = assetError;
            return false;
        }
        if (stagedLookup.contains(asset.projectileDefinitionId)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Duplicate projectile visual binding: " +
                    asset.projectileDefinitionId;
            }
            return false;
        }
        stagedLookup.emplace(asset.projectileDefinitionId, staged.size());
        staged.push_back(std::move(asset));
    }
    if (staged.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Projectile visual directory contains no assets.";
        }
        return false;
    }

    directory_ = directory;
    assets_ = std::move(staged);
    byProjectileDefinition_ = std::move(stagedLookup);
    ++assetRevision_;
    return true;
}

void EnemyProjectileVfxRenderer::Reset(EffectRuntime* effectRuntime) {
    if (effectRuntime != nullptr) {
        for (auto& [projectileId, managed] : managedEffects_) {
            (void)projectileId;
            StopManaged(managed, effectRuntime);
        }
    }
    managedEffects_.clear();
    frame_ = {};
    revision_ = 0;
}

void EnemyProjectileVfxRenderer::Update(
    const EnemyProjectileVfxRenderInput& input) {
    frame_ = {};
    frame_.assetRevision = assetRevision_;
    const uint64_t touchedRevision = ++revision_;
    if (!input.settings.enabled || !input.gameplayActive ||
        input.presentation == nullptr) {
        StopUntouched(input.effectRuntime, touchedRevision);
        frame_.revision = touchedRevision;
        return;
    }

    const Vector3 cameraRight = NormalizeOr(
        input.cameraRight, {1.0f, 0.0f, 0.0f});
    const Vector3 cameraUp = NormalizeOr(
        input.cameraUp, {0.0f, 1.0f, 0.0f});
    const float maximumDrawDistance = (std::max)(
        1.0f, input.settings.maximumDrawDistance);
    const size_t budget = (std::max)(
        static_cast<size_t>(1), input.settings.maximumVisibleProjectiles);
    frame_.proxies.reserve((std::min)(
        budget, input.presentation->projectiles.size()));

    for (const EnemyProjectilePresentation& projectile :
         input.presentation->projectiles) {
        const float cameraDistance = Length(Subtract(
            projectile.worldPosition, input.cameraWorldPosition));
        if (!std::isfinite(cameraDistance) ||
            cameraDistance > maximumDrawDistance) {
            ++frame_.culledByDistance;
            continue;
        }
        if (frame_.proxies.size() >= budget) {
            ++frame_.droppedByBudget;
            continue;
        }

        const EnemyProjectileVisualDefinitionAsset& visual =
            ResolveVisual(projectile);
        if (!visual.enabled) continue;
        const float pulse = 1.0f + visual.pulseAmplitude * std::sin(
            input.elapsedTime * visual.pulseFrequencyHz * 6.28318530718f +
            static_cast<float>(projectile.projectileId % 29u) * 0.21f);
        const float threatScale = projectile.threat
            ? visual.threatRadiusScale
            : 1.0f;
        const float physicalRadius = projectile.collisionRadius *
            visual.coreRadiusScale;
        const float angularRadius = cameraDistance *
            visual.minimumAngularRadius;
        const float authoredCoreRadius = (std::clamp)(
            (std::max)(physicalRadius, angularRadius) * pulse * threatScale,
            0.04f,
            visual.maximumWorldRadius);
        const float authoredHaloRadius = (std::min)(
            authoredCoreRadius * visual.haloRadiusScale,
            visual.maximumWorldRadius * visual.haloRadiusScale);
        EnemyProjectileScreenSpaceReadabilityInput readabilityInput{};
        readabilityInput.cameraDistance = cameraDistance;
        readabilityInput.verticalFovRadians = input.verticalFovRadians;
        readabilityInput.viewportHeightPixels = input.viewportHeightPixels;
        readabilityInput.authoredCoreRadius = authoredCoreRadius;
        readabilityInput.authoredHaloRadius = authoredHaloRadius;
        readabilityInput.authoredTrailWidth =
            authoredCoreRadius * visual.trailWidthScale;
        readabilityInput.threat = projectile.threat;
        readabilityInput.settings = input.readabilitySettings;
        const EnemyProjectileScreenSpaceReadabilityResult readability =
            EnemyProjectileScreenSpaceReadabilityPolicy{}.Evaluate(
                readabilityInput);
        const float coreRadius = readability.coreRadius;
        const float haloRadius = readability.haloRadius;

        EnemyProjectileVfxProxy proxy{};
        proxy.projectileId = projectile.projectileId;
        proxy.visualDefinitionId = visual.id;
        proxy.style = visual.style;
        proxy.worldPosition = projectile.worldPosition;
        proxy.cameraRight = cameraRight;
        proxy.cameraUp = cameraUp;
        proxy.coreColor = visual.coreColor;
        proxy.haloColor = visual.haloColor;
        proxy.trailColor = visual.trailColor;
        proxy.coreRadius = coreRadius;
        proxy.haloRadius = haloRadius;
        proxy.trailWidth = readability.trailWidth;
        proxy.distanceFromCamera = cameraDistance;
        proxy.coreDiameterPixels = readability.coreDiameterPixels;
        proxy.haloDiameterPixels = readability.haloDiameterPixels;
        proxy.threat = projectile.threat;
        proxy.readabilityBoosted = readability.boosted;
        proxy.readabilityLimitReached = readability.worldLimitReached;
        if (readability.boosted) ++frame_.readabilityBoostedProjectiles;
        if (readability.worldLimitReached) {
            ++frame_.readabilityLimitedProjectiles;
        }
        const Vector3 motionDirection = NormalizeOr(
            projectile.motionDirection, {0.0f, 0.0f, -1.0f});
        proxy.trailStart = Add(
            projectile.worldPosition,
            Scale(motionDirection, -coreRadius * visual.trailLengthInRadii));

        ManagedEffect& managed = managedEffects_[projectile.projectileId];
        if (managed.visualDefinitionId != visual.id) {
            StopManaged(managed, input.effectRuntime);
            managed.visualDefinitionId = visual.id;
        }
        managed.touchedRevision = touchedRevision;
        if (input.settings.effectRuntimeEnabled && input.effectRuntime != nullptr) {
            if (managed.coreInstanceId == 0) {
                managed.coreInstanceId = input.effectRuntime->PlayEffectWithParams(
                    visual.coreEffectId,
                    projectile.worldPosition,
                    visual.coreColor,
                    {coreRadius, coreRadius, coreRadius});
                if (managed.coreInstanceId != 0) {
                    input.effectRuntime->SetEffectPreviewLoop(
                        managed.coreInstanceId, true);
                }
            }
            if (managed.haloInstanceId == 0) {
                managed.haloInstanceId = input.effectRuntime->PlayEffectWithParams(
                    visual.haloEffectId,
                    projectile.worldPosition,
                    visual.haloColor,
                    {haloRadius, haloRadius, haloRadius});
                if (managed.haloInstanceId != 0) {
                    input.effectRuntime->SetEffectPreviewLoop(
                        managed.haloInstanceId, true);
                }
            }
            UpdateEffectInstance(
                *input.effectRuntime,
                managed.coreInstanceId,
                projectile.worldPosition,
                visual.coreColor,
                coreRadius);
            UpdateEffectInstance(
                *input.effectRuntime,
                managed.haloInstanceId,
                projectile.worldPosition,
                visual.haloColor,
                haloRadius);
        }
        proxy.coreEffectInstanceId = managed.coreInstanceId;
        proxy.haloEffectInstanceId = managed.haloInstanceId;
        proxy.effectBacked =
            IsRenderableEffectInstance(input.effectRuntime, managed.coreInstanceId) &&
            IsRenderableEffectInstance(input.effectRuntime, managed.haloInstanceId);
        if (proxy.effectBacked) {
            proxy.visualState = EnemyProjectileVisualState::ProductionEffectReady;
            ++frame_.effectBackedProjectiles;
        } else if (input.settings.productionPrimitivesEnabled ||
                   input.settings.fallbackPrimitivesEnabled) {
            proxy.visualState = EnemyProjectileVisualState::ProductionFallbackReady;
            ++frame_.fallbackProjectiles;
        } else {
            proxy.visualState = EnemyProjectileVisualState::Unavailable;
            ++frame_.unavailableProjectiles;
        }
        if (input.settings.productionPrimitivesEnabled ||
            (!proxy.effectBacked && input.settings.fallbackPrimitivesEnabled)) {
            ++frame_.productionSubmittedProjectiles;
        }
        frame_.proxies.push_back(std::move(proxy));
    }

    StopUntouched(input.effectRuntime, touchedRevision);
    frame_.sourcePresentationRevision = input.presentation->revision;
    frame_.revision = touchedRevision;
}

void EnemyProjectileVfxRenderer::AppendProductionWorldPrimitives(
    ge3::debug::DebugDrawSystem& productionDraw) const {
    for (const EnemyProjectileVfxProxy& proxy : frame_.proxies) {
        if (proxy.visualState == EnemyProjectileVisualState::Unavailable) continue;
        AppendProjectilePrimitive(proxy, productionDraw);
    }
}

void EnemyProjectileVfxRenderer::AppendFallbackWorldPrimitives(
    ge3::debug::DebugDrawSystem& debugDraw) const {
    for (const EnemyProjectileVfxProxy& proxy : frame_.proxies) {
        if (proxy.effectBacked) continue;
        AppendProjectilePrimitive(proxy, debugDraw);
    }
}

const char* ToString(EnemyProjectileVisualState state) noexcept {
    switch (state) {
    case EnemyProjectileVisualState::ProductionEffectReady:
        return "ProductionEffectReady";
    case EnemyProjectileVisualState::ProductionFallbackReady:
        return "ProductionFallbackReady";
    case EnemyProjectileVisualState::Unavailable:
        return "Unavailable";
    }
    return "Unknown";
}

const EnemyProjectileVisualDefinitionAsset*
EnemyProjectileVfxRenderer::FindVisual(
    const std::string& projectileDefinitionId) const noexcept {
    const auto found = byProjectileDefinition_.find(projectileDefinitionId);
    return found != byProjectileDefinition_.end()
        ? &assets_[found->second]
        : nullptr;
}

std::filesystem::path EnemyProjectileVfxRenderer::DefaultDirectory() {
    return std::filesystem::path{"Resources/courses/projectile_visuals"};
}

const EnemyProjectileVisualDefinitionAsset&
EnemyProjectileVfxRenderer::ResolveVisual(
    const EnemyProjectilePresentation& projectile) const noexcept {
    if (const EnemyProjectileVisualDefinitionAsset* resolved =
            FindVisual(projectile.definitionId)) {
        return *resolved;
    }
    switch (projectile.trajectory) {
    case EnemyProjectileTrajectory::Direct: return fallbackDirect_;
    case EnemyProjectileTrajectory::Predictive: return fallbackPredictive_;
    case EnemyProjectileTrajectory::Homing: return fallbackHoming_;
    case EnemyProjectileTrajectory::Arc: return fallbackArc_;
    }
    return fallbackDirect_;
}

void EnemyProjectileVfxRenderer::StopManaged(
    ManagedEffect& managed,
    EffectRuntime* runtime) {
    if (runtime != nullptr) {
        if (managed.coreInstanceId != 0) runtime->StopEffect(managed.coreInstanceId);
        if (managed.haloInstanceId != 0) runtime->StopEffect(managed.haloInstanceId);
    }
    managed.coreInstanceId = 0;
    managed.haloInstanceId = 0;
}

void EnemyProjectileVfxRenderer::StopUntouched(
    EffectRuntime* runtime,
    uint64_t touchedRevision) {
    for (auto it = managedEffects_.begin(); it != managedEffects_.end();) {
        if (it->second.touchedRevision == touchedRevision) {
            ++it;
            continue;
        }
        StopManaged(it->second, runtime);
        it = managedEffects_.erase(it);
    }
}
