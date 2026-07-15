#include "EditorProductionLightingPipeline.h"

#include "core/ShaderCompiler.h"

#include <DirectXMath.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace editor {
namespace {

using namespace DirectX;
using Microsoft::WRL::ComPtr;

constexpr float kPi = 3.14159265358979323846f;
constexpr uint64_t kConstantAlignment = 256;

void SetError(std::string* errorMessage, std::string message) {
    if (errorMessage != nullptr) *errorMessage = std::move(message);
}

float Clamp(float value, float minimum, float maximum) {
    return (std::max)(minimum, (std::min)(maximum, value));
}

float Length(const Vector3& value) {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

Vector3 NormalizeOr(Vector3 value, Vector3 fallback) {
    const float length = Length(value);
    if (length <= 1.0e-6f) return fallback;
    value.x /= length;
    value.y /= length;
    value.z /= length;
    return value;
}

Vector3 TransformPointNoDivide(const Vector3& value, const Matrix4x4& matrix) {
    return {
        value.x * matrix.m[0][0] + value.y * matrix.m[1][0] +
            value.z * matrix.m[2][0] + matrix.m[3][0],
        value.x * matrix.m[0][1] + value.y * matrix.m[1][1] +
            value.z * matrix.m[2][1] + matrix.m[3][1],
        value.x * matrix.m[0][2] + value.y * matrix.m[1][2] +
            value.z * matrix.m[2][2] + matrix.m[3][2]};
}

Vector3 TransformDirection(const Vector3& value, const Matrix4x4& matrix) {
    return {
        value.x * matrix.m[0][0] + value.y * matrix.m[1][0] + value.z * matrix.m[2][0],
        value.x * matrix.m[0][1] + value.y * matrix.m[1][1] + value.z * matrix.m[2][1],
        value.x * matrix.m[0][2] + value.y * matrix.m[1][2] + value.z * matrix.m[2][2]};
}

const EditorSceneProperty* Property(const EditorSceneComponent& component, std::string_view name) {
    const auto found = std::find_if(component.properties.begin(), component.properties.end(),
        [&](const EditorSceneProperty& value) { return value.name == name; });
    return found == component.properties.end() ? nullptr : &*found;
}

float FloatProperty(const EditorSceneComponent& component, std::string_view name, float fallback) {
    const EditorSceneProperty* property = Property(component, name);
    if (property == nullptr) return fallback;
    std::istringstream input(property->value);
    float result = fallback;
    return (input >> result) && std::isfinite(result) ? result : fallback;
}

bool BoolProperty(const EditorSceneComponent& component, std::string_view name, bool fallback) {
    const EditorSceneProperty* property = Property(component, name);
    if (property == nullptr) return fallback;
    return property->value == "1" || property->value == "true" || property->value == "True" ||
        (property->value != "0" && property->value != "false" &&
         property->value != "False" && fallback);
}

Vector3 VectorProperty(const EditorSceneComponent& component, std::string_view name, Vector3 fallback) {
    const EditorSceneProperty* property = Property(component, name);
    if (property == nullptr) return fallback;
    std::istringstream input(property->value);
    Vector3 result{};
    if (!(input >> result.x >> result.y >> result.z) || !std::isfinite(result.x) ||
        !std::isfinite(result.y) || !std::isfinite(result.z)) return fallback;
    return result;
}

Vector4 ColorProperty(const EditorSceneComponent& component) {
    const EditorSceneProperty* property = Property(component, "color");
    Vector4 result{1.0f, 1.0f, 1.0f, 1.0f};
    if (property == nullptr) return result;
    std::istringstream input(property->value);
    if (!(input >> result.x >> result.y >> result.z)) return {1.0f, 1.0f, 1.0f, 1.0f};
    if (!(input >> result.w)) result.w = 1.0f;
    result.x = Clamp(result.x, 0.0f, 100.0f);
    result.y = Clamp(result.y, 0.0f, 100.0f);
    result.z = Clamp(result.z, 0.0f, 100.0f);
    result.w = Clamp(result.w, 0.0f, 1.0f);
    return result;
}

Matrix4x4 ToMatrix4x4(FXMMATRIX value) {
    XMFLOAT4X4 stored{};
    XMStoreFloat4x4(&stored, value);
    Matrix4x4 result{};
    std::memcpy(result.m, stored.m, sizeof(result.m));
    return result;
}

Matrix4x4 LookAt(const Vector3& eye, const Vector3& target, Vector3 up) {
    const Vector3 forward = NormalizeOr(
        {target.x - eye.x, target.y - eye.y, target.z - eye.z}, {0.0f, 0.0f, 1.0f});
    if (std::abs(forward.x * up.x + forward.y * up.y + forward.z * up.z) > 0.98f)
        up = {0.0f, 0.0f, 1.0f};
    return ToMatrix4x4(XMMatrixLookAtLH(
        XMVectorSet(eye.x, eye.y, eye.z, 1.0f),
        XMVectorSet(target.x, target.y, target.z, 1.0f),
        XMVectorSet(up.x, up.y, up.z, 0.0f)));
}

bool CreateUploadBuffer(
    ID3D12Device* device,
    uint64_t byteSize,
    ComPtr<ID3D12Resource>& resource,
    uint8_t** mapped,
    std::string* errorMessage) {
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC description{};
    description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    description.Width = (std::max)(kConstantAlignment, (byteSize + 255ull) & ~255ull);
    description.Height = 1;
    description.DepthOrArraySize = 1;
    description.MipLevels = 1;
    description.SampleDesc.Count = 1;
    description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if (FAILED(device->CreateCommittedResource(
            &heap, D3D12_HEAP_FLAG_NONE, &description,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(resource.ReleaseAndGetAddressOf())))) {
        SetError(errorMessage, "E-10 failed to create a mapped GPU buffer.");
        return false;
    }
    void* pointer = nullptr;
    if (FAILED(resource->Map(0, nullptr, &pointer)) || pointer == nullptr) {
        SetError(errorMessage, "E-10 failed to map a GPU buffer.");
        resource.Reset();
        return false;
    }
    *mapped = static_cast<uint8_t*>(pointer);
    std::memset(*mapped, 0, static_cast<size_t>(description.Width));
    return true;
}

struct LightCandidate {
    const EditorSceneEntity* entity = nullptr;
    const EditorSceneComponent* component = nullptr;
    EditorProductionLightType type = EditorProductionLightType::Directional;
    Matrix4x4 world = MakeIdentity4x4();
    float priority = 0.0f;
    float shadowPriority = 0.0f;
    bool castsShadow = false;
};

} // namespace

uint32_t EditorProductionLightingPipeline::DepthSlice(
    float depth, float nearPlane, float farPlane, uint32_t sliceCount) noexcept {
    if (sliceCount <= 1 || farPlane <= nearPlane) return 0;
    const float safeNear = (std::max)(0.001f, nearPlane);
    const float safeFar = (std::max)(safeNear + 0.001f, farPlane);
    const float safeDepth = Clamp(depth, safeNear, safeFar);
    const float normalized = std::log(safeDepth / safeNear) / std::log(safeFar / safeNear);
    return (std::min)(sliceCount - 1,
        static_cast<uint32_t>(normalized * static_cast<float>(sliceCount)));
}

bool EditorProductionLightingPipeline::Initialize(
    ID3D12Device* device,
    ID3D12DescriptorHeap* sharedSrvHeap,
    uint32_t descriptorSize,
    uint32_t shadowSrvDescriptorIndex,
    ID3D12RootSignature* mainRootSignature,
    EditorProductionLightingPolicy policy,
    std::string* errorMessage) {
    Shutdown();
    if (device == nullptr || sharedSrvHeap == nullptr || descriptorSize == 0 ||
        mainRootSignature == nullptr) {
        SetError(errorMessage, "E-10 requires a D3D12 device, shared SRV heap, and Main root signature.");
        return false;
    }
    policy.maximumVisibleLights = (std::max)(1u, policy.maximumVisibleLights);
    policy.tileSizePixels = (std::max)(8u, policy.tileSizePixels);
    policy.depthSliceCount = (std::max)(1u, policy.depthSliceCount);
    policy.maximumTileCountX = (std::max)(1u, policy.maximumTileCountX);
    policy.maximumTileCountY = (std::max)(1u, policy.maximumTileCountY);
    policy.maximumLightsPerCluster = (std::max)(1u, policy.maximumLightsPerCluster);
    policy.maximumShadowMaps = (std::min)(kEditorProductionMaximumShadowMaps,
        (std::max)(1u, policy.maximumShadowMaps));
    policy.shadowMapSize = (std::max)(128u, policy.shadowMapSize);
    policy_ = policy;
    device_ = device;
    sharedSrvHeap_ = sharedSrvHeap;
    rootSignature_ = mainRootSignature;

    const D3D12_GPU_DESCRIPTOR_HANDLE start = sharedSrvHeap->GetGPUDescriptorHandleForHeapStart();
    shadowAtlasGpu_.ptr = start.ptr +
        static_cast<UINT64>(shadowSrvDescriptorIndex) * descriptorSize;
    if (!CreateGpuResources(errorMessage) || !CreateShadowResources(errorMessage) ||
        !CreateShadowPipeline(errorMessage)) {
        Shutdown();
        return false;
    }

    const D3D12_CPU_DESCRIPTOR_HANDLE cpuStart = sharedSrvHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_CPU_DESCRIPTOR_HANDLE shadowCpu{};
    shadowCpu.ptr = cpuStart.ptr +
        static_cast<SIZE_T>(shadowSrvDescriptorIndex) * descriptorSize;
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format = DXGI_FORMAT_R32_FLOAT;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2DArray.MipLevels = 1;
    srv.Texture2DArray.ArraySize = policy_.maximumShadowMaps;
    device_->CreateShaderResourceView(shadowAtlas_.Get(), &srv, shadowCpu);
    return true;
}

void EditorProductionLightingPipeline::Shutdown() {
    const auto unmap = [](ComPtr<ID3D12Resource>& resource, void*& pointer) {
        if (resource && pointer != nullptr) resource->Unmap(0, nullptr);
        pointer = nullptr;
        resource.Reset();
    };
    void* pointer = mappedLights_; unmap(lightBuffer_, pointer); mappedLights_ = nullptr;
    pointer = mappedClusterRanges_; unmap(clusterRangeBuffer_, pointer); mappedClusterRanges_ = nullptr;
    pointer = mappedClusterIndices_; unmap(clusterIndexBuffer_, pointer); mappedClusterIndices_ = nullptr;
    pointer = mappedConstants_; unmap(constantsBuffer_, pointer); mappedConstants_ = nullptr;
    pointer = mappedShadowDraws_; unmap(shadowDrawBuffer_, pointer); mappedShadowDraws_ = nullptr;
    shadowAtlas_.Reset();
    shadowDsvHeap_.Reset();
    shadowPipelineState_.Reset();
    shadowVertexShader_.Reset();
    rootSignature_.Reset();
    sharedSrvHeap_.Reset();
    device_.Reset();
    shadowDsvs_.clear();
    shadowAtlasGpu_ = {};
    shadowAtlasState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    lights_.clear();
    clusterRanges_.clear();
    clusterIndices_.clear();
    shadowAllocations_.clear();
    constants_ = {};
    stats_ = {};
    diagnostics_.clear();
}

bool EditorProductionLightingPipeline::CreateGpuResources(std::string* errorMessage) {
    const uint64_t maximumClusters = static_cast<uint64_t>(policy_.maximumTileCountX) *
        policy_.maximumTileCountY * policy_.depthSliceCount;
    if (!CreateUploadBuffer(device_.Get(),
            static_cast<uint64_t>(policy_.maximumVisibleLights) * sizeof(EditorProductionGpuLight),
            lightBuffer_, &mappedLights_, errorMessage) ||
        !CreateUploadBuffer(device_.Get(), maximumClusters * sizeof(EditorProductionClusterRange),
            clusterRangeBuffer_, &mappedClusterRanges_, errorMessage) ||
        !CreateUploadBuffer(device_.Get(), maximumClusters * policy_.maximumLightsPerCluster * sizeof(uint32_t),
            clusterIndexBuffer_, &mappedClusterIndices_, errorMessage)) return false;
    uint8_t* constants = nullptr;
    if (!CreateUploadBuffer(device_.Get(), sizeof(EditorProductionLightingConstants),
            constantsBuffer_, &constants, errorMessage)) return false;
    mappedConstants_ = reinterpret_cast<EditorProductionLightingConstants*>(constants);
    if (!CreateUploadBuffer(device_.Get(),
            static_cast<uint64_t>(policy_.maximumShadowMaps) * kConstantAlignment,
            shadowDrawBuffer_, &mappedShadowDraws_, errorMessage)) return false;
    return true;
}

bool EditorProductionLightingPipeline::CreateShadowResources(std::string* errorMessage) {
    const uint64_t bytesPerMap = static_cast<uint64_t>(policy_.shadowMapSize) *
        policy_.shadowMapSize * sizeof(float);
    if (bytesPerMap * policy_.maximumShadowMaps > policy_.shadowBudgetBytes) {
        policy_.maximumShadowMaps = static_cast<uint32_t>((std::max)(1ull,
            policy_.shadowBudgetBytes / (std::max)(1ull, bytesPerMap)));
        policy_.maximumShadowMaps = (std::min)(policy_.maximumShadowMaps,
            kEditorProductionMaximumShadowMaps);
    }
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC description{};
    description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    description.Width = policy_.shadowMapSize;
    description.Height = policy_.shadowMapSize;
    description.DepthOrArraySize = static_cast<UINT16>(policy_.maximumShadowMaps);
    description.MipLevels = 1;
    description.Format = DXGI_FORMAT_R32_TYPELESS;
    description.SampleDesc.Count = 1;
    description.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    D3D12_CLEAR_VALUE clear{};
    clear.Format = DXGI_FORMAT_D32_FLOAT;
    clear.DepthStencil.Depth = 1.0f;
    if (FAILED(device_->CreateCommittedResource(
            &heap, D3D12_HEAP_FLAG_NONE, &description,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear,
            IID_PPV_ARGS(shadowAtlas_.ReleaseAndGetAddressOf())))) {
        SetError(errorMessage, "E-10 failed to allocate the shadow texture array.");
        return false;
    }
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    heapDesc.NumDescriptors = policy_.maximumShadowMaps;
    if (FAILED(device_->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&shadowDsvHeap_)))) {
        SetError(errorMessage, "E-10 failed to allocate the shadow DSV heap.");
        return false;
    }
    const uint32_t dsvSize = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    const D3D12_CPU_DESCRIPTOR_HANDLE dsvStart = shadowDsvHeap_->GetCPUDescriptorHandleForHeapStart();
    shadowDsvs_.resize(policy_.maximumShadowMaps);
    for (uint32_t slice = 0; slice < policy_.maximumShadowMaps; ++slice) {
        shadowDsvs_[slice].ptr = dsvStart.ptr + static_cast<SIZE_T>(slice) * dsvSize;
        D3D12_DEPTH_STENCIL_VIEW_DESC dsv{};
        dsv.Format = DXGI_FORMAT_D32_FLOAT;
        dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
        dsv.Texture2DArray.ArraySize = 1;
        dsv.Texture2DArray.FirstArraySlice = slice;
        device_->CreateDepthStencilView(shadowAtlas_.Get(), &dsv, shadowDsvs_[slice]);
    }
    stats_.shadowAtlasBytes = bytesPerMap * policy_.maximumShadowMaps;
    return true;
}

bool EditorProductionLightingPipeline::CreateShadowPipeline(std::string* errorMessage) {
    ge3::core::ShaderCompiler compiler;
    if (!compiler.Initialize()) {
        SetError(errorMessage, "E-10 failed to initialize DXC for the shadow pipeline.");
        return false;
    }
    shadowVertexShader_ = compiler.CompileFromFile(
        L"Resources/ProductionShadow.VS.hlsl", L"main", L"vs_6_0");
    if (shadowVertexShader_ == nullptr) {
        SetError(errorMessage, "E-10 failed to compile Resources/ProductionShadow.VS.hlsl.");
        return false;
    }
    static const D3D12_INPUT_ELEMENT_DESC inputElements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC description{};
    description.pRootSignature = rootSignature_.Get();
    description.VS = {shadowVertexShader_->GetBufferPointer(), shadowVertexShader_->GetBufferSize()};
    description.BlendState.AlphaToCoverageEnable = FALSE;
    description.BlendState.IndependentBlendEnable = FALSE;
    description.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    description.SampleMask = UINT_MAX;
    description.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    description.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    description.RasterizerState.DepthClipEnable = TRUE;
    description.RasterizerState.DepthBias = 1000;
    description.RasterizerState.SlopeScaledDepthBias = 1.5f;
    description.DepthStencilState.DepthEnable = TRUE;
    description.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    description.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    description.InputLayout = {inputElements, _countof(inputElements)};
    description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    description.NumRenderTargets = 0;
    description.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    description.SampleDesc.Count = 1;
    if (FAILED(device_->CreateGraphicsPipelineState(
            &description, IID_PPV_ARGS(shadowPipelineState_.ReleaseAndGetAddressOf())))) {
        SetError(errorMessage, "E-10 failed to create the shadow PSO.");
        return false;
    }
    return true;
}

bool EditorProductionLightingPipeline::Sync(
    const EditorScene& scene,
    const Vector3& cameraWorldPosition,
    const Matrix4x4& view,
    const Matrix4x4& projection,
    const Matrix4x4& viewProjection,
    uint32_t viewportWidth,
    uint32_t viewportHeight,
    float nearPlane,
    float farPlane,
    std::string* errorMessage,
    const std::unordered_set<std::string>* sourceResidentEntities) {
    policy_.maximumVisibleLights = (std::max)(1u, policy_.maximumVisibleLights);
    policy_.tileSizePixels = (std::max)(8u, policy_.tileSizePixels);
    policy_.depthSliceCount = (std::max)(1u, policy_.depthSliceCount);
    policy_.maximumTileCountX = (std::max)(1u, policy_.maximumTileCountX);
    policy_.maximumTileCountY = (std::max)(1u, policy_.maximumTileCountY);
    policy_.maximumLightsPerCluster = (std::max)(1u, policy_.maximumLightsPerCluster);
    policy_.maximumShadowMaps = (std::min)(kEditorProductionMaximumShadowMaps,
        (std::max)(1u, policy_.maximumShadowMaps));
    lights_.clear();
    clusterRanges_.clear();
    clusterIndices_.clear();
    shadowAllocations_.clear();
    diagnostics_.clear();
    const uint64_t atlasBytes = stats_.shadowAtlasBytes;
    stats_ = {};
    stats_.shadowAtlasBytes = atlasBytes;

    std::unordered_map<std::string, Matrix4x4> worlds;
    std::unordered_map<std::string, bool> hierarchyVisible;
    std::unordered_set<std::string> visiting;
    std::unordered_set<std::string> visibilityVisiting;
    const auto resolveWorld = [&](const auto& self, const EditorSceneEntity& entity) -> Matrix4x4 {
        if (const auto found = worlds.find(entity.guid); found != worlds.end()) return found->second;
        if (!visiting.insert(entity.guid).second) return MakeIdentity4x4();
        const EditorSceneComponent* transform = scene.FindComponent(entity, kEditorTransformComponentType);
        const Vector3 translation = transform ? VectorProperty(*transform, "translation", {}) : Vector3{};
        const Vector3 rotation = transform ? VectorProperty(*transform, "rotation", {}) : Vector3{};
        const Vector3 scale = transform ? VectorProperty(*transform, "scale", {1.0f, 1.0f, 1.0f}) : Vector3{1.0f, 1.0f, 1.0f};
        Matrix4x4 world = MakeAffineMatrix(scale, rotation, translation);
        if (!entity.parentGuid.empty()) {
            if (const EditorSceneEntity* parent = scene.FindEntity(entity.parentGuid))
                world = Multiply(world, self(self, *parent));
        }
        visiting.erase(entity.guid);
        worlds.insert_or_assign(entity.guid, world);
        return world;
    };
    const auto resolveVisible = [&](const auto& self, const EditorSceneEntity& entity) -> bool {
        if (const auto found = hierarchyVisible.find(entity.guid); found != hierarchyVisible.end())
            return found->second;
        if (!visibilityVisiting.insert(entity.guid).second) return false;
        bool visible = entity.visible;
        if (visible && !entity.parentGuid.empty()) {
            if (const EditorSceneEntity* parent = scene.FindEntity(entity.parentGuid))
                visible = self(self, *parent);
        }
        visibilityVisiting.erase(entity.guid);
        hierarchyVisible.insert_or_assign(entity.guid, visible);
        return visible;
    };

    std::vector<LightCandidate> candidates;
    for (const EditorSceneEntity& entity : scene.entities) {
        if (sourceResidentEntities != nullptr &&
            !sourceResidentEntities->contains(entity.guid)) continue;
        if (!resolveVisible(resolveVisible, entity)) continue;
        const auto append = [&](std::string_view typeId, EditorProductionLightType type, bool defaultShadow) {
            const EditorSceneComponent* component = scene.FindComponent(entity, typeId);
            if (component == nullptr || !component->enabled) return;
            LightCandidate candidate{};
            candidate.entity = &entity;
            candidate.component = component;
            candidate.type = type;
            candidate.world = resolveWorld(resolveWorld, entity);
            candidate.priority = FloatProperty(*component, "priority", 0.0f);
            candidate.shadowPriority = FloatProperty(*component, "shadowPriority", candidate.priority);
            candidate.castsShadow = BoolProperty(*component, "castsShadow", defaultShadow);
            candidates.push_back(std::move(candidate));
        };
        append(kEditorDirectionalLightComponentType, EditorProductionLightType::Directional, true);
        append(kEditorPointLightComponentType, EditorProductionLightType::Point, false);
        append(kEditorSpotLightComponentType, EditorProductionLightType::Spot, true);
    }
    stats_.submittedLights = static_cast<uint32_t>(candidates.size());
    std::sort(candidates.begin(), candidates.end(), [](const LightCandidate& a, const LightCandidate& b) {
        if (a.priority != b.priority) return a.priority > b.priority;
        return a.entity->guid < b.entity->guid;
    });
    if (candidates.size() > policy_.maximumVisibleLights) {
        stats_.rejectedByLightBudget = static_cast<uint32_t>(
            candidates.size() - policy_.maximumVisibleLights);
        candidates.resize(policy_.maximumVisibleLights);
        diagnostics_.push_back("E-10 visible Light budget was exceeded; lowest-priority Lights use deterministic fallback rejection.");
    }

    lights_.reserve(candidates.size());
    for (const LightCandidate& candidate : candidates) {
        const EditorSceneComponent& component = *candidate.component;
        const Vector4 color = ColorProperty(component);
        const float intensity = Clamp(FloatProperty(component, "intensity", 1.0f), 0.0f, 100000.0f);
        const Vector3 position{candidate.world.m[3][0], candidate.world.m[3][1], candidate.world.m[3][2]};
        Vector3 direction = VectorProperty(component, "direction", {0.0f, -1.0f, 0.0f});
        direction = NormalizeOr(TransformDirection(direction, candidate.world), {0.0f, -1.0f, 0.0f});
        float range = 1000000.0f;
        float decay = 0.0f;
        float cosAngle = -1.0f;
        if (candidate.type == EditorProductionLightType::Point) {
            range = Clamp(FloatProperty(component, "radius", 10.0f), 0.001f, 1000000.0f);
            decay = Clamp(FloatProperty(component, "decay", 2.0f), 0.0f, 64.0f);
        } else if (candidate.type == EditorProductionLightType::Spot) {
            range = Clamp(FloatProperty(component, "distance", 10.0f), 0.001f, 1000000.0f);
            decay = Clamp(FloatProperty(component, "decay", 2.0f), 0.0f, 64.0f);
            const float angle = Clamp(FloatProperty(component, "angle", 30.0f), 0.1f, 89.9f);
            cosAngle = std::cos(angle * kPi / 180.0f);
        }
        EditorProductionGpuLight light{};
        light.colorIntensity = {color.x, color.y, color.z, intensity};
        light.positionRange = {position.x, position.y, position.z, range};
        light.directionType = {direction.x, direction.y, direction.z,
            static_cast<float>(candidate.type)};
        light.attenuationShadow = {decay, cosAngle, -1.0f, candidate.priority};
        lights_.push_back(light);
    }
    stats_.visibleLights = static_cast<uint32_t>(lights_.size());

    std::vector<uint32_t> shadowOrder;
    for (uint32_t index = 0; index < candidates.size(); ++index) {
        if (!candidates[index].castsShadow) continue;
        ++stats_.shadowRequests;
        if (candidates[index].type == EditorProductionLightType::Point) {
            ++stats_.rejectedByShadowBudget;
            diagnostics_.push_back("E-10 point-light cube shadows are rejected by policy; directional/spot atlas residency remains valid.");
            continue;
        }
        shadowOrder.push_back(index);
    }
    std::sort(shadowOrder.begin(), shadowOrder.end(), [&](uint32_t a, uint32_t b) {
        if (candidates[a].shadowPriority != candidates[b].shadowPriority)
            return candidates[a].shadowPriority > candidates[b].shadowPriority;
        return candidates[a].entity->guid < candidates[b].entity->guid;
    });
    if (shadowOrder.size() > policy_.maximumShadowMaps) {
        stats_.rejectedByShadowBudget += static_cast<uint32_t>(
            shadowOrder.size() - policy_.maximumShadowMaps);
        shadowOrder.resize(policy_.maximumShadowMaps);
        diagnostics_.push_back("E-10 shadow atlas budget was exceeded; lowest shadow-priority requests render unshadowed.");
    }
    for (uint32_t slot = 0; slot < shadowOrder.size(); ++slot) {
        const uint32_t lightIndex = shadowOrder[slot];
        const LightCandidate& candidate = candidates[lightIndex];
        const EditorProductionGpuLight& light = lights_[lightIndex];
        const Vector3 position{light.positionRange.x, light.positionRange.y, light.positionRange.z};
        const Vector3 direction{light.directionType.x, light.directionType.y, light.directionType.z};
        Matrix4x4 lightView{};
        Matrix4x4 lightProjection{};
        if (candidate.type == EditorProductionLightType::Directional) {
            const Vector3 eye{cameraWorldPosition.x - direction.x * 100.0f,
                cameraWorldPosition.y - direction.y * 100.0f,
                cameraWorldPosition.z - direction.z * 100.0f};
            lightView = LookAt(eye, cameraWorldPosition, {0.0f, 1.0f, 0.0f});
            lightProjection = MakeOrthographicMatrix(-75.0f, 75.0f, 75.0f, -75.0f, 0.1f, 300.0f);
        } else {
            const Vector3 target{position.x + direction.x, position.y + direction.y, position.z + direction.z};
            lightView = LookAt(position, target, {0.0f, 1.0f, 0.0f});
            const float angle = std::acos(Clamp(light.attenuationShadow.y, -0.999f, 0.999f));
            lightProjection = MakePerspectiveFovMatrix(
                (std::max)(0.01f, angle * 2.0f), 1.0f, 0.05f,
                (std::max)(0.1f, light.positionRange.w));
        }
        EditorProductionShadowAllocation allocation{};
        allocation.entityGuid = candidate.entity->guid;
        allocation.type = candidate.type;
        allocation.lightIndex = lightIndex;
        allocation.atlasSlice = slot;
        allocation.lightViewProjection = Multiply(lightView, lightProjection);
        shadowAllocations_.push_back(allocation);
        lights_[lightIndex].attenuationShadow.z = static_cast<float>(slot);
    }
    stats_.residentShadowMaps = static_cast<uint32_t>(shadowAllocations_.size());

    viewportWidth = (std::max)(1u, viewportWidth);
    viewportHeight = (std::max)(1u, viewportHeight);
    nearPlane = (std::max)(0.001f, nearPlane);
    farPlane = (std::max)(nearPlane + 0.001f, farPlane);
    constants_ = {};
    constants_.tileCountX = (std::min)(policy_.maximumTileCountX,
        (viewportWidth + policy_.tileSizePixels - 1) / policy_.tileSizePixels);
    constants_.tileCountY = (std::min)(policy_.maximumTileCountY,
        (viewportHeight + policy_.tileSizePixels - 1) / policy_.tileSizePixels);
    constants_.sliceCount = policy_.depthSliceCount;
    constants_.lightCount = static_cast<uint32_t>(lights_.size());
    constants_.maxLightsPerCluster = policy_.maximumLightsPerCluster;
    constants_.shadowCount = static_cast<uint32_t>(shadowAllocations_.size());
    constants_.nearPlane = nearPlane;
    constants_.farPlane = farPlane;
    constants_.viewportAndInverse = {static_cast<float>(viewportWidth), static_cast<float>(viewportHeight),
        1.0f / viewportWidth, 1.0f / viewportHeight};
    constants_.clusterParameters = {static_cast<float>(policy_.tileSizePixels), 0.0f, 0.0f, 0.0f};
    constants_.cameraPosition = {cameraWorldPosition.x, cameraWorldPosition.y, cameraWorldPosition.z, 1.0f};
    for (const EditorProductionShadowAllocation& shadow : shadowAllocations_) {
        constants_.shadowViewProjection[shadow.atlasSlice] = shadow.lightViewProjection;
        constants_.shadowParameters[shadow.atlasSlice] = {
            0.0015f, 1.0f, 1.0f / static_cast<float>(policy_.shadowMapSize), 1.0f};
    }

    const uint32_t clusterCount = constants_.tileCountX * constants_.tileCountY * constants_.sliceCount;
    std::vector<std::vector<uint32_t>> lists(clusterCount);
    const auto appendRange = [&](uint32_t lightIndex, uint32_t minX, uint32_t maxX,
                                 uint32_t minY, uint32_t maxY, uint32_t minZ, uint32_t maxZ) {
        for (uint32_t z = minZ; z <= maxZ; ++z) {
            for (uint32_t y = minY; y <= maxY; ++y) {
                for (uint32_t x = minX; x <= maxX; ++x) {
                    auto& list = lists[(z * constants_.tileCountY + y) * constants_.tileCountX + x];
                    if (list.size() < policy_.maximumLightsPerCluster) list.push_back(lightIndex);
                    else ++stats_.clusterOverflowCount;
                }
            }
        }
    };
    for (uint32_t lightIndex = 0; lightIndex < lights_.size(); ++lightIndex) {
        const EditorProductionGpuLight& light = lights_[lightIndex];
        const EditorProductionLightType type = static_cast<EditorProductionLightType>(
            static_cast<uint32_t>(light.directionType.w + 0.5f));
        if (type == EditorProductionLightType::Directional) {
            appendRange(lightIndex, 0, constants_.tileCountX - 1, 0, constants_.tileCountY - 1,
                0, constants_.sliceCount - 1);
            continue;
        }
        const Vector3 position{light.positionRange.x, light.positionRange.y, light.positionRange.z};
        const Vector3 viewPosition = TransformPointNoDivide(position, view);
        const float range = light.positionRange.w;
        if (viewPosition.z + range < nearPlane || viewPosition.z - range > farPlane) continue;
        const float clipX = position.x * viewProjection.m[0][0] +
            position.y * viewProjection.m[1][0] + position.z * viewProjection.m[2][0] + viewProjection.m[3][0];
        const float clipY = position.x * viewProjection.m[0][1] +
            position.y * viewProjection.m[1][1] + position.z * viewProjection.m[2][1] + viewProjection.m[3][1];
        const float clipW = position.x * viewProjection.m[0][3] +
            position.y * viewProjection.m[1][3] + position.z * viewProjection.m[2][3] + viewProjection.m[3][3];
        if (std::abs(clipW) <= 1.0e-6f) continue;
        const float centerX = (clipX / clipW * 0.5f + 0.5f) * viewportWidth;
        const float centerY = (0.5f - clipY / clipW * 0.5f) * viewportHeight;
        const float radiusPixels = range / (std::max)(nearPlane, std::abs(viewPosition.z)) *
            std::abs(projection.m[1][1]) * static_cast<float>(viewportHeight) * 0.5f;
        const auto tile = [&](float pixel, uint32_t count) {
            const float positivePixel = (std::max)(0.0f, pixel);
            const uint32_t value = static_cast<uint32_t>(
                positivePixel / static_cast<float>(policy_.tileSizePixels));
            return (std::min)(count - 1, value);
        };
        const uint32_t minX = tile(centerX - radiusPixels, constants_.tileCountX);
        const uint32_t maxX = tile(centerX + radiusPixels, constants_.tileCountX);
        const uint32_t minY = tile(centerY - radiusPixels, constants_.tileCountY);
        const uint32_t maxY = tile(centerY + radiusPixels, constants_.tileCountY);
        const uint32_t minZ = DepthSlice(viewPosition.z - range, nearPlane, farPlane, constants_.sliceCount);
        const uint32_t maxZ = DepthSlice(viewPosition.z + range, nearPlane, farPlane, constants_.sliceCount);
        appendRange(lightIndex, (std::min)(minX, maxX), (std::max)(minX, maxX),
            (std::min)(minY, maxY), (std::max)(minY, maxY),
            (std::min)(minZ, maxZ), (std::max)(minZ, maxZ));
    }
    clusterRanges_.resize(clusterCount);
    for (uint32_t cluster = 0; cluster < clusterCount; ++cluster) {
        clusterRanges_[cluster].offset = static_cast<uint32_t>(clusterIndices_.size());
        clusterRanges_[cluster].count = static_cast<uint32_t>(lists[cluster].size());
        clusterIndices_.insert(clusterIndices_.end(), lists[cluster].begin(), lists[cluster].end());
    }
    stats_.clusterCount = clusterCount;
    stats_.clusterIndexCount = static_cast<uint32_t>(clusterIndices_.size());
    stats_.lightBufferBytes = lights_.size() * sizeof(EditorProductionGpuLight);
    stats_.clusterBufferBytes = clusterRanges_.size() * sizeof(EditorProductionClusterRange) +
        clusterIndices_.size() * sizeof(uint32_t);
    UploadCpuData();
    if (!diagnostics_.empty() && errorMessage != nullptr) *errorMessage = diagnostics_.front();
    return true;
}

void EditorProductionLightingPipeline::UploadCpuData() {
    if (mappedLights_ != nullptr && !lights_.empty())
        std::memcpy(mappedLights_, lights_.data(), lights_.size() * sizeof(lights_.front()));
    if (mappedClusterRanges_ != nullptr && !clusterRanges_.empty())
        std::memcpy(mappedClusterRanges_, clusterRanges_.data(),
            clusterRanges_.size() * sizeof(clusterRanges_.front()));
    if (mappedClusterIndices_ != nullptr && !clusterIndices_.empty())
        std::memcpy(mappedClusterIndices_, clusterIndices_.data(),
            clusterIndices_.size() * sizeof(clusterIndices_.front()));
    if (mappedConstants_ != nullptr) *mappedConstants_ = constants_;
    if (mappedShadowDraws_ != nullptr) {
        for (const EditorProductionShadowAllocation& shadow : shadowAllocations_) {
            std::memcpy(mappedShadowDraws_ + static_cast<uint64_t>(shadow.atlasSlice) * kConstantAlignment,
                &shadow.lightViewProjection, sizeof(Matrix4x4));
        }
    }
}

void EditorProductionLightingPipeline::RenderShadowMaps(
    ID3D12GraphicsCommandList* commandList,
    const std::vector<EditorProductionSceneRenderPacket>& packets) {
    stats_.renderedShadowDraws = 0;
    if (commandList == nullptr || shadowAtlas_ == nullptr || shadowPipelineState_ == nullptr ||
        rootSignature_ == nullptr || shadowAllocations_.empty()) return;
    if (shadowAtlasState_ != D3D12_RESOURCE_STATE_DEPTH_WRITE) {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = shadowAtlas_.Get();
        barrier.Transition.StateBefore = shadowAtlasState_;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &barrier);
        shadowAtlasState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    }
    D3D12_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(policy_.shadowMapSize);
    viewport.Height = static_cast<float>(policy_.shadowMapSize);
    viewport.MaxDepth = 1.0f;
    D3D12_RECT scissor{0, 0, static_cast<LONG>(policy_.shadowMapSize),
        static_cast<LONG>(policy_.shadowMapSize)};
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissor);
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(shadowPipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    for (const EditorProductionShadowAllocation& shadow : shadowAllocations_) {
        commandList->OMSetRenderTargets(0, nullptr, FALSE, &shadowDsvs_[shadow.atlasSlice]);
        commandList->ClearDepthStencilView(shadowDsvs_[shadow.atlasSlice],
            D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        commandList->SetGraphicsRootConstantBufferView(17,
            shadowDrawBuffer_->GetGPUVirtualAddress() +
                static_cast<uint64_t>(shadow.atlasSlice) * kConstantAlignment);
        for (const EditorProductionSceneRenderPacket& packet : packets) {
            if (packet.indexCount == 0 || packet.transformAddress == 0 ||
                packet.vertexBuffer.BufferLocation == 0 || packet.indexBuffer.BufferLocation == 0) continue;
            commandList->SetGraphicsRootConstantBufferView(1, packet.transformAddress);
            commandList->IASetVertexBuffers(0, 1, &packet.vertexBuffer);
            commandList->IASetIndexBuffer(&packet.indexBuffer);
            commandList->DrawIndexedInstanced(packet.indexCount, 1, 0, 0, 0);
            ++stats_.renderedShadowDraws;
        }
    }
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = shadowAtlas_.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);
    shadowAtlasState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

D3D12_GPU_VIRTUAL_ADDRESS EditorProductionLightingPipeline::LightBufferAddress() const noexcept {
    return lightBuffer_ ? lightBuffer_->GetGPUVirtualAddress() : 0;
}
D3D12_GPU_VIRTUAL_ADDRESS EditorProductionLightingPipeline::ClusterRangeBufferAddress() const noexcept {
    return clusterRangeBuffer_ ? clusterRangeBuffer_->GetGPUVirtualAddress() : 0;
}
D3D12_GPU_VIRTUAL_ADDRESS EditorProductionLightingPipeline::ClusterIndexBufferAddress() const noexcept {
    return clusterIndexBuffer_ ? clusterIndexBuffer_->GetGPUVirtualAddress() : 0;
}
D3D12_GPU_VIRTUAL_ADDRESS EditorProductionLightingPipeline::ConstantsAddress() const noexcept {
    return constantsBuffer_ ? constantsBuffer_->GetGPUVirtualAddress() : 0;
}

} // namespace editor
