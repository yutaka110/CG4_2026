#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "EnemyProjectilePresentationBridge.h"
#include "EnemyProjectileVisualDefinitionAsset.h"

class EffectRuntime;
namespace ge3::debug { class DebugDrawSystem; }

struct EnemyProjectileVfxRendererSettings final {
    bool enabled = true;
    bool effectRuntimeEnabled = true;
    bool fallbackPrimitivesEnabled = true;
    size_t maximumVisibleProjectiles = 256;
    float maximumDrawDistance = 900.0f;
};

struct EnemyProjectileVfxProxy final {
    uint64_t projectileId = 0;
    std::string visualDefinitionId;
    EnemyProjectileVisualStyle style = EnemyProjectileVisualStyle::Bolt;
    Vector3 worldPosition{};
    Vector3 trailStart{};
    Vector3 cameraRight{1.0f, 0.0f, 0.0f};
    Vector3 cameraUp{0.0f, 1.0f, 0.0f};
    Vector4 coreColor{1.0f, 1.0f, 1.0f, 1.0f};
    Vector4 haloColor{1.0f, 0.08f, 0.72f, 0.86f};
    Vector4 trailColor{1.0f, 0.05f, 0.58f, 0.62f};
    float coreRadius = 0.5f;
    float haloRadius = 1.0f;
    float trailWidth = 0.2f;
    float distanceFromCamera = 0.0f;
    uint32_t coreEffectInstanceId = 0;
    uint32_t haloEffectInstanceId = 0;
    bool threat = false;
    bool effectBacked = false;
};

struct EnemyProjectileVfxRenderFrame final {
    std::vector<EnemyProjectileVfxProxy> proxies;
    uint32_t effectBackedProjectiles = 0;
    uint32_t fallbackProjectiles = 0;
    uint32_t culledByDistance = 0;
    uint32_t droppedByBudget = 0;
    uint64_t sourcePresentationRevision = 0;
    uint64_t assetRevision = 0;
    uint64_t revision = 0;
};

struct EnemyProjectileVfxRenderInput final {
    const EnemyProjectilePresentationFrame* presentation = nullptr;
    EffectRuntime* effectRuntime = nullptr;
    Vector3 cameraWorldPosition{};
    Vector3 cameraRight{1.0f, 0.0f, 0.0f};
    Vector3 cameraUp{0.0f, 1.0f, 0.0f};
    float elapsedTime = 0.0f;
    bool gameplayActive = true;
    EnemyProjectileVfxRendererSettings settings{};
};

// Production hostile-projectile presentation. It resolves a visual asset by
// projectile definition ID, owns moving EffectRuntime instances, and retains
// a bounded primitive fallback so missing packaged effects never make an
// attack invisible in Release.
class EnemyProjectileVfxRenderer final {
public:
    EnemyProjectileVfxRenderer();

    bool LoadDirectory(
        const std::filesystem::path& directory,
        std::string* errorMessage = nullptr);
    void Reset(EffectRuntime* effectRuntime = nullptr);
    void Update(const EnemyProjectileVfxRenderInput& input);
    void AppendFallbackWorldPrimitives(
        ge3::debug::DebugDrawSystem& debugDraw) const;

    const EnemyProjectileVfxRenderFrame& Frame() const noexcept {
        return frame_;
    }
    const std::vector<EnemyProjectileVisualDefinitionAsset>& Assets() const noexcept {
        return assets_;
    }
    const EnemyProjectileVisualDefinitionAsset* FindVisual(
        const std::string& projectileDefinitionId) const noexcept;
    uint64_t AssetRevision() const noexcept { return assetRevision_; }
    const std::filesystem::path& Directory() const noexcept { return directory_; }

    static std::filesystem::path DefaultDirectory();

private:
    struct ManagedEffect final {
        uint32_t coreInstanceId = 0;
        uint32_t haloInstanceId = 0;
        std::string visualDefinitionId;
        uint64_t touchedRevision = 0;
    };

    const EnemyProjectileVisualDefinitionAsset& ResolveVisual(
        const EnemyProjectilePresentation& projectile) const noexcept;
    void StopManaged(ManagedEffect& managed, EffectRuntime* runtime);
    void StopUntouched(EffectRuntime* runtime, uint64_t touchedRevision);

    std::filesystem::path directory_;
    std::vector<EnemyProjectileVisualDefinitionAsset> assets_;
    std::unordered_map<std::string, size_t> byProjectileDefinition_;
    EnemyProjectileVisualDefinitionAsset fallbackDirect_{};
    EnemyProjectileVisualDefinitionAsset fallbackPredictive_{};
    EnemyProjectileVisualDefinitionAsset fallbackHoming_{};
    EnemyProjectileVisualDefinitionAsset fallbackArc_{};
    std::unordered_map<uint64_t, ManagedEffect> managedEffects_;
    EnemyProjectileVfxRenderFrame frame_{};
    uint64_t assetRevision_ = 0;
    uint64_t revision_ = 0;
};
