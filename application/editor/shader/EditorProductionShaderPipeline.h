#pragma once

#include "../material/EditorProductionMaterialPipeline.h"
#include "../texture/EditorProductionTexturePipeline.h"

#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>

#include <cstdint>
#include <filesystem>
#include <future>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace editor {

struct EditorProductionShaderVariantKey {
    uint64_t graphSourceFingerprint = 0;
    uint64_t materialVariantHash = 0;
    uint64_t pipelineContractHash = 0;
    EditorMaterialDomain domain = EditorMaterialDomain::Surface;
    EditorMaterialBlendMode blendMode = EditorMaterialBlendMode::Opaque;
    EditorMaterialShadingModel shadingModel = EditorMaterialShadingModel::Lit;
    bool normalMap = false;

    uint64_t Hash() const noexcept;
    std::string StableName() const;
};

struct EditorProductionShaderBinding {
    std::string entityGuid;
    std::string materialAssetGuid;
    uint32_t materialSlot = 0;
    uint64_t variantHash = 0;
    ID3D12PipelineState* pipelineState = nullptr;
    bool fallback = true;
    bool lastKnownGood = false;
    bool normalMapEnabled = false;
};

struct EditorProductionShaderPipelinePolicy {
    uint32_t maxResidentVariants = 128;
    uint64_t inactiveFrameRetention = 300;
    std::filesystem::path generatedSourceRoot = "cache/shader_variants/generated";
    std::filesystem::path pipelineLibraryPath = "cache/shader_variants/production.pso";
};

struct EditorProductionShaderPipelineStats {
    uint32_t requestedBindings = 0;
    uint32_t residentVariants = 0;
    uint32_t queuedCompiles = 0;
    uint32_t readyBindings = 0;
    uint32_t fallbackBindings = 0;
    uint32_t lastKnownGoodBindings = 0;
    uint32_t normalMapBindings = 0;
    uint32_t failedVariants = 0;
    uint32_t pendingGpuRetirements = 0;
    uint64_t memoryCacheHits = 0;
    uint64_t memoryCacheMisses = 0;
    uint64_t pipelineLibraryHits = 0;
    uint64_t pipelineLibraryMisses = 0;
    uint64_t compilesStarted = 0;
    uint64_t compilesCompleted = 0;
    uint64_t compilesFailed = 0;
    uint64_t hotReloads = 0;
};

// E-9 renderer-owned shader permutation and PSO cache. Generated Material
// Graph code is compiled off the render thread; PSO creation, cache lookup,
// swapping, and lifetime retirement stay on the frame thread.
class EditorProductionShaderPipeline {
public:
    bool Initialize(
        ID3D12Device* device,
        ID3D12RootSignature* rootSignature,
        ID3D12PipelineState* fallbackPipelineState,
        EditorProductionShaderPipelinePolicy policy = {},
        std::string* errorMessage = nullptr);
    void Shutdown();

    bool Sync(
        const EditorProductionMaterialPipeline& materials,
        const EditorProductionTexturePipeline& textures,
        uint64_t completedFenceValue,
        uint64_t scheduledFenceValue,
        std::string* errorMessage = nullptr);

    const EditorProductionShaderBinding* Resolve(
        std::string_view entityGuid,
        uint32_t materialSlot) const;
    const EditorProductionShaderPipelineStats& Stats() const noexcept { return stats_; }
    const std::vector<std::string>& Diagnostics() const noexcept { return diagnostics_; }

    static EditorProductionShaderVariantKey MakeVariantKey(
        const EditorProductionMaterialShaderSource& source,
        bool normalMap,
        uint64_t pipelineContractHash = 0);
    static bool BuildGeneratedPixelShaderSource(
        const EditorProductionMaterialShaderSource& source,
        const EditorProductionShaderVariantKey& key,
        std::string_view shaderTemplate,
        std::string& output,
        std::string* errorMessage = nullptr);

private:
    struct CompileCandidate {
        Microsoft::WRL::ComPtr<IDxcBlob> bytecode;
        std::string error;
    };
    struct CompileJob {
        EditorProductionShaderVariantKey key;
        std::filesystem::path sourcePath;
        std::future<CompileCandidate> future;
    };
    struct ResidentVariant {
        EditorProductionShaderVariantKey key;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
        uint64_t lastUsedFrame = 0;
    };
    struct PendingPipeline {
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
        uint64_t retireFenceValue = 0;
    };

    bool QueueCompile(
        const EditorProductionMaterialShaderSource& source,
        const EditorProductionShaderVariantKey& key,
        std::string* errorMessage);
    void FinalizeCompletedJobs();
    Microsoft::WRL::ComPtr<ID3D12PipelineState> CreateVariantPipeline(
        const EditorProductionShaderVariantKey& key,
        IDxcBlob* pixelShader,
        bool& libraryHit,
        std::string& errorMessage);
    void CollectRetired(uint64_t completedFenceValue);
    void EvictInactive(
        const std::unordered_set<uint64_t>& activeVariants,
        uint64_t scheduledFenceValue);
    bool LoadPipelineLibrary();
    void SavePipelineLibrary();

    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> fallbackPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineLibrary> pipelineLibrary_;
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShader_;
    EditorProductionShaderPipelinePolicy policy_{};
    uint64_t pipelineContractHash_ = 0;
    uint64_t frameIndex_ = 0;
    std::string shaderTemplate_;
    std::unordered_map<uint64_t, CompileJob> compileJobs_;
    std::unordered_map<uint64_t, ResidentVariant> variants_;
    std::unordered_map<std::string, uint64_t> lastKnownGoodByMaterial_;
    std::unordered_set<uint64_t> failedVariants_;
    std::vector<PendingPipeline> pendingPipelines_;
    std::vector<EditorProductionShaderBinding> bindings_;
    EditorProductionShaderPipelineStats stats_{};
    std::vector<std::string> diagnostics_;
};

} // namespace editor
