#include "EditorProductionTexturePipeline.h"

#include "../../../externals/DirectXTex/DirectXTex.h"
#include "../../../externals/DirectXTex/d3dx12.h"

#include <algorithm>
#include <cwctype>
#include <limits>
#include <set>
#include <unordered_set>

namespace editor {
namespace {

void SetError(std::string* errorMessage, std::string message) {
    if (errorMessage != nullptr) *errorMessage = std::move(message);
}

uint64_t FileTimestamp(const std::filesystem::path& path) {
    std::error_code error;
    const auto value = std::filesystem::last_write_time(path, error);
    return error ? 0ull : static_cast<uint64_t>(value.time_since_epoch().count());
}

std::wstring LowerExtension(const std::filesystem::path& path) {
    std::wstring extension = path.extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](wchar_t value) {
        return static_cast<wchar_t>(std::towlower(value));
    });
    return extension;
}

DXGI_FORMAT LinearFormat(DXGI_FORMAT format) {
    switch (format) {
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_BC1_UNORM_SRGB: return DXGI_FORMAT_BC1_UNORM;
    case DXGI_FORMAT_BC2_UNORM_SRGB: return DXGI_FORMAT_BC2_UNORM;
    case DXGI_FORMAT_BC3_UNORM_SRGB: return DXGI_FORMAT_BC3_UNORM;
    case DXGI_FORMAT_BC7_UNORM_SRGB: return DXGI_FORMAT_BC7_UNORM;
    default: return format;
    }
}

DXGI_FORMAT ColorFormat(DXGI_FORMAT format) {
    switch (format) {
    case DXGI_FORMAT_R8G8B8A8_UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case DXGI_FORMAT_B8G8R8A8_UNORM: return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    case DXGI_FORMAT_BC1_UNORM: return DXGI_FORMAT_BC1_UNORM_SRGB;
    case DXGI_FORMAT_BC2_UNORM: return DXGI_FORMAT_BC2_UNORM_SRGB;
    case DXGI_FORMAT_BC3_UNORM: return DXGI_FORMAT_BC3_UNORM_SRGB;
    case DXGI_FORMAT_BC7_UNORM: return DXGI_FORMAT_BC7_UNORM_SRGB;
    default: return format;
    }
}

bool DecodeTexture(
    const std::filesystem::path& path,
    DirectX::ScratchImage& output,
    std::string* errorMessage) {
    DirectX::ScratchImage decoded;
    DirectX::TexMetadata metadata{};
    const std::wstring source = path.wstring();
    const std::wstring extension = LowerExtension(path);
    HRESULT result = E_FAIL;
    if (extension == L".dds") {
        result = DirectX::LoadFromDDSFile(
            source.c_str(), DirectX::DDS_FLAGS_NONE, &metadata, decoded);
    } else if (extension == L".tga") {
        result = DirectX::LoadFromTGAFile(
            source.c_str(), DirectX::TGA_FLAGS_NONE, &metadata, decoded);
    } else {
        result = DirectX::LoadFromWICFile(
            source.c_str(), DirectX::WIC_FLAGS_FORCE_RGB, &metadata, decoded);
    }
    if (FAILED(result)) {
        SetError(errorMessage, "Texture decode failed: " + path.generic_string());
        return false;
    }
    if (metadata.dimension != DirectX::TEX_DIMENSION_TEXTURE2D || metadata.arraySize != 1 ||
        metadata.depth != 1 || metadata.IsCubemap() || metadata.width == 0 || metadata.height == 0 ||
        metadata.mipLevels == 0 || metadata.format == DXGI_FORMAT_UNKNOWN) {
        SetError(errorMessage, "E-8 accepts non-cubemap 2D Texture assets only: " + path.generic_string());
        return false;
    }
    if (metadata.mipLevels == 1 && !DirectX::IsCompressed(metadata.format) &&
        (metadata.width > 1 || metadata.height > 1)) {
        DirectX::ScratchImage mips;
        result = DirectX::GenerateMipMaps(
            decoded.GetImages(), decoded.GetImageCount(), metadata,
            DirectX::TEX_FILTER_DEFAULT, 0, mips);
        if (SUCCEEDED(result)) {
            output = std::move(mips);
            return true;
        }
        // Some optimized DirectXTex builds reject legacy BGR TGA layouts for
        // mip filtering. Normalize the single source image to RGBA and retry;
        // residency behavior must not depend on Debug vs Development linkage.
        const DirectX::Image* baseImage = decoded.GetImage(0, 0, 0);
        DirectX::ScratchImage converted;
        if (baseImage != nullptr && SUCCEEDED(DirectX::Convert(
                *baseImage,
                DXGI_FORMAT_R8G8B8A8_UNORM,
                DirectX::TEX_FILTER_DEFAULT,
                DirectX::TEX_THRESHOLD_DEFAULT,
                converted))) {
            DirectX::ScratchImage convertedMips;
            const DirectX::TexMetadata convertedMetadata = converted.GetMetadata();
            result = DirectX::GenerateMipMaps(
                converted.GetImages(), converted.GetImageCount(), convertedMetadata,
                DirectX::TEX_FILTER_DEFAULT, 0, convertedMips);
            if (SUCCEEDED(result)) {
                output = std::move(convertedMips);
                return true;
            }
        }
    }
    output = std::move(decoded);
    return true;
}

} // namespace

bool EditorProductionTexturePipeline::Initialize(
    ID3D12Device* device,
    ID3D12DescriptorHeap* shaderVisibleSrvHeap,
    uint32_t descriptorSize,
    uint32_t firstDescriptorIndex,
    uint32_t descriptorCapacity,
    EditorProductionTexturePolicy policy,
    std::string* errorMessage) {
    Shutdown();
    if (device == nullptr || shaderVisibleSrvHeap == nullptr || descriptorSize == 0 ||
        descriptorCapacity == 0 || policy.gpuBudgetBytes == 0 || policy.maxTextureBytes == 0 ||
        policy.minimumResidentMipCount == 0) {
        SetError(errorMessage, "E-8 initialization requires a device, SRV heap, descriptor range, and non-zero budget.");
        return false;
    }
    const D3D12_DESCRIPTOR_HEAP_DESC heapDescription = shaderVisibleSrvHeap->GetDesc();
    if (heapDescription.Type != D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ||
        (heapDescription.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) == 0 ||
        firstDescriptorIndex > heapDescription.NumDescriptors ||
        descriptorCapacity > heapDescription.NumDescriptors - firstDescriptorIndex) {
        SetError(errorMessage, "E-8 descriptor range is outside the shared shader-visible SRV heap.");
        return false;
    }
    device_ = device;
    srvHeap_ = shaderVisibleSrvHeap;
    descriptorSize_ = descriptorSize;
    firstDescriptorIndex_ = firstDescriptorIndex;
    descriptorCapacity_ = descriptorCapacity;
    policy_ = policy;
    freeDescriptors_.reserve(descriptorCapacity_);
    for (uint32_t offset = descriptorCapacity_; offset > 0; --offset) {
        freeDescriptors_.push_back(firstDescriptorIndex_ + offset - 1);
    }
    return true;
}

void EditorProductionTexturePipeline::Shutdown() {
    bindings_.clear();
    diagnostics_.clear();
    activeKeys_.clear();
    resident_.clear();
    pending_.clear();
    freeDescriptors_.clear();
    srvHeap_.Reset();
    device_.Reset();
    descriptorSize_ = 0;
    firstDescriptorIndex_ = 0;
    descriptorCapacity_ = 0;
    frameIndex_ = 0;
    stats_ = {};
}

uint16_t EditorProductionTexturePipeline::ChooseFirstResidentMip(
    const std::vector<uint64_t>& mipByteSizes,
    uint64_t targetBytes,
    uint16_t minimumResidentMipCount) noexcept {
    if (mipByteSizes.empty()) return 0;
    const size_t minimum = (std::max)(size_t{1},
        (std::min)(static_cast<size_t>(minimumResidentMipCount), mipByteSizes.size()));
    uint64_t bytes = 0;
    size_t first = mipByteSizes.size() - minimum;
    for (size_t index = mipByteSizes.size(); index-- > first;) {
        bytes += mipByteSizes[index];
    }
    while (first > 0 && mipByteSizes[first - 1] <= targetBytes - (std::min)(targetBytes, bytes)) {
        --first;
        bytes += mipByteSizes[first];
    }
    return static_cast<uint16_t>(first);
}

std::string EditorProductionTexturePipeline::MakeKey(
    std::string_view guid,
    EditorProductionTextureUsage usage) {
    std::string key(guid);
    key += usage == EditorProductionTextureUsage::Albedo ? "#color" : "#normal";
    return key;
}

bool EditorProductionTexturePipeline::Sync(
    const EditorProductionMaterialPipeline& materials,
    const EditorAssetRegistry& registry,
    ID3D12GraphicsCommandList* uploadCommandList,
    uint64_t completedFenceValue,
    uint64_t scheduledFenceValue,
    std::string* errorMessage) {
    CollectRetired(completedFenceValue);
    ++frameIndex_;
    bindings_.clear();
    diagnostics_.clear();
    activeKeys_.clear();
    const uint64_t cacheHits = stats_.cacheHits;
    const uint64_t cacheMisses = stats_.cacheMisses;
    const uint64_t evictions = stats_.evictions;
    const uint64_t hotReloads = stats_.hotReloads;
    stats_ = {};
    stats_.cacheHits = cacheHits;
    stats_.cacheMisses = cacheMisses;
    stats_.evictions = evictions;
    stats_.hotReloads = hotReloads;
    stats_.gpuBudgetBytes = policy_.gpuBudgetBytes;

    std::vector<TextureRequest> requests;
    std::unordered_set<std::string> uniqueRequests;
    const auto appendRequest = [&](std::string_view guid, EditorProductionTextureUsage usage) {
        if (guid.empty()) return;
        const std::string key = MakeKey(guid, usage);
        if (!uniqueRequests.insert(key).second) return;
        const EditorAssetRecord* record = registry.FindByGuid(guid);
        if (record == nullptr || record->kind != EditorAssetKind::Texture || record->missing) {
            diagnostics_.push_back("Texture GUID '" + std::string(guid) + "' is missing or not a Texture Asset.");
            return;
        }
        activeKeys_[key] = true;
        requests.push_back({key, std::string(guid), usage, record});
    };
    for (const EditorProductionMaterialBinding& material : materials.Bindings()) {
        EditorProductionTextureBinding binding{};
        binding.entityGuid = material.entityGuid;
        binding.materialSlot = material.materialSlot;
        binding.albedoTextureGuid = material.albedoTextureGuid;
        binding.normalTextureGuid = material.normalTextureGuid;
        bindings_.push_back(std::move(binding));
        ++stats_.requestedBindings;
        if (!material.fallback) {
            appendRequest(material.albedoTextureGuid, EditorProductionTextureUsage::Albedo);
            appendRequest(material.normalTextureGuid, EditorProductionTextureUsage::Normal);
        }
    }
    stats_.requestedTextures = static_cast<uint32_t>(requests.size());

    for (auto iterator = resident_.begin(); iterator != resident_.end();) {
        const bool inactive = !activeKeys_.contains(iterator->first);
        const bool expired = inactive &&
            frameIndex_ - iterator->second.lastUsedFrame > policy_.inactiveFrameRetention;
        if (!expired) {
            ++iterator;
            continue;
        }
        ResidentTexture retired = std::move(iterator->second);
        iterator = resident_.erase(iterator);
        Retire(std::move(retired), scheduledFenceValue);
        ++stats_.evictions;
    }

    const uint64_t perTextureBudget = requests.empty()
        ? policy_.maxTextureBytes
        : (std::max)(uint64_t{1}, (std::min)(
            policy_.maxTextureBytes, policy_.gpuBudgetBytes / requests.size()));
    for (const TextureRequest& request : requests) {
        std::string requestError;
        if (!EnsureResident(
                request, perTextureBudget, uploadCommandList,
                scheduledFenceValue, &requestError)) {
            ++stats_.fallbackTextures;
            if (!requestError.empty()) diagnostics_.push_back(std::move(requestError));
        }
    }

    for (EditorProductionTextureBinding& binding : bindings_) {
        const auto apply = [&](std::string_view guid, EditorProductionTextureUsage usage,
                               D3D12_GPU_DESCRIPTOR_HANDLE& handle, uint16_t& firstMip,
                               uint16_t& mipCount, bool& fallback) {
            if (guid.empty()) return;
            const auto found = resident_.find(MakeKey(guid, usage));
            if (found == resident_.end()) return;
            handle = found->second.gpuHandle;
            firstMip = found->second.firstResidentMip;
            mipCount = found->second.residentMipCount;
            fallback = handle.ptr == 0;
        };
        apply(binding.albedoTextureGuid, EditorProductionTextureUsage::Albedo,
            binding.albedoHandle, binding.albedoFirstResidentMip,
            binding.albedoResidentMipCount, binding.albedoFallback);
        apply(binding.normalTextureGuid, EditorProductionTextureUsage::Normal,
            binding.normalHandle, binding.normalFirstResidentMip,
            binding.normalResidentMipCount, binding.normalFallback);
        if (binding.albedoTextureGuid.empty()) ++stats_.fallbackTextures;
    }

    stats_.residentTextures = static_cast<uint32_t>(resident_.size());
    stats_.residentDescriptors = stats_.residentTextures;
    for (const auto& [key, texture] : resident_) {
        (void)key;
        stats_.residentGpuBytes += texture.allocationBytes;
        if (texture.firstResidentMip == 0) ++stats_.fullMipTextures;
        else ++stats_.partialMipTextures;
    }
    stats_.pendingGpuRetirements = static_cast<uint32_t>(pending_.size());
    for (const PendingResource& pending : pending_) stats_.pendingGpuBytes += pending.allocationBytes;
    if (!diagnostics_.empty() && errorMessage != nullptr) *errorMessage = diagnostics_.front();
    return true;
}

bool EditorProductionTexturePipeline::EnsureResident(
    const TextureRequest& request,
    uint64_t targetBytes,
    ID3D12GraphicsCommandList* uploadCommandList,
    uint64_t scheduledFenceValue,
    std::string* errorMessage) {
    const uint64_t timestamp = request.record->sourceTimestamp != 0
        ? request.record->sourceTimestamp : FileTimestamp(request.record->sourcePath);
    auto existing = resident_.find(request.key);
    if (existing != resident_.end() && existing->second.sourceTimestamp == timestamp) {
        existing->second.lastUsedFrame = frameIndex_;
        ++stats_.cacheHits;
        return true;
    }
    if (device_ == nullptr || srvHeap_ == nullptr || uploadCommandList == nullptr) {
        SetError(errorMessage, "Texture '" + request.guid + "' is awaiting a D3D12 upload context.");
        return false;
    }

    ++stats_.cacheMisses;
    DirectX::ScratchImage decoded;
    if (!DecodeTexture(request.record->sourcePath, decoded, errorMessage)) return false;
    const DirectX::TexMetadata& metadata = decoded.GetMetadata();
    if (metadata.width > policy_.maxDimension || metadata.height > policy_.maxDimension ||
        metadata.mipLevels > UINT16_MAX) {
        SetError(errorMessage, "Texture exceeds E-8 dimension or mip safety limits: " +
            std::filesystem::path(request.record->sourcePath).generic_string());
        return false;
    }
    D3D12_FEATURE_DATA_FORMAT_SUPPORT support{metadata.format};
    if (FAILED(device_->CheckFeatureSupport(
            D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support))) ||
        (support.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE2D) == 0 ||
        (support.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE) == 0) {
        SetError(errorMessage, "Texture format is not sampleable on this D3D12 device.");
        return false;
    }
    std::vector<uint64_t> mipBytes;
    mipBytes.reserve(metadata.mipLevels);
    for (size_t mip = 0; mip < metadata.mipLevels; ++mip) {
        const DirectX::Image* image = decoded.GetImage(mip, 0, 0);
        if (image == nullptr) {
            SetError(errorMessage, "Decoded Texture mip chain is incomplete.");
            return false;
        }
        mipBytes.push_back(image->slicePitch);
    }
    uint16_t firstMip = ChooseFirstResidentMip(
        mipBytes, targetBytes, policy_.minimumResidentMipCount);
    const auto buildDescription = [&](uint16_t mip) {
        const DirectX::Image* first = decoded.GetImage(mip, 0, 0);
        D3D12_RESOURCE_DESC description{};
        description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        description.Width = first != nullptr ? first->width : 1;
        description.Height = static_cast<UINT>(first != nullptr ? first->height : 1);
        description.DepthOrArraySize = 1;
        description.MipLevels = static_cast<UINT16>(metadata.mipLevels - mip);
        description.Format = metadata.format;
        description.SampleDesc.Count = 1;
        description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        return description;
    };
    D3D12_RESOURCE_DESC description = buildDescription(firstMip);
    D3D12_RESOURCE_ALLOCATION_INFO allocation = device_->GetResourceAllocationInfo(
        0, 1, &description);
    const uint16_t minimumMips = (std::min)(policy_.minimumResidentMipCount,
        static_cast<uint16_t>(metadata.mipLevels));
    while (allocation.SizeInBytes > targetBytes &&
           metadata.mipLevels - firstMip > minimumMips) {
        ++firstMip;
        description = buildDescription(firstMip);
        allocation = device_->GetResourceAllocationInfo(0, 1, &description);
    }
    if (allocation.SizeInBytes > policy_.maxTextureBytes ||
        allocation.SizeInBytes > policy_.gpuBudgetBytes) {
        SetError(errorMessage, "Texture cannot fit the configured E-8 GPU residency budget.");
        return false;
    }

    const uint64_t existingBytes = existing == resident_.end() ? 0 : existing->second.allocationBytes;
    while (ResidentBytes() - existingBytes + allocation.SizeInBytes > policy_.gpuBudgetBytes) {
        if (!EvictOneInactive(activeKeys_, scheduledFenceValue)) {
            SetError(errorMessage, "Texture residency budget is exhausted by active Texture assets.");
            return false;
        }
    }
    const uint32_t descriptorIndex = AcquireDescriptor();
    if (descriptorIndex == UINT32_MAX) {
        SetError(errorMessage, "E-8 SRV descriptor range is exhausted; fallback remains bound until a frame fence completes.");
        return false;
    }

    D3D12_HEAP_PROPERTIES defaultHeap{};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
    Microsoft::WRL::ComPtr<ID3D12Resource> textureResource;
    HRESULT result = device_->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &description,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
        IID_PPV_ARGS(textureResource.ReleaseAndGetAddressOf()));
    if (FAILED(result)) {
        ReleaseDescriptor(descriptorIndex);
        SetError(errorMessage, "D3D12 failed to allocate a resident Texture resource.");
        return false;
    }
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    subresources.reserve(description.MipLevels);
    for (uint16_t mip = firstMip; mip < metadata.mipLevels; ++mip) {
        const DirectX::Image* image = decoded.GetImage(mip, 0, 0);
        subresources.push_back({image->pixels,
            static_cast<LONG_PTR>(image->rowPitch),
            static_cast<LONG_PTR>(image->slicePitch)});
    }
    const UINT64 uploadSize = GetRequiredIntermediateSize(
        textureResource.Get(), 0, description.MipLevels);
    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC uploadDescription{};
    uploadDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadDescription.Width = uploadSize;
    uploadDescription.Height = 1;
    uploadDescription.DepthOrArraySize = 1;
    uploadDescription.MipLevels = 1;
    uploadDescription.SampleDesc.Count = 1;
    uploadDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    Microsoft::WRL::ComPtr<ID3D12Resource> upload;
    result = device_->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE, &uploadDescription,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(upload.ReleaseAndGetAddressOf()));
    if (FAILED(result) || UpdateSubresources(
            uploadCommandList, textureResource.Get(), upload.Get(), 0, 0,
            description.MipLevels, subresources.data()) == 0) {
        ReleaseDescriptor(descriptorIndex);
        SetError(errorMessage, "D3D12 failed to stage the resident Texture mip chain.");
        return false;
    }
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = textureResource.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = static_cast<D3D12_RESOURCE_STATES>(
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    uploadCommandList->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE cpu = srvHeap_->GetCPUDescriptorHandleForHeapStart();
    cpu.ptr += static_cast<SIZE_T>(descriptorSize_) * descriptorIndex;
    D3D12_GPU_DESCRIPTOR_HANDLE gpu = srvHeap_->GetGPUDescriptorHandleForHeapStart();
    gpu.ptr += static_cast<UINT64>(descriptorSize_) * descriptorIndex;
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Format = request.usage == EditorProductionTextureUsage::Albedo
        ? ColorFormat(metadata.format) : LinearFormat(metadata.format);
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Texture2D.MostDetailedMip = 0;
    srv.Texture2D.MipLevels = description.MipLevels;
    srv.Texture2D.ResourceMinLODClamp = 0.0f;
    device_->CreateShaderResourceView(textureResource.Get(), &srv, cpu);

    ResidentTexture built{};
    built.guid = request.guid;
    built.sourcePath = request.record->sourcePath;
    built.usage = request.usage;
    built.sourceTimestamp = timestamp;
    built.allocationBytes = allocation.SizeInBytes;
    built.lastUsedFrame = frameIndex_;
    built.descriptorIndex = descriptorIndex;
    built.width = static_cast<uint32_t>(metadata.width);
    built.height = static_cast<uint32_t>(metadata.height);
    built.sourceMipCount = static_cast<uint16_t>(metadata.mipLevels);
    built.firstResidentMip = firstMip;
    built.residentMipCount = description.MipLevels;
    built.resource = std::move(textureResource);
    built.gpuHandle = gpu;
    pending_.push_back({upload, scheduledFenceValue, uploadSize, UINT32_MAX});
    stats_.uploadedGpuBytes += allocation.SizeInBytes;

    if (existing != resident_.end()) {
        ResidentTexture replaced = std::move(existing->second);
        existing->second = std::move(built);
        Retire(std::move(replaced), scheduledFenceValue);
        ++stats_.hotReloads;
    } else {
        resident_.emplace(request.key, std::move(built));
    }
    return true;
}

bool EditorProductionTexturePipeline::EvictOneInactive(
    const std::unordered_map<std::string, bool>& activeKeys,
    uint64_t scheduledFenceValue) {
    auto candidate = resident_.end();
    for (auto iterator = resident_.begin(); iterator != resident_.end(); ++iterator) {
        if (activeKeys.contains(iterator->first)) continue;
        if (candidate == resident_.end() ||
            iterator->second.lastUsedFrame < candidate->second.lastUsedFrame ||
            (iterator->second.lastUsedFrame == candidate->second.lastUsedFrame &&
             iterator->first < candidate->first)) {
            candidate = iterator;
        }
    }
    if (candidate == resident_.end()) return false;
    ResidentTexture retired = std::move(candidate->second);
    resident_.erase(candidate);
    Retire(std::move(retired), scheduledFenceValue);
    ++stats_.evictions;
    return true;
}

void EditorProductionTexturePipeline::Retire(
    ResidentTexture&& texture,
    uint64_t scheduledFenceValue) {
    pending_.push_back({std::move(texture.resource), scheduledFenceValue,
        texture.allocationBytes, texture.descriptorIndex});
}

void EditorProductionTexturePipeline::CollectRetired(uint64_t completedFenceValue) {
    for (auto iterator = pending_.begin(); iterator != pending_.end();) {
        if (iterator->retireFenceValue > completedFenceValue) {
            ++iterator;
            continue;
        }
        if (iterator->descriptorIndex != UINT32_MAX) {
            ReleaseDescriptor(iterator->descriptorIndex);
        }
        iterator = pending_.erase(iterator);
    }
}

uint32_t EditorProductionTexturePipeline::AcquireDescriptor() {
    if (freeDescriptors_.empty()) return UINT32_MAX;
    const uint32_t result = freeDescriptors_.back();
    freeDescriptors_.pop_back();
    return result;
}

void EditorProductionTexturePipeline::ReleaseDescriptor(uint32_t descriptorIndex) {
    if (descriptorIndex < firstDescriptorIndex_ ||
        descriptorIndex >= firstDescriptorIndex_ + descriptorCapacity_) return;
    if (std::find(freeDescriptors_.begin(), freeDescriptors_.end(), descriptorIndex) ==
        freeDescriptors_.end()) {
        freeDescriptors_.push_back(descriptorIndex);
    }
}

uint64_t EditorProductionTexturePipeline::ResidentBytes() const noexcept {
    uint64_t bytes = 0;
    for (const auto& [key, texture] : resident_) {
        (void)key;
        bytes += texture.allocationBytes;
    }
    return bytes;
}

const EditorProductionTextureBinding* EditorProductionTexturePipeline::Resolve(
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
