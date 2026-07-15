#pragma once

#include "EditorMaterialGraph.h"
#include "../EditorAssetRegistry.h"
#include "../scene/EditorScene.h"

#include <d3d12.h>
#include <wrl.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "utils/math/MathUtils.h"

namespace editor {

inline constexpr uint32_t kEditorMaterialInstanceSchemaVersion = 1;

struct EditorMaterialInstanceAsset {
    uint32_t schemaVersion = kEditorMaterialInstanceSchemaVersion;
    std::string assetGuid;
    std::string parentMaterialGuid;
    std::string name;
    Vector4 baseColor{1.0f, 1.0f, 1.0f, 1.0f};
    float roughness = 0.5f;
    float metallic = 0.0f;
    float environmentCoefficient = 0.3f;
    std::string albedoTextureGuid;
    std::string normalTextureGuid;
    uint64_t revision = 0;
};

bool EncodeEditorMaterialInstance(
    const EditorMaterialInstanceAsset& asset,
    std::string& output,
    std::string* errorMessage = nullptr);
bool DecodeEditorMaterialInstance(
    std::string_view content,
    EditorMaterialInstanceAsset& output,
    std::string* errorMessage = nullptr);
bool LoadEditorMaterialInstance(
    const std::filesystem::path& path,
    EditorMaterialInstanceAsset& output,
    std::string* errorMessage = nullptr);

struct EditorProductionMaterialBinding {
    std::string entityGuid;
    std::string materialAssetGuid;
    std::string parentMaterialGuid;
    uint32_t materialSlot = 0;
    uint64_t shaderVariantHash = 0;
    D3D12_GPU_VIRTUAL_ADDRESS materialAddress = 0;
    std::string albedoTextureGuid;
    std::string normalTextureGuid;
    bool fallback = true;
};

// One immutable shader-authoring payload per resident Material Instance.  E-9
// consumes this separately from per-entity bindings so generated HLSL is not
// duplicated for every Scene Entity using the same material.
struct EditorProductionMaterialShaderSource {
    std::string materialAssetGuid;
    uint64_t shaderVariantHash = 0;
    uint64_t graphSourceFingerprint = 0;
    EditorMaterialDomain domain = EditorMaterialDomain::Surface;
    EditorMaterialBlendMode blendMode = EditorMaterialBlendMode::Opaque;
    EditorMaterialShadingModel shadingModel = EditorMaterialShadingModel::Lit;
    std::string graphHlslSource;
    std::vector<std::string> textureAssetGuids;
};

struct EditorProductionSceneLighting {
    DirectionalLight directional{};
    PointLight point{};
    SpotLight spot{};
    D3D12_GPU_VIRTUAL_ADDRESS directionalAddress = 0;
    D3D12_GPU_VIRTUAL_ADDRESS pointAddress = 0;
    D3D12_GPU_VIRTUAL_ADDRESS spotAddress = 0;
    uint32_t directionalCount = 0;
    uint32_t pointCount = 0;
    uint32_t spotCount = 0;
};

struct EditorProductionMaterialPipelineStats {
    uint32_t requestedBindings = 0;
    uint32_t resolvedBindings = 0;
    uint32_t fallbackBindings = 0;
    uint32_t residentMaterialInstances = 0;
    uint32_t residentShaderVariants = 0;
    uint32_t pendingGpuRetirements = 0;
    uint64_t hotReloads = 0;
    uint64_t residentGpuBytes = 0;
};

// E-7 transient bridge. Durable Material Instance data and Scene light
// components are resolved into frame-safe CBVs without leaking GPU state into
// the Scene document.
class EditorProductionMaterialPipeline {
public:
    bool Initialize(ID3D12Device* device, std::string* errorMessage = nullptr);
    void Shutdown();

    bool Sync(
        const EditorScene& scene,
        const EditorAssetRegistry& registry,
        uint64_t completedFenceValue,
        uint64_t scheduledFenceValue,
        std::string* errorMessage = nullptr);

    const EditorProductionMaterialBinding* Resolve(
        std::string_view entityGuid,
        uint32_t materialSlot) const;
    const std::vector<EditorProductionMaterialBinding>& Bindings() const noexcept {
        return bindings_;
    }
    const EditorProductionMaterialShaderSource* ResolveShaderSource(
        std::string_view materialAssetGuid) const;
    const EditorProductionSceneLighting& Lighting() const noexcept { return lighting_; }
    const EditorProductionMaterialPipelineStats& Stats() const noexcept { return stats_; }
    const std::vector<std::string>& Diagnostics() const noexcept { return diagnostics_; }

private:
    struct ResidentMaterial {
        EditorMaterialInstanceAsset asset;
        uint64_t sourceTimestamp = 0;
        uint64_t parentTimestamp = 0;
        uint64_t shaderVariantHash = 0;
        EditorProductionMaterialShaderSource shaderSource;
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        Material* mapped = nullptr;
    };
    struct PendingResource {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        uint64_t retireFenceValue = 0;
    };

    bool ResolveMaterial(
        const EditorAssetRecord& record,
        const EditorAssetRegistry& registry,
        uint64_t scheduledFenceValue,
        ResidentMaterial*& output,
        std::string* errorMessage);
    bool EnsureLightResources(std::string* errorMessage);
    void CollectRetired(uint64_t completedFenceValue);

    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    std::unordered_map<std::string, ResidentMaterial> materials_;
    std::vector<EditorProductionMaterialBinding> bindings_;
    std::vector<PendingResource> pendingResources_;
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> pointResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> spotResource_;
    DirectionalLight* mappedDirectional_ = nullptr;
    PointLight* mappedPoint_ = nullptr;
    SpotLight* mappedSpot_ = nullptr;
    EditorProductionSceneLighting lighting_{};
    EditorProductionMaterialPipelineStats stats_{};
    std::vector<std::string> diagnostics_;
};

} // namespace editor
