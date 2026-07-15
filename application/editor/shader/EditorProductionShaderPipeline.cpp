#include "EditorProductionShaderPipeline.h"

#include "core/ShaderCompiler.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

namespace editor {
namespace {

constexpr uint64_t kFnvOffset = 1469598103934665603ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;

uint64_t HashBytes(uint64_t hash, const void* bytes, size_t size) {
    const auto* data = static_cast<const uint8_t*>(bytes);
    for (size_t index = 0; index < size; ++index) {
        hash ^= data[index];
        hash *= kFnvPrime;
    }
    return hash;
}

uint64_t HashText(std::string_view text, uint64_t seed = kFnvOffset) {
    return HashBytes(seed, text.data(), text.size());
}

template <typename T>
uint64_t HashValue(uint64_t hash, const T& value) {
    return HashBytes(hash, &value, sizeof(value));
}

void SetError(std::string* output, std::string message) {
    if (output != nullptr) *output = std::move(message);
}

std::filesystem::path ResolveResource(std::string_view name) {
    const std::array<std::filesystem::path, 4> candidates = {
        std::filesystem::path("Resources") / name,
        std::filesystem::path("resources") / name,
        std::filesystem::path("../Resources") / name,
        std::filesystem::path("../../Resources") / name,
    };
    std::error_code error;
    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate, error)) {
            return std::filesystem::weakly_canonical(candidate, error);
        }
        error.clear();
    }
    return candidates.front();
}

bool ReadText(const std::filesystem::path& path, std::string& output) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    output.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return input.good() || input.eof();
}

bool WriteIfChanged(const std::filesystem::path& path, std::string_view source) {
    std::string existing;
    if (ReadText(path, existing) && existing == source) return true;
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) return false;
    const std::filesystem::path temporary = path.string() + ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) return false;
        output.write(source.data(), static_cast<std::streamsize>(source.size()));
        if (!output) return false;
    }
    std::filesystem::rename(temporary, path, error);
    if (!error) return true;
    error.clear();
    std::filesystem::remove(path, error);
    error.clear();
    std::filesystem::rename(temporary, path, error);
    return !error;
}

std::string Hex(uint64_t value) {
    std::ostringstream stream;
    stream << std::hex << std::setw(16) << std::setfill('0') << value;
    return stream.str();
}

std::string NormalizeGraphSource(std::string_view source) {
    std::istringstream input{std::string(source)};
    std::ostringstream output;
    std::string line;
    while (std::getline(input, line)) {
        const size_t texture = line.find("Texture2D MG_Texture_");
        if (texture != std::string::npos) {
            const size_t nameStart = line.find("MG_Texture_", texture);
            const size_t nameEnd = line.find(';', nameStart);
            if (nameStart != std::string::npos && nameEnd != std::string::npos) {
                output << "#define " << line.substr(nameStart, nameEnd - nameStart)
                       << " gTexture\n";
                continue;
            }
        }
        if (line.find("SamplerState MG_LinearSampler") != std::string::npos) {
            output << "#define MG_LinearSampler gSampler\n";
            continue;
        }
        output << line << '\n';
    }
    return output.str();
}

} // namespace

uint64_t EditorProductionShaderVariantKey::Hash() const noexcept {
    uint64_t hash = kFnvOffset;
    hash = HashValue(hash, graphSourceFingerprint);
    hash = HashValue(hash, materialVariantHash);
    hash = HashValue(hash, pipelineContractHash);
    hash = HashValue(hash, domain);
    hash = HashValue(hash, blendMode);
    hash = HashValue(hash, shadingModel);
    hash = HashValue(hash, normalMap);
    return hash;
}

std::string EditorProductionShaderVariantKey::StableName() const {
    return "ge3_e9_" + Hex(Hash());
}

EditorProductionShaderVariantKey EditorProductionShaderPipeline::MakeVariantKey(
    const EditorProductionMaterialShaderSource& source,
    bool normalMap,
    uint64_t pipelineContractHash) {
    EditorProductionShaderVariantKey key{};
    key.graphSourceFingerprint = source.graphSourceFingerprint;
    key.materialVariantHash = source.shaderVariantHash;
    key.pipelineContractHash = pipelineContractHash;
    key.domain = source.domain;
    key.blendMode = source.blendMode;
    key.shadingModel = source.shadingModel;
    key.normalMap = normalMap;
    return key;
}

bool EditorProductionShaderPipeline::BuildGeneratedPixelShaderSource(
    const EditorProductionMaterialShaderSource& source,
    const EditorProductionShaderVariantKey& key,
    std::string_view shaderTemplate,
    std::string& output,
    std::string* errorMessage) {
    output.clear();
    if (source.graphHlslSource.empty()) {
        SetError(errorMessage, "Material Graph compile artifact has no generated HLSL source.");
        return false;
    }
    if (source.textureAssetGuids.size() > 1) {
        SetError(errorMessage,
            "E-9 currently supports one sampled Material Graph texture per Surface variant.");
        return false;
    }
    if (key.domain != EditorMaterialDomain::Surface) {
        SetError(errorMessage,
            "Post Process Material Domain requires the independent post-process graph pipeline.");
        return false;
    }
    constexpr std::string_view marker = "//__GE3_MATERIAL_GRAPH__";
    const size_t markerPosition = shaderTemplate.find(marker);
    if (markerPosition == std::string_view::npos) {
        SetError(errorMessage, "Production Material shader template marker is missing.");
        return false;
    }
    std::ostringstream generated;
    generated << "// Generated by E-9. Do not edit. Variant " << key.StableName() << "\n"
              << "#define GE3_VARIANT_NORMAL_MAP " << (key.normalMap ? 1 : 0) << "\n"
              << "#define GE3_VARIANT_UNLIT "
              << (key.shadingModel == EditorMaterialShadingModel::Unlit ? 1 : 0) << "\n"
              << "#define GE3_VARIANT_MASKED "
              << (key.blendMode == EditorMaterialBlendMode::Masked ? 1 : 0) << "\n"
              << "#define GE3_VARIANT_TRANSLUCENT "
              << (key.blendMode == EditorMaterialBlendMode::Translucent ? 1 : 0) << "\n";
    generated << shaderTemplate.substr(0, markerPosition);
    generated << NormalizeGraphSource(source.graphHlslSource);
    generated << shaderTemplate.substr(markerPosition + marker.size());
    output = generated.str();
    return true;
}

bool EditorProductionShaderPipeline::Initialize(
    ID3D12Device* device,
    ID3D12RootSignature* rootSignature,
    ID3D12PipelineState* fallbackPipelineState,
    EditorProductionShaderPipelinePolicy policy,
    std::string* errorMessage) {
    Shutdown();
    if (device == nullptr || rootSignature == nullptr || fallbackPipelineState == nullptr) {
        SetError(errorMessage, "E-9 requires a D3D12 device, Main root signature, and fallback PSO.");
        return false;
    }
    if (policy.maxResidentVariants == 0) policy.maxResidentVariants = 1;
    device_ = device;
    rootSignature_ = rootSignature;
    fallbackPipelineState_ = fallbackPipelineState;
    policy_ = std::move(policy);

    const std::filesystem::path templatePath = ResolveResource("ProductionMaterial.PS.hlsl");
    const std::filesystem::path vertexPath = ResolveResource("Object3D.VS.hlsl");
    if (!ReadText(templatePath, shaderTemplate_)) {
        SetError(errorMessage, "E-9 could not read Resources/ProductionMaterial.PS.hlsl.");
        Shutdown();
        return false;
    }
    std::string vertexSource;
    if (!ReadText(vertexPath, vertexSource)) {
        SetError(errorMessage, "E-9 could not read Resources/Object3D.VS.hlsl.");
        Shutdown();
        return false;
    }
    pipelineContractHash_ = HashText(shaderTemplate_);
    pipelineContractHash_ = HashText(vertexSource, pipelineContractHash_);

    ge3::core::ShaderCompiler compiler;
    if (!compiler.Initialize()) {
        SetError(errorMessage, "E-9 failed to initialize DXC for the shared Object3D vertex shader.");
        Shutdown();
        return false;
    }
    vertexShader_ = compiler.CompileFromFile(vertexPath.wstring(), L"main", L"vs_6_0");
    if (vertexShader_ == nullptr) {
        SetError(errorMessage, "E-9 failed to compile the shared Object3D vertex shader.");
        Shutdown();
        return false;
    }
    if (!LoadPipelineLibrary()) {
        // A corrupt or driver-incompatible cache is recoverable: start empty.
        pipelineLibrary_.Reset();
        Microsoft::WRL::ComPtr<ID3D12Device1> device1;
        if (SUCCEEDED(device_.As(&device1)) && device1 != nullptr) {
            device1->CreatePipelineLibrary(nullptr, 0, IID_PPV_ARGS(&pipelineLibrary_));
        }
    }
    return true;
}

void EditorProductionShaderPipeline::Shutdown() {
    for (auto& [hash, job] : compileJobs_) {
        (void)hash;
        if (job.future.valid()) job.future.wait();
    }
    compileJobs_.clear();
    bindings_.clear();
    variants_.clear();
    lastKnownGoodByMaterial_.clear();
    failedVariants_.clear();
    pendingPipelines_.clear();
    diagnostics_.clear();
    pipelineLibrary_.Reset();
    vertexShader_.Reset();
    fallbackPipelineState_.Reset();
    rootSignature_.Reset();
    device_.Reset();
    shaderTemplate_.clear();
    pipelineContractHash_ = 0;
    frameIndex_ = 0;
    stats_ = {};
}

bool EditorProductionShaderPipeline::LoadPipelineLibrary() {
    Microsoft::WRL::ComPtr<ID3D12Device1> device1;
    if (FAILED(device_.As(&device1)) || device1 == nullptr) return false;
    std::vector<uint8_t> bytes;
    std::error_code error;
    const uint64_t size = std::filesystem::file_size(policy_.pipelineLibraryPath, error);
    if (!error && size > 0 && size <= 64ull * 1024ull * 1024ull) {
        std::ifstream input(policy_.pipelineLibraryPath, std::ios::binary);
        bytes.resize(static_cast<size_t>(size));
        if (!input.read(reinterpret_cast<char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()))) {
            bytes.clear();
        }
    }
    const void* data = bytes.empty() ? nullptr : bytes.data();
    const size_t dataSize = bytes.size();
    return SUCCEEDED(device1->CreatePipelineLibrary(
        data, dataSize, IID_PPV_ARGS(&pipelineLibrary_)));
}

void EditorProductionShaderPipeline::SavePipelineLibrary() {
    if (pipelineLibrary_ == nullptr) return;
    const SIZE_T size = pipelineLibrary_->GetSerializedSize();
    if (size == 0 || size > 64ull * 1024ull * 1024ull) return;
    std::vector<uint8_t> bytes(size);
    if (FAILED(pipelineLibrary_->Serialize(bytes.data(), bytes.size()))) return;
    std::error_code error;
    std::filesystem::create_directories(policy_.pipelineLibraryPath.parent_path(), error);
    if (error) return;
    const auto temporary = policy_.pipelineLibraryPath.string() + ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) return;
        output.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        if (!output) return;
    }
    error.clear();
    std::filesystem::rename(temporary, policy_.pipelineLibraryPath, error);
    if (!error) return;
    error.clear();
    std::filesystem::remove(policy_.pipelineLibraryPath, error);
    error.clear();
    std::filesystem::rename(temporary, policy_.pipelineLibraryPath, error);
}

bool EditorProductionShaderPipeline::QueueCompile(
    const EditorProductionMaterialShaderSource& source,
    const EditorProductionShaderVariantKey& key,
    std::string* errorMessage) {
    const uint64_t hash = key.Hash();
    std::string generated;
    if (!BuildGeneratedPixelShaderSource(
            source, key, shaderTemplate_, generated, errorMessage)) {
        failedVariants_.insert(hash);
        ++stats_.compilesFailed;
        return false;
    }
    const std::filesystem::path sourcePath =
        policy_.generatedSourceRoot / (key.StableName() + ".PS.hlsl");
    if (!WriteIfChanged(sourcePath, generated)) {
        SetError(errorMessage, "E-9 failed to atomically write generated shader source.");
        failedVariants_.insert(hash);
        ++stats_.compilesFailed;
        return false;
    }
    const std::filesystem::path resolvedSource = std::filesystem::absolute(sourcePath);
    const std::filesystem::path resourceRoot = ResolveResource("Object3d.hlsli").parent_path();
    CompileJob job{};
    job.key = key;
    job.sourcePath = resolvedSource;
    job.future = std::async(std::launch::async, [resolvedSource, resourceRoot]() {
        CompileCandidate candidate{};
        ge3::core::ShaderCompiler compiler;
        if (!compiler.Initialize()) {
            candidate.error = "DXC initialization failed on the shader worker.";
            return candidate;
        }
        const std::wstring includePath = resourceRoot.wstring();
        const std::vector<LPCWSTR> arguments = {L"-I", includePath.c_str()};
        candidate.bytecode = compiler.CompileFromFile(
            resolvedSource.wstring(), L"main", L"ps_6_0", arguments);
        if (candidate.bytecode == nullptr) {
            candidate.error = "DXC rejected generated Material Graph HLSL.";
        }
        return candidate;
    });
    compileJobs_.insert_or_assign(hash, std::move(job));
    ++stats_.compilesStarted;
    return true;
}

Microsoft::WRL::ComPtr<ID3D12PipelineState>
EditorProductionShaderPipeline::CreateVariantPipeline(
    const EditorProductionShaderVariantKey& key,
    IDxcBlob* pixelShader,
    bool& libraryHit,
    std::string& errorMessage) {
    libraryHit = false;
    if (device_ == nullptr || rootSignature_ == nullptr || vertexShader_ == nullptr ||
        pixelShader == nullptr) {
        errorMessage = "E-9 PSO creation contract is incomplete.";
        return nullptr;
    }
    static const D3D12_INPUT_ELEMENT_DESC inputElements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
            D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
            D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
            D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC description{};
    description.pRootSignature = rootSignature_.Get();
    description.InputLayout = {inputElements, static_cast<UINT>(std::size(inputElements))};
    description.VS = {vertexShader_->GetBufferPointer(), vertexShader_->GetBufferSize()};
    description.PS = {pixelShader->GetBufferPointer(), pixelShader->GetBufferSize()};
    description.BlendState.AlphaToCoverageEnable = FALSE;
    description.BlendState.IndependentBlendEnable = FALSE;
    auto& target = description.BlendState.RenderTarget[0];
    target.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    if (key.blendMode == EditorMaterialBlendMode::Translucent) {
        target.BlendEnable = TRUE;
        target.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        target.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        target.BlendOp = D3D12_BLEND_OP_ADD;
        target.SrcBlendAlpha = D3D12_BLEND_ONE;
        target.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        target.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    } else {
        target.BlendEnable = FALSE;
        target.SrcBlend = D3D12_BLEND_ONE;
        target.DestBlend = D3D12_BLEND_ZERO;
        target.BlendOp = D3D12_BLEND_OP_ADD;
        target.SrcBlendAlpha = D3D12_BLEND_ONE;
        target.DestBlendAlpha = D3D12_BLEND_ZERO;
        target.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    }
    description.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    description.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    description.RasterizerState.FrontCounterClockwise = FALSE;
    description.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    description.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    description.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    description.RasterizerState.DepthClipEnable = TRUE;
    description.DepthStencilState.DepthEnable = TRUE;
    description.DepthStencilState.DepthWriteMask =
        key.blendMode == EditorMaterialBlendMode::Translucent
            ? D3D12_DEPTH_WRITE_MASK_ZERO : D3D12_DEPTH_WRITE_MASK_ALL;
    description.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    description.DepthStencilState.StencilEnable = FALSE;
    description.SampleMask = (std::numeric_limits<UINT>::max)();
    description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    description.NumRenderTargets = 1;
    description.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    description.SampleDesc.Count = 1;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline;
    const std::string stableName = key.StableName();
    const std::wstring libraryName(stableName.begin(), stableName.end());
    if (pipelineLibrary_ != nullptr && SUCCEEDED(pipelineLibrary_->LoadGraphicsPipeline(
            libraryName.c_str(), &description, IID_PPV_ARGS(&pipeline)))) {
        libraryHit = true;
        return pipeline;
    }
    const HRESULT result = device_->CreateGraphicsPipelineState(
        &description, IID_PPV_ARGS(&pipeline));
    if (FAILED(result)) {
        std::ostringstream stream;
        stream << "D3D12 rejected generated PSO (HRESULT 0x" << std::hex
               << static_cast<uint32_t>(result) << ").";
        errorMessage = stream.str();
        return nullptr;
    }
    if (pipelineLibrary_ != nullptr) {
        pipelineLibrary_->StorePipeline(libraryName.c_str(), pipeline.Get());
        SavePipelineLibrary();
    }
    return pipeline;
}

void EditorProductionShaderPipeline::FinalizeCompletedJobs() {
    for (auto iterator = compileJobs_.begin(); iterator != compileJobs_.end();) {
        if (iterator->second.future.wait_for(std::chrono::seconds(0)) !=
            std::future_status::ready) {
            ++iterator;
            continue;
        }
        const uint64_t hash = iterator->first;
        CompileCandidate candidate = iterator->second.future.get();
        if (candidate.bytecode == nullptr) {
            failedVariants_.insert(hash);
            diagnostics_.push_back(iterator->second.key.StableName() + ": " +
                (candidate.error.empty() ? "Shader compile failed." : candidate.error));
            ++stats_.compilesFailed;
            iterator = compileJobs_.erase(iterator);
            continue;
        }
        bool libraryHit = false;
        std::string error;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline = CreateVariantPipeline(
            iterator->second.key, candidate.bytecode.Get(), libraryHit, error);
        if (pipeline == nullptr) {
            failedVariants_.insert(hash);
            diagnostics_.push_back(iterator->second.key.StableName() + ": " + error);
            ++stats_.compilesFailed;
        } else {
            ResidentVariant variant{};
            variant.key = iterator->second.key;
            variant.pipelineState = std::move(pipeline);
            variant.lastUsedFrame = frameIndex_;
            variants_.insert_or_assign(hash, std::move(variant));
            failedVariants_.erase(hash);
            ++stats_.compilesCompleted;
            if (libraryHit) ++stats_.pipelineLibraryHits;
            else ++stats_.pipelineLibraryMisses;
        }
        iterator = compileJobs_.erase(iterator);
    }
}

void EditorProductionShaderPipeline::CollectRetired(uint64_t completedFenceValue) {
    std::erase_if(pendingPipelines_, [&](const PendingPipeline& pending) {
        return pending.retireFenceValue <= completedFenceValue;
    });
}

void EditorProductionShaderPipeline::EvictInactive(
    const std::unordered_set<uint64_t>& activeVariants,
    uint64_t scheduledFenceValue) {
    std::vector<std::pair<uint64_t, uint64_t>> candidates;
    for (const auto& [hash, variant] : variants_) {
        if (!activeVariants.contains(hash)) candidates.push_back({hash, variant.lastUsedFrame});
    }
    std::sort(candidates.begin(), candidates.end(), [](const auto& left, const auto& right) {
        if (left.second != right.second) return left.second < right.second;
        return left.first < right.first;
    });
    for (const auto& [hash, lastUsed] : candidates) {
        const bool retentionExpired = frameIndex_ >= lastUsed &&
            frameIndex_ - lastUsed >= policy_.inactiveFrameRetention;
        if (!retentionExpired && variants_.size() <= policy_.maxResidentVariants) continue;
        auto found = variants_.find(hash);
        if (found == variants_.end()) continue;
        pendingPipelines_.push_back({found->second.pipelineState, scheduledFenceValue});
        variants_.erase(found);
        for (auto lkg = lastKnownGoodByMaterial_.begin();
             lkg != lastKnownGoodByMaterial_.end();) {
            if (lkg->second == hash) lkg = lastKnownGoodByMaterial_.erase(lkg);
            else ++lkg;
        }
    }
}

bool EditorProductionShaderPipeline::Sync(
    const EditorProductionMaterialPipeline& materials,
    const EditorProductionTexturePipeline& textures,
    uint64_t completedFenceValue,
    uint64_t scheduledFenceValue,
    std::string* errorMessage) {
    if (device_ == nullptr) {
        SetError(errorMessage, "E-9 shader pipeline is not initialized.");
        return false;
    }
    ++frameIndex_;
    bindings_.clear();
    diagnostics_.clear();
    CollectRetired(completedFenceValue);
    FinalizeCompletedJobs();
    stats_.requestedBindings = 0;
    stats_.readyBindings = 0;
    stats_.fallbackBindings = 0;
    stats_.lastKnownGoodBindings = 0;
    stats_.normalMapBindings = 0;
    std::unordered_set<uint64_t> activeVariants;

    for (const EditorProductionMaterialBinding& material : materials.Bindings()) {
        EditorProductionShaderBinding binding{};
        binding.entityGuid = material.entityGuid;
        binding.materialAssetGuid = material.materialAssetGuid;
        binding.materialSlot = material.materialSlot;
        binding.pipelineState = fallbackPipelineState_.Get();
        ++stats_.requestedBindings;

        const EditorProductionMaterialShaderSource* source = !material.fallback
            ? materials.ResolveShaderSource(material.materialAssetGuid) : nullptr;
        if (source == nullptr) {
            ++stats_.fallbackBindings;
            bindings_.push_back(std::move(binding));
            continue;
        }
        const EditorProductionTextureBinding* texture = textures.Resolve(
            material.entityGuid, material.materialSlot);
        const bool normalMap = texture != nullptr && !texture->normalFallback &&
            texture->normalHandle.ptr != 0;
        const EditorProductionShaderVariantKey key = MakeVariantKey(
            *source, normalMap, pipelineContractHash_);
        const uint64_t hash = key.Hash();
        binding.variantHash = hash;
        binding.normalMapEnabled = normalMap;

        auto resident = variants_.find(hash);
        if (resident != variants_.end()) {
            resident->second.lastUsedFrame = frameIndex_;
            binding.pipelineState = resident->second.pipelineState.Get();
            binding.fallback = false;
            activeVariants.insert(hash);
            ++stats_.memoryCacheHits;
            ++stats_.readyBindings;
            if (normalMap) ++stats_.normalMapBindings;
            const auto previous = lastKnownGoodByMaterial_.find(source->materialAssetGuid);
            if (previous != lastKnownGoodByMaterial_.end() && previous->second != hash) {
                ++stats_.hotReloads;
            }
            lastKnownGoodByMaterial_[source->materialAssetGuid] = hash;
        } else {
            ++stats_.memoryCacheMisses;
            if (!compileJobs_.contains(hash) && !failedVariants_.contains(hash)) {
                std::string queueError;
                if (!QueueCompile(*source, key, &queueError) && !queueError.empty()) {
                    diagnostics_.push_back(key.StableName() + ": " + queueError);
                }
            }
            const auto lkg = lastKnownGoodByMaterial_.find(source->materialAssetGuid);
            const auto priorVariant = lkg != lastKnownGoodByMaterial_.end()
                ? variants_.find(lkg->second) : variants_.end();
            if (priorVariant != variants_.end()) {
                priorVariant->second.lastUsedFrame = frameIndex_;
                binding.pipelineState = priorVariant->second.pipelineState.Get();
                binding.fallback = false;
                binding.lastKnownGood = true;
                activeVariants.insert(lkg->second);
                ++stats_.readyBindings;
                ++stats_.lastKnownGoodBindings;
            } else {
                ++stats_.fallbackBindings;
            }
        }
        bindings_.push_back(std::move(binding));
    }
    EvictInactive(activeVariants, scheduledFenceValue);
    stats_.residentVariants = static_cast<uint32_t>(variants_.size());
    stats_.queuedCompiles = static_cast<uint32_t>(compileJobs_.size());
    stats_.failedVariants = static_cast<uint32_t>(failedVariants_.size());
    stats_.pendingGpuRetirements = static_cast<uint32_t>(pendingPipelines_.size());
    if (!diagnostics_.empty() && errorMessage != nullptr) *errorMessage = diagnostics_.front();
    return true;
}

const EditorProductionShaderBinding* EditorProductionShaderPipeline::Resolve(
    std::string_view entityGuid,
    uint32_t materialSlot) const {
    const auto exact = std::find_if(bindings_.begin(), bindings_.end(), [&](const auto& binding) {
        return binding.entityGuid == entityGuid && binding.materialSlot == materialSlot;
    });
    if (exact != bindings_.end()) return &*exact;
    const auto fallback = std::find_if(bindings_.begin(), bindings_.end(), [&](const auto& binding) {
        return binding.entityGuid == entityGuid && binding.materialSlot == 0;
    });
    return fallback == bindings_.end() ? nullptr : &*fallback;
}

} // namespace editor
