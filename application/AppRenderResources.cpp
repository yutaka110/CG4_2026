#include "AppRenderResources.h"

#include "AppRuntimeUtils.h"

#include "utils/dx12/BufferHelper.h"
#include "utils/math/Vector.h"

#include "../../externals/DirectXTex/d3dx12.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace {
bool HasExtension(const std::string& filePath, const wchar_t* extension) {
    std::filesystem::path path(filePath);
    std::wstring ext = path.extension().wstring();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(std::towlower(c));
    });
    return ext == extension;
}
} // namespace

ComPtr<ID3D12DescriptorHeap> AppRenderResources::CreateDescriptorHeap(
    ComPtr<ID3D12Device> device,
    D3D12_DESCRIPTOR_HEAP_TYPE heapType,
    UINT numDescriptors,
    bool shaderVisible) {
    ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;
    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
    descriptorHeapDesc.Type = heapType;
    descriptorHeapDesc.NumDescriptors = numDescriptors;
    descriptorHeapDesc.Flags = shaderVisible
        ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
        : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    HRESULT hr = device->CreateDescriptorHeap(
        &descriptorHeapDesc,
        IID_PPV_ARGS(&descriptorHeap));
    assert(SUCCEEDED(hr));
    return descriptorHeap;
}

DirectX::ScratchImage AppRenderResources::LoadTexture(const std::string& filePath) {
    DirectX::ScratchImage image{};
    std::wstring filePathW = ConvertString(filePath);

    HRESULT hr = S_OK;
    if (HasExtension(filePath, L".dds")) {
        hr = DirectX::LoadFromDDSFile(
            filePathW.c_str(),
            DirectX::DDS_FLAGS_NONE,
            nullptr,
            image);
    } else {
        hr = DirectX::LoadFromWICFile(
            filePathW.c_str(),
            DirectX::WIC_FLAGS_FORCE_SRGB,
            nullptr,
            image);
    }
    assert(SUCCEEDED(hr));

    const DirectX::TexMetadata& metadata = image.GetMetadata();
    if (metadata.IsCubemap() || DirectX::IsCompressed(metadata.format)) {
        return image;
    }

    DirectX::ScratchImage mipImages{};
    hr = DirectX::GenerateMipMaps(
        image.GetImages(),
        image.GetImageCount(),
        image.GetMetadata(),
        DirectX::TEX_FILTER_SRGB,
        0,
        mipImages);
    assert(SUCCEEDED(hr));

    return mipImages;
}

ComPtr<ID3D12Resource> AppRenderResources::CreateTextureResource(
    ComPtr<ID3D12Device> device,
    const DirectX::TexMetadata& metadata) {
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = UINT(metadata.width);
    resourceDesc.Height = UINT(metadata.height);
    resourceDesc.MipLevels = UINT16(metadata.mipLevels);
    resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);
    resourceDesc.Format = metadata.format;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);

    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    ComPtr<ID3D12Resource> resource = nullptr;
    HRESULT hr = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&resource));

    assert(SUCCEEDED(hr));
    return resource;
}

void AppRenderResources::UploadTextureData(
    ComPtr<ID3D12Device> device,
    ID3D12GraphicsCommandList* commandList,
    ComPtr<ID3D12Resource> texture,
    const DirectX::ScratchImage& mipImages,
    std::vector<ComPtr<ID3D12Resource>>& retainedUploadResources) {
    assert(device != nullptr);
    assert(commandList != nullptr);
    assert(texture != nullptr);

    const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
    const UINT subresourceCount =
        static_cast<UINT>(metadata.arraySize * metadata.mipLevels);
    assert(subresourceCount > 0);

    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    subresources.reserve(subresourceCount);

    for (size_t item = 0; item < metadata.arraySize; ++item) {
        for (size_t mipLevel = 0; mipLevel < metadata.mipLevels; ++mipLevel) {
            const DirectX::Image* img = mipImages.GetImage(mipLevel, item, 0);
            assert(img != nullptr);

            D3D12_SUBRESOURCE_DATA subresource{};
            subresource.pData = img->pixels;
            subresource.RowPitch = static_cast<LONG_PTR>(img->rowPitch);
            subresource.SlicePitch = static_cast<LONG_PTR>(img->slicePitch);
            subresources.push_back(subresource);
        }
    }

    const UINT64 uploadBufferSize =
        GetRequiredIntermediateSize(texture.Get(), 0, subresourceCount);

    D3D12_HEAP_PROPERTIES uploadHeapProperties{};
    uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC uploadResourceDesc{};
    uploadResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadResourceDesc.Width = uploadBufferSize;
    uploadResourceDesc.Height = 1;
    uploadResourceDesc.DepthOrArraySize = 1;
    uploadResourceDesc.MipLevels = 1;
    uploadResourceDesc.SampleDesc.Count = 1;
    uploadResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ComPtr<ID3D12Resource> uploadResource = nullptr;
    HRESULT hr = device->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &uploadResourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadResource));
    assert(SUCCEEDED(hr));

    UpdateSubresources(
        commandList,
        texture.Get(),
        uploadResource.Get(),
        0,
        0,
        subresourceCount,
        subresources.data());

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = texture.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = static_cast<D3D12_RESOURCE_STATES>(
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    commandList->ResourceBarrier(1, &barrier);

    retainedUploadResources.push_back(uploadResource);
}

D3D12_CPU_DESCRIPTOR_HANDLE AppRenderResources::GetCPUDescriptorHandle(
    ComPtr<ID3D12DescriptorHeap> descriptorHeap,
    uint32_t descriptorSize,
    uint32_t index) {
    D3D12_CPU_DESCRIPTOR_HANDLE handleCPU =
        descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    handleCPU.ptr += descriptorSize * index;
    return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE AppRenderResources::GetGPUDescriptorHandle(
    ComPtr<ID3D12DescriptorHeap> descriptorHeap,
    uint32_t descriptorSize,
    uint32_t index) {
    D3D12_GPU_DESCRIPTOR_HANDLE handleGPU =
        descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    handleGPU.ptr += descriptorSize * index;
    return handleGPU;
}

bool AppRenderResources::InitializeSharedSpriteQuad(ComPtr<ID3D12Device> device) {
    sharedSpriteQuadVertexResource_ = CreateBufferResource(device, sizeof(VertexData) * 6);
    if (sharedSpriteQuadVertexResource_ == nullptr) {
        return false;
    }

    VertexData* vertices = nullptr;
    HRESULT hr = sharedSpriteQuadVertexResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&vertices));
    if (FAILED(hr) || vertices == nullptr) {
        return false;
    }

    vertices[0].position = {-0.5f, -0.5f, 0.0f, 1.0f};
    vertices[0].texcoord = {0.0f, 1.0f};
    vertices[0].normal = {0.0f, 0.0f, -1.0f};

    vertices[1].position = {-0.5f, 0.5f, 0.0f, 1.0f};
    vertices[1].texcoord = {0.0f, 0.0f};
    vertices[1].normal = {0.0f, 0.0f, -1.0f};

    vertices[2].position = {0.5f, -0.5f, 0.0f, 1.0f};
    vertices[2].texcoord = {1.0f, 1.0f};
    vertices[2].normal = {0.0f, 0.0f, -1.0f};

    vertices[3].position = {-0.5f, 0.5f, 0.0f, 1.0f};
    vertices[3].texcoord = {0.0f, 0.0f};
    vertices[3].normal = {0.0f, 0.0f, -1.0f};

    vertices[4].position = {0.5f, 0.5f, 0.0f, 1.0f};
    vertices[4].texcoord = {1.0f, 0.0f};
    vertices[4].normal = {0.0f, 0.0f, -1.0f};

    vertices[5].position = {0.5f, -0.5f, 0.0f, 1.0f};
    vertices[5].texcoord = {1.0f, 1.0f};
    vertices[5].normal = {0.0f, 0.0f, -1.0f};

    sharedSpriteQuadVertexResource_->Unmap(0, nullptr);

    sharedSpriteQuadVertexBufferView_.BufferLocation =
        sharedSpriteQuadVertexResource_->GetGPUVirtualAddress();
    sharedSpriteQuadVertexBufferView_.SizeInBytes = sizeof(VertexData) * 6;
    sharedSpriteQuadVertexBufferView_.StrideInBytes = sizeof(VertexData);
    return true;
}

const D3D12_VERTEX_BUFFER_VIEW& AppRenderResources::SharedSpriteQuadVertexBufferView() const {
    return sharedSpriteQuadVertexBufferView_;
}

const D3D12_VERTEX_BUFFER_VIEW& AppRenderResources::ParticleVertexBufferView() const {
    return sharedSpriteQuadVertexBufferView_;
}
