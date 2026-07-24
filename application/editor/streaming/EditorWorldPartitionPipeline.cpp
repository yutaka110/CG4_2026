#include "EditorWorldPartitionPipeline.h"

#include "../../../externals/DirectXTex/d3dx12.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>

namespace editor {
namespace {

using Microsoft::WRL::ComPtr;
constexpr float kEpsilon = 1.0e-6f;

void SetError(std::string* output, std::string value) {
    if (output != nullptr) *output = std::move(value);
}

uint64_t HashAppend(uint64_t value, uint64_t part) {
    value ^= part + 0x9e3779b97f4a7c15ull + (value << 6) + (value >> 2);
    return value;
}

uint64_t HashText(std::string_view value) {
    uint64_t hash = 1469598103934665603ull;
    for (unsigned char character : value) {
        hash ^= character;
        hash *= 1099511628211ull;
    }
    return hash;
}

const EditorSceneProperty* Property(
    const EditorSceneComponent* component, std::string_view name) {
    if (component == nullptr) return nullptr;
    const auto found = std::find_if(component->properties.begin(), component->properties.end(),
        [&](const EditorSceneProperty& value) { return value.name == name; });
    return found == component->properties.end() ? nullptr : &*found;
}

Vector3 VectorProperty(
    const EditorSceneComponent* component, std::string_view name, Vector3 fallback) {
    const EditorSceneProperty* property = Property(component, name);
    if (property == nullptr) return fallback;
    std::istringstream input(property->value);
    Vector3 value{};
    if (!(input >> value.x >> value.y >> value.z) || !std::isfinite(value.x) ||
        !std::isfinite(value.y) || !std::isfinite(value.z)) return fallback;
    return value;
}

int32_t IntegerProperty(
    const EditorSceneComponent* component, std::string_view name, int32_t fallback) {
    const EditorSceneProperty* property = Property(component, name);
    if (property == nullptr) return fallback;
    std::istringstream input(property->value);
    int32_t value = fallback;
    return input >> value ? value : fallback;
}

bool BooleanProperty(
    const EditorSceneComponent* component, std::string_view name, bool fallback) {
    const EditorSceneProperty* property = Property(component, name);
    if (property == nullptr) return fallback;
    if (property->value == "1" || property->value == "true" || property->value == "True") return true;
    if (property->value == "0" || property->value == "false" || property->value == "False") return false;
    return fallback;
}

std::string TextProperty(
    const EditorSceneComponent* component, std::string_view name, std::string fallback) {
    const EditorSceneProperty* property = Property(component, name);
    return property == nullptr || property->value.empty() ? std::move(fallback) : property->value;
}

std::string AssetReference(const EditorSceneComponent& component) {
    const auto found = std::find_if(component.references.begin(), component.references.end(),
        [](const EditorSceneObjectReference& value) { return value.property == "asset"; });
    return found == component.references.end() ? std::string{} : found->assetGuid;
}

Vector3 TransformPoint(const Vector3& value, const Matrix4x4& matrix) {
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

Vector3 NormalizeLocal(Vector3 value) {
    const float length = std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    if (length <= kEpsilon) return {0.0f, 1.0f, 0.0f};
    return {value.x / length, value.y / length, value.z / length};
}

float Length(Vector3 value) {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

bool SphereVisible(Vector3 center, float radius, const Matrix4x4& matrix) {
    const float x = center.x * matrix.m[0][0] + center.y * matrix.m[1][0] +
        center.z * matrix.m[2][0] + matrix.m[3][0];
    const float y = center.x * matrix.m[0][1] + center.y * matrix.m[1][1] +
        center.z * matrix.m[2][1] + matrix.m[3][1];
    const float z = center.x * matrix.m[0][2] + center.y * matrix.m[1][2] +
        center.z * matrix.m[2][2] + matrix.m[3][2];
    const float w = center.x * matrix.m[0][3] + center.y * matrix.m[1][3] +
        center.z * matrix.m[2][3] + matrix.m[3][3];
    if (w <= kEpsilon) return false;
    const float projected = radius * (std::max)(std::abs(matrix.m[0][0]),
        std::abs(matrix.m[1][1]));
    return x >= -w - projected && x <= w + projected &&
        y >= -w - projected && y <= w + projected &&
        z >= -projected && z <= w + projected;
}

bool CreateBuffer(ID3D12Device* device, D3D12_HEAP_TYPE heapType, uint64_t bytes,
    D3D12_RESOURCE_STATES state, ComPtr<ID3D12Resource>& output) {
    if (device == nullptr || bytes == 0) return false;
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = heapType;
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = (bytes + 255ull) & ~255ull;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return SUCCEEDED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE,
        &desc, state, nullptr, IID_PPV_ARGS(output.ReleaseAndGetAddressOf())));
}

} // namespace

struct EditorWorldPartitionPipeline::HlodBuildInput {
    struct Source {
        std::vector<EditorCookedMeshVertex> vertices;
        std::vector<uint32_t> indices;
        Matrix4x4 world = MakeIdentity4x4();
    };
    EditorWorldPartitionCellKey key{};
    uint64_t fingerprint = 0;
    uint32_t maximumVertices = 0;
    uint32_t maximumTriangles = 0;
    std::vector<Source> sources;
};

struct EditorWorldPartitionPipeline::HlodBuildResult {
    EditorWorldPartitionCellKey key{};
    uint64_t fingerprint = 0;
    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices;
    Vector3 boundsMin{};
    Vector3 boundsMax{};
    bool succeeded = false;
    std::string diagnostic;
};

struct EditorWorldPartitionPipeline::PendingHlodBuild {
    EditorWorldPartitionCellKey key{};
    uint64_t fingerprint = 0;
    std::future<HlodBuildResult> future;
};

struct EditorWorldPartitionPipeline::ResidentHlod {
    uint64_t fingerprint = 0;
    ComPtr<ID3D12Resource> vertexResource;
    ComPtr<ID3D12Resource> indexResource;
    ComPtr<ID3D12Resource> transformResource;
    TransformationMatrix* mappedTransform = nullptr;
    D3D12_VERTEX_BUFFER_VIEW vertexBuffer{};
    D3D12_INDEX_BUFFER_VIEW indexBuffer{};
    uint32_t indexCount = 0;
    Vector3 boundsMin{};
    Vector3 boundsMax{};
    uint64_t gpuBytes = 0;
    uint64_t lastUsedFrame = 0;
};

struct EditorWorldPartitionPipeline::PendingResource {
    ComPtr<ID3D12Resource> resource;
    uint64_t fenceValue = 0;
};

struct EditorWorldPartitionPipeline::CellRuntime {
    EditorWorldPartitionCellState state = EditorWorldPartitionCellState::Unloaded;
    uint64_t stateFrame = 0;
    uint64_t fingerprint = 0;
    bool manifestPending = false;
    std::future<bool> manifestFuture;
};

bool EditorWorldPartitionCellKey::operator<(
    const EditorWorldPartitionCellKey& other) const noexcept {
    if (dataLayer != other.dataLayer) return dataLayer < other.dataLayer;
    if (x != other.x) return x < other.x;
    return z < other.z;
}

std::string EditorWorldPartitionCellKey::StableName() const {
    return dataLayer + ":" + std::to_string(x) + ":" + std::to_string(z);
}

std::size_t EditorWorldPartitionCellKeyHash::operator()(
    const EditorWorldPartitionCellKey& key) const noexcept {
    uint64_t hash = HashText(key.dataLayer);
    hash = HashAppend(hash, static_cast<uint32_t>(key.x));
    hash = HashAppend(hash, static_cast<uint32_t>(key.z));
    return static_cast<std::size_t>(hash);
}

EditorWorldPartitionPipeline::EditorWorldPartitionPipeline() = default;
EditorWorldPartitionPipeline::~EditorWorldPartitionPipeline() { Shutdown(); }

EditorWorldPartitionCellKey EditorWorldPartitionPipeline::CellForPosition(
    const Vector3& position, float cellSize, std::string dataLayer) {
    const float safeSize = (std::max)(1.0f, cellSize);
    return {static_cast<int32_t>(std::floor(position.x / safeSize)),
        static_cast<int32_t>(std::floor(position.z / safeSize)),
        dataLayer.empty() ? "Default" : std::move(dataLayer)};
}

uint32_t EditorWorldPartitionPipeline::ChebyshevDistance(
    const EditorWorldPartitionCellKey& a,
    const EditorWorldPartitionCellKey& b) noexcept {
    const uint64_t dx = static_cast<uint64_t>(std::abs(int64_t(a.x) - int64_t(b.x)));
    const uint64_t dz = static_cast<uint64_t>(std::abs(int64_t(a.z) - int64_t(b.z)));
    return static_cast<uint32_t>((std::min<uint64_t>)((std::max)(dx, dz), UINT32_MAX));
}

bool EditorWorldPartitionPipeline::Initialize(
    ID3D12Device* device, EditorWorldPartitionPolicy policy, std::string* errorMessage) {
    Shutdown();
    if (device == nullptr) {
        SetError(errorMessage, "E-12 requires a D3D12 device.");
        return false;
    }
    policy.cellSize = (std::max)(1.0f, policy.cellSize);
    policy.sourceUnloadRadiusCells = (std::max)(
        policy.sourceLoadRadiusCells, policy.sourceUnloadRadiusCells);
    policy.hlodRadiusCells = (std::max)(policy.sourceUnloadRadiusCells,
        policy.hlodRadiusCells);
    policy.maximumSourceCells = (std::max)(1u, policy.maximumSourceCells);
    policy.maximumSourceEntities = (std::max)(1u, policy.maximumSourceEntities);
    policy.maximumHlodProxies = (std::max)(1u, policy.maximumHlodProxies);
    policy.maximumConcurrentBuilds = (std::max)(1u, policy.maximumConcurrentBuilds);
    policy.maximumHlodVertices = (std::max)(3u, policy.maximumHlodVertices);
    policy.maximumHlodTriangles = (std::max)(1u, policy.maximumHlodTriangles);
    policy_ = policy;
    device_ = device;
    return true;
}

void EditorWorldPartitionPipeline::Shutdown() {
    for (auto& pending : pendingBuilds_) {
        if (pending && pending->future.valid()) pending->future.wait();
    }
    pendingBuilds_.clear();
    for (auto& [key, runtime] : runtimeCells_) {
        (void)key;
        if (runtime.manifestPending && runtime.manifestFuture.valid()) runtime.manifestFuture.wait();
    }
    for (auto& [key, hlod] : residentHlods_) {
        (void)key;
        if (hlod.transformResource && hlod.mappedTransform) hlod.transformResource->Unmap(0, nullptr);
    }
    runtimeCells_.clear();
    residentHlods_.clear();
    pendingResources_.clear();
    sourceResidentEntities_.clear();
    cells_.clear();
    crossCellReferences_.clear();
    hlodPackets_.clear();
    diagnostics_.clear();
    stats_ = {};
    frameIndex_ = 0;
    device_.Reset();
}

bool EditorWorldPartitionPipeline::IsEntitySourceResident(std::string_view entityGuid) const {
    return sourceResidentEntities_.contains(std::string(entityGuid));
}

void EditorWorldPartitionPipeline::CollectRetired(uint64_t completedFenceValue) {
    std::erase_if(pendingResources_, [&](const PendingResource& resource) {
        return resource.fenceValue <= completedFenceValue;
    });
}

void EditorWorldPartitionPipeline::RetireHlod(
    ResidentHlod& hlod, uint64_t scheduledFenceValue) {
    if (hlod.transformResource && hlod.mappedTransform) {
        hlod.transformResource->Unmap(0, nullptr);
        hlod.mappedTransform = nullptr;
    }
    if (hlod.vertexResource) pendingResources_.push_back({hlod.vertexResource, scheduledFenceValue});
    if (hlod.indexResource) pendingResources_.push_back({hlod.indexResource, scheduledFenceValue});
    if (hlod.transformResource) pendingResources_.push_back({hlod.transformResource, scheduledFenceValue});
    hlod = {};
}

bool EditorWorldPartitionPipeline::QueueHlodBuild(
    const EditorWorldPartitionCell& cell, const EditorScene& scene,
    const EditorAssetRegistry& registry, EditorProductionMeshRuntimeCache& runtimeCache,
    const std::unordered_map<std::string, Matrix4x4>& worlds,
    uint64_t scheduledFenceValue) {
    (void)scheduledFenceValue;
    if (pendingBuilds_.size() >= policy_.maximumConcurrentBuilds) return false;
    if (std::any_of(pendingBuilds_.begin(), pendingBuilds_.end(), [&](const auto& pending) {
            return pending->key == cell.key && pending->fingerprint == cell.sourceFingerprint;
        })) return true;

    HlodBuildInput input{};
    input.key = cell.key;
    input.fingerprint = cell.sourceFingerprint;
    input.maximumVertices = policy_.maximumHlodVertices;
    input.maximumTriangles = policy_.maximumHlodTriangles;
    for (const std::string& entityGuid : cell.entityGuids) {
        const EditorSceneEntity* entity = scene.FindEntity(entityGuid);
        if (entity == nullptr) continue;
        bool hierarchyVisible = entity->visible;
        std::unordered_set<std::string> visitedParents;
        for (const EditorSceneEntity* parent = entity; hierarchyVisible &&
             !parent->parentGuid.empty();) {
            if (!visitedParents.insert(parent->guid).second) {
                hierarchyVisible = false;
                break;
            }
            parent = scene.FindEntity(parent->parentGuid);
            if (parent == nullptr || !parent->visible) hierarchyVisible = false;
        }
        if (!hierarchyVisible) continue;
        const EditorSceneComponent* renderer = scene.FindComponent(*entity,
            kEditorMeshRendererComponentType);
        if (renderer == nullptr || !renderer->enabled) continue;
        const std::string assetGuid = AssetReference(*renderer);
        const EditorAssetRecord* record = registry.FindByGuid(assetGuid);
        if (record == nullptr || record->kind != EditorAssetKind::Mesh || record->missing) continue;
        const EditorProductionMeshRuntimeResource* resource = runtimeCache.Find(assetGuid);
        if (resource == nullptr || resource->sourceTimestamp != record->sourceTimestamp) {
            std::string ignored;
            if (!runtimeCache.Load(*record, &ignored)) {
                if (resource == nullptr) continue;
            } else {
                resource = runtimeCache.Find(assetGuid);
            }
        }
        if (resource == nullptr || resource->mesh.lods.empty()) continue;
        const EditorCookedMeshLod& lod = resource->mesh.lods.back();
        if (lod.vertices.empty() || lod.indices.size() < 3) continue;
        HlodBuildInput::Source source{};
        source.vertices = lod.vertices;
        source.indices = lod.indices;
        if (const auto world = worlds.find(entityGuid); world != worlds.end()) source.world = world->second;
        input.sources.push_back(std::move(source));
    }
    if (input.sources.empty()) return false;

    auto pending = std::make_unique<PendingHlodBuild>();
    pending->key = input.key;
    pending->fingerprint = input.fingerprint;
    pending->future = std::async(std::launch::async, [input = std::move(input)]() mutable {
        HlodBuildResult result{};
        result.key = input.key;
        result.fingerprint = input.fingerprint;
        result.boundsMin = {(std::numeric_limits<float>::max)(),
            (std::numeric_limits<float>::max)(), (std::numeric_limits<float>::max)()};
        result.boundsMax = {-(std::numeric_limits<float>::max)(),
            -(std::numeric_limits<float>::max)(), -(std::numeric_limits<float>::max)()};
        uint32_t triangleCount = 0;
        for (const HlodBuildInput::Source& source : input.sources) {
            if (result.vertices.size() + source.vertices.size() > input.maximumVertices) break;
            const uint32_t baseVertex = static_cast<uint32_t>(result.vertices.size());
            for (const EditorCookedMeshVertex& vertex : source.vertices) {
                const Vector3 position = TransformPoint(vertex.position, source.world);
                const Vector3 normal = NormalizeLocal(TransformDirection(vertex.normal, source.world));
                result.vertices.push_back({{position.x, position.y, position.z, 1.0f},
                    {vertex.u, vertex.v}, normal});
                result.boundsMin.x = (std::min)(result.boundsMin.x, position.x);
                result.boundsMin.y = (std::min)(result.boundsMin.y, position.y);
                result.boundsMin.z = (std::min)(result.boundsMin.z, position.z);
                result.boundsMax.x = (std::max)(result.boundsMax.x, position.x);
                result.boundsMax.y = (std::max)(result.boundsMax.y, position.y);
                result.boundsMax.z = (std::max)(result.boundsMax.z, position.z);
            }
            for (std::size_t index = 0; index + 2 < source.indices.size(); index += 3) {
                if (triangleCount >= input.maximumTriangles) break;
                const uint32_t a = source.indices[index];
                const uint32_t b = source.indices[index + 1];
                const uint32_t c = source.indices[index + 2];
                if (a >= source.vertices.size() || b >= source.vertices.size() ||
                    c >= source.vertices.size()) continue;
                result.indices.push_back(baseVertex + a);
                result.indices.push_back(baseVertex + b);
                result.indices.push_back(baseVertex + c);
                ++triangleCount;
            }
            if (triangleCount >= input.maximumTriangles) break;
        }
        result.succeeded = !result.vertices.empty() && result.indices.size() >= 3;
        if (!result.succeeded) result.diagnostic = "HLOD merge produced no valid triangles.";
        return result;
    });
    pendingBuilds_.push_back(std::move(pending));
    ++stats_.queuedHlodBuilds;
    return true;
}

bool EditorWorldPartitionPipeline::UploadHlod(
    HlodBuildResult&& result, ID3D12GraphicsCommandList* commandList,
    uint64_t scheduledFenceValue, std::string* errorMessage) {
    if (!result.succeeded || commandList == nullptr) return false;
    const uint64_t vertexBytes = result.vertices.size() * sizeof(VertexData);
    const uint64_t indexBytes = result.indices.size() * sizeof(uint32_t);
    ResidentHlod built{};
    ComPtr<ID3D12Resource> vertexUpload;
    ComPtr<ID3D12Resource> indexUpload;
    if (!CreateBuffer(device_.Get(), D3D12_HEAP_TYPE_DEFAULT, vertexBytes,
            D3D12_RESOURCE_STATE_COMMON, built.vertexResource) ||
        !CreateBuffer(device_.Get(), D3D12_HEAP_TYPE_UPLOAD, vertexBytes,
            D3D12_RESOURCE_STATE_GENERIC_READ, vertexUpload) ||
        !CreateBuffer(device_.Get(), D3D12_HEAP_TYPE_DEFAULT, indexBytes,
            D3D12_RESOURCE_STATE_COMMON, built.indexResource) ||
        !CreateBuffer(device_.Get(), D3D12_HEAP_TYPE_UPLOAD, indexBytes,
            D3D12_RESOURCE_STATE_GENERIC_READ, indexUpload) ||
        !CreateBuffer(device_.Get(), D3D12_HEAP_TYPE_UPLOAD, sizeof(TransformationMatrix),
            D3D12_RESOURCE_STATE_GENERIC_READ, built.transformResource)) {
        SetError(errorMessage, "E-12 failed to allocate HLOD GPU buffers.");
        return false;
    }
    void* mapped = nullptr;
    if (FAILED(vertexUpload->Map(0, nullptr, &mapped))) return false;
    std::memcpy(mapped, result.vertices.data(), static_cast<size_t>(vertexBytes));
    vertexUpload->Unmap(0, nullptr);
    if (FAILED(indexUpload->Map(0, nullptr, &mapped))) return false;
    std::memcpy(mapped, result.indices.data(), static_cast<size_t>(indexBytes));
    indexUpload->Unmap(0, nullptr);
    if (FAILED(built.transformResource->Map(0, nullptr, &mapped))) return false;
    built.mappedTransform = static_cast<TransformationMatrix*>(mapped);

    D3D12_RESOURCE_BARRIER barriers[2] = {
        CD3DX12_RESOURCE_BARRIER::Transition(built.vertexResource.Get(),
            D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST),
        CD3DX12_RESOURCE_BARRIER::Transition(built.indexResource.Get(),
            D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST)};
    commandList->ResourceBarrier(2, barriers);
    commandList->CopyBufferRegion(built.vertexResource.Get(), 0, vertexUpload.Get(), 0, vertexBytes);
    commandList->CopyBufferRegion(built.indexResource.Get(), 0, indexUpload.Get(), 0, indexBytes);
    barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(built.vertexResource.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(built.indexResource.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
    commandList->ResourceBarrier(2, barriers);

    built.fingerprint = result.fingerprint;
    built.vertexBuffer = {built.vertexResource->GetGPUVirtualAddress(),
        static_cast<UINT>(vertexBytes), sizeof(VertexData)};
    built.indexBuffer = {built.indexResource->GetGPUVirtualAddress(),
        static_cast<UINT>(indexBytes), DXGI_FORMAT_R32_UINT};
    built.indexCount = static_cast<uint32_t>(result.indices.size());
    built.boundsMin = result.boundsMin;
    built.boundsMax = result.boundsMax;
    built.gpuBytes = vertexBytes + indexBytes + 256;
    built.lastUsedFrame = frameIndex_;
    if (auto existing = residentHlods_.find(result.key); existing != residentHlods_.end()) {
        RetireHlod(existing->second, scheduledFenceValue);
        existing->second = std::move(built);
    } else {
        residentHlods_.emplace(result.key, std::move(built));
    }
    pendingResources_.push_back({vertexUpload, scheduledFenceValue});
    pendingResources_.push_back({indexUpload, scheduledFenceValue});
    stats_.uploadedHlodGpuBytes += vertexBytes + indexBytes + 256;
    ++stats_.completedHlodBuilds;
    return true;
}

void EditorWorldPartitionPipeline::CollectBuilds(
    ID3D12GraphicsCommandList* uploadCommandList, uint64_t scheduledFenceValue) {
    for (auto iterator = pendingBuilds_.begin(); iterator != pendingBuilds_.end();) {
        PendingHlodBuild& pending = **iterator;
        if (pending.future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            ++iterator;
            continue;
        }
        HlodBuildResult result = pending.future.get();
        if (!result.succeeded) diagnostics_.push_back(
            "Cell '" + result.key.StableName() + "' HLOD failed: " + result.diagnostic);
        else {
            std::string uploadError;
            if (!UploadHlod(std::move(result), uploadCommandList,
                    scheduledFenceValue, &uploadError) && !uploadError.empty())
                diagnostics_.push_back(uploadError);
        }
        iterator = pendingBuilds_.erase(iterator);
    }
}

bool EditorWorldPartitionPipeline::Sync(
    const EditorScene& scene, const EditorAssetRegistry& registry,
    EditorProductionMeshRuntimeCache& runtimeCache, const Vector3& cameraWorldPosition,
    const Matrix4x4& viewProjection, ID3D12GraphicsCommandList* uploadCommandList,
    uint64_t completedFenceValue, uint64_t scheduledFenceValue,
    std::string* errorMessage) {
    if (!device_ || uploadCommandList == nullptr) {
        SetError(errorMessage, "E-12 is not initialized or has no upload command list.");
        return false;
    }
    ++frameIndex_;
    CollectRetired(completedFenceValue);
    const uint64_t totalUploaded = stats_.uploadedHlodGpuBytes;
    const uint32_t completedBuilds = stats_.completedHlodBuilds;
    stats_ = {};
    stats_.uploadedHlodGpuBytes = totalUploaded;
    stats_.completedHlodBuilds = completedBuilds;
    diagnostics_.clear();
    sourceResidentEntities_.clear();
    cells_.clear();
    crossCellReferences_.clear();
    hlodPackets_.clear();

    std::unordered_map<std::string, Matrix4x4> worlds;
    std::unordered_set<std::string> visiting;
    const auto resolveWorld = [&](const auto& self, const EditorSceneEntity& entity) -> Matrix4x4 {
        if (const auto found = worlds.find(entity.guid); found != worlds.end()) return found->second;
        if (!visiting.insert(entity.guid).second) return MakeIdentity4x4();
        const EditorSceneComponent* transform = scene.FindComponent(entity, kEditorTransformComponentType);
        Matrix4x4 world = MakeAffineMatrix(
            VectorProperty(transform, "scale", {1.0f, 1.0f, 1.0f}),
            VectorProperty(transform, "rotation", {}),
            VectorProperty(transform, "translation", {}));
        if (!entity.parentGuid.empty()) {
            if (const EditorSceneEntity* parent = scene.FindEntity(entity.parentGuid))
                world = Multiply(world, self(self, *parent));
        }
        visiting.erase(entity.guid);
        worlds.insert_or_assign(entity.guid, world);
        return world;
    };

    std::unordered_map<EditorWorldPartitionCellKey, size_t,
        EditorWorldPartitionCellKeyHash> cellIndices;
    std::unordered_map<std::string, EditorWorldPartitionCellKey> entityCells;
    for (const EditorSceneEntity& entity : scene.entities) {
        const Matrix4x4 world = resolveWorld(resolveWorld, entity);
        const Vector3 position{world.m[3][0], world.m[3][1], world.m[3][2]};
        const EditorSceneComponent* overrideComponent = scene.FindComponent(
            entity, kEditorWorldPartitionComponentType);
        const std::string layer = TextProperty(overrideComponent, "dataLayer", "Default");
        EditorWorldPartitionCellKey key = CellForPosition(position, policy_.cellSize, layer);
        if (overrideComponent != nullptr) {
            key.x = IntegerProperty(overrideComponent, "cellX", key.x);
            key.z = IntegerProperty(overrideComponent, "cellZ", key.z);
        }
        auto [found, inserted] = cellIndices.emplace(key, cells_.size());
        if (inserted) {
            EditorWorldPartitionCell cell{};
            cell.key = key;
            cell.boundsMin = {key.x * policy_.cellSize, -100000.0f, key.z * policy_.cellSize};
            cell.boundsMax = {(key.x + 1) * policy_.cellSize, 100000.0f,
                (key.z + 1) * policy_.cellSize};
            cells_.push_back(std::move(cell));
        }
        EditorWorldPartitionCell& cell = cells_[found->second];
        cell.entityGuids.push_back(entity.guid);
        cell.alwaysLoaded = cell.alwaysLoaded || BooleanProperty(
            overrideComponent, "alwaysLoaded", false);
        cell.sourceFingerprint = HashAppend(cell.sourceFingerprint, HashText(entity.guid));
        for (const auto& row : world.m) for (float value : row)
            cell.sourceFingerprint = HashAppend(cell.sourceFingerprint, std::bit_cast<uint32_t>(value));
        if (const EditorSceneComponent* renderer = scene.FindComponent(
                entity, kEditorMeshRendererComponentType)) {
            const std::string assetGuid = AssetReference(*renderer);
            cell.sourceFingerprint = HashAppend(cell.sourceFingerprint, HashText(assetGuid));
            if (const EditorAssetRecord* record = registry.FindByGuid(assetGuid))
                cell.sourceFingerprint = HashAppend(
                    cell.sourceFingerprint, record->sourceTimestamp);
            if (const EditorProductionMeshRuntimeResource* resource =
                    runtimeCache.Find(assetGuid)) {
                cell.sourceFingerprint = HashAppend(
                    cell.sourceFingerprint, resource->generation);
            }
        }
        entityCells.emplace(entity.guid, key);
    }
    std::sort(cells_.begin(), cells_.end(), [](const auto& a, const auto& b) {
        return a.key < b.key;
    });
    const EditorWorldPartitionCellKey cameraCell = CellForPosition(
        cameraWorldPosition, policy_.cellSize);
    for (EditorWorldPartitionCell& cell : cells_) {
        cell.distanceCells = cell.key.dataLayer == cameraCell.dataLayer
            ? ChebyshevDistance(cell.key, cameraCell) : UINT32_MAX;
        std::sort(cell.entityGuids.begin(), cell.entityGuids.end());
    }

    for (const EditorSceneEntity& entity : scene.entities) {
        const auto sourceCell = entityCells.find(entity.guid);
        if (sourceCell == entityCells.end()) continue;
        if (!entity.parentGuid.empty()) {
            const auto parent = entityCells.find(entity.parentGuid);
            if (parent != entityCells.end() && !(sourceCell->second == parent->second)) {
                crossCellReferences_.push_back({entity.guid, entity.parentGuid,
                    sourceCell->second, parent->second, true});
            }
        }
        for (const EditorSceneComponent& component : entity.components) {
            for (const EditorSceneObjectReference& reference : component.references) {
                if (reference.entityGuid.empty()) continue;
                const auto target = entityCells.find(reference.entityGuid);
                if (target == entityCells.end()) {
                    ++stats_.missingEntityReferences;
                    diagnostics_.push_back("Entity '" + entity.guid +
                        "' has a missing cross-cell target '" + reference.entityGuid + "'.");
                    continue;
                }
                if (!(sourceCell->second == target->second)) {
                    crossCellReferences_.push_back({entity.guid, reference.entityGuid,
                        sourceCell->second, target->second, true});
                }
            }
        }
    }
    stats_.crossCellReferences = static_cast<uint32_t>(crossCellReferences_.size());

    std::vector<size_t> order(cells_.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        if (cells_[a].alwaysLoaded != cells_[b].alwaysLoaded)
            return cells_[a].alwaysLoaded > cells_[b].alwaysLoaded;
        if (cells_[a].distanceCells != cells_[b].distanceCells)
            return cells_[a].distanceCells < cells_[b].distanceCells;
        return cells_[a].key < cells_[b].key;
    });
    std::unordered_set<EditorWorldPartitionCellKey, EditorWorldPartitionCellKeyHash> sourceWanted;
    uint32_t sourceEntities = 0;
    for (size_t index : order) {
        EditorWorldPartitionCell& cell = cells_[index];
        const auto runtime = runtimeCells_.find(cell.key);
        const bool hysteresisResident = runtime != runtimeCells_.end() &&
            (runtime->second.state == EditorWorldPartitionCellState::SourceResident ||
             runtime->second.state == EditorWorldPartitionCellState::Loading) &&
            cell.distanceCells <= policy_.sourceUnloadRadiusCells;
        if (!cell.alwaysLoaded && cell.distanceCells > policy_.sourceLoadRadiusCells &&
            !hysteresisResident) continue;
        if (sourceWanted.size() >= policy_.maximumSourceCells) {
            ++stats_.rejectedByCellBudget;
            continue;
        }
        if (sourceEntities + cell.entityGuids.size() > policy_.maximumSourceEntities) {
            ++stats_.rejectedByEntityBudget;
            continue;
        }
        sourceWanted.insert(cell.key);
        sourceEntities += static_cast<uint32_t>(cell.entityGuids.size());
    }
    bool pulled = true;
    while (pulled) {
        pulled = false;
        for (const EditorWorldPartitionCrossCellReference& reference : crossCellReferences_) {
            if (!sourceWanted.contains(reference.sourceCell) ||
                sourceWanted.contains(reference.targetCell)) continue;
            const auto target = std::find_if(cells_.begin(), cells_.end(), [&](const auto& cell) {
                return cell.key == reference.targetCell;
            });
            if (target == cells_.end()) {
                diagnostics_.push_back("Hard cross-cell reference target Cell is unavailable.");
                continue;
            }
            if (sourceWanted.size() >= policy_.maximumSourceCells) {
                ++stats_.rejectedByCellBudget;
                diagnostics_.push_back("Hard cross-cell reference could not be pulled within streaming budget.");
                continue;
            }
            if (sourceEntities + target->entityGuids.size() > policy_.maximumSourceEntities) {
                ++stats_.rejectedByEntityBudget;
                diagnostics_.push_back("Hard cross-cell reference could not be pulled within streaming budget.");
                continue;
            }
            sourceWanted.insert(target->key);
            sourceEntities += static_cast<uint32_t>(target->entityGuids.size());
            ++stats_.hardReferencePulls;
            target->hardReferencePulled = true;
            pulled = true;
        }
    }

    CollectBuilds(uploadCommandList, scheduledFenceValue);
    std::unordered_set<EditorWorldPartitionCellKey, EditorWorldPartitionCellKeyHash> hlodWanted;
    for (size_t index : order) {
        EditorWorldPartitionCell& cell = cells_[index];
        if (sourceWanted.contains(cell.key) || cell.distanceCells > policy_.hlodRadiusCells) continue;
        if (hlodWanted.size() >= policy_.maximumHlodProxies) {
            ++stats_.rejectedByHlodBudget;
            continue;
        }
        hlodWanted.insert(cell.key);
        const auto resident = residentHlods_.find(cell.key);
        cell.hlodReady = resident != residentHlods_.end() &&
            resident->second.fingerprint == cell.sourceFingerprint;
        if (!cell.hlodReady) QueueHlodBuild(cell, scene, registry, runtimeCache,
            worlds, scheduledFenceValue);
    }

    for (EditorWorldPartitionCell& cell : cells_) {
        CellRuntime& runtime = runtimeCells_[cell.key];
        runtime.fingerprint = cell.sourceFingerprint;
        const bool wantsSource = sourceWanted.contains(cell.key);
        const bool wantsHlod = hlodWanted.contains(cell.key);
        const auto hlod = residentHlods_.find(cell.key);
        const bool hlodReady = hlod != residentHlods_.end() &&
            hlod->second.fingerprint == cell.sourceFingerprint;
        if (wantsSource) {
            if (runtime.state == EditorWorldPartitionCellState::Unloaded ||
                runtime.state == EditorWorldPartitionCellState::HlodResident) {
                runtime.state = EditorWorldPartitionCellState::Loading;
                runtime.stateFrame = frameIndex_;
                runtime.manifestPending = true;
                const uint64_t fingerprint = cell.sourceFingerprint;
                runtime.manifestFuture = std::async(std::launch::async,
                    [fingerprint]() { return fingerprint != 0; });
            }
            if (runtime.manifestPending && runtime.manifestFuture.valid() &&
                runtime.manifestFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                runtime.manifestFuture.get();
                runtime.manifestPending = false;
                runtime.state = EditorWorldPartitionCellState::SourceResident;
            }
            for (const std::string& guid : cell.entityGuids) sourceResidentEntities_.insert(guid);
        } else if (wantsHlod) {
            if (hlodReady) runtime.state = EditorWorldPartitionCellState::HlodResident;
            else {
                runtime.state = EditorWorldPartitionCellState::Loading;
                for (const std::string& guid : cell.entityGuids) sourceResidentEntities_.insert(guid);
            }
        } else if (runtime.state != EditorWorldPartitionCellState::Unloaded) {
            if (runtime.state != EditorWorldPartitionCellState::Unloading) {
                runtime.state = EditorWorldPartitionCellState::Unloading;
                runtime.stateFrame = frameIndex_;
            } else if (frameIndex_ > runtime.stateFrame) runtime.state = EditorWorldPartitionCellState::Unloaded;
        }
        cell.state = runtime.state;
        if (cell.state == EditorWorldPartitionCellState::Loading) ++stats_.loadingCells;
        else if (cell.state == EditorWorldPartitionCellState::SourceResident) ++stats_.sourceResidentCells;
        else if (cell.state == EditorWorldPartitionCellState::HlodResident) ++stats_.hlodResidentCells;
        else if (cell.state == EditorWorldPartitionCellState::Unloaded) ++stats_.unloadedCells;
    }

    for (auto& [key, hlod] : residentHlods_) {
        if (hlodWanted.contains(key)) {
            hlod.lastUsedFrame = frameIndex_;
            hlod.mappedTransform->World = MakeIdentity4x4();
            hlod.mappedTransform->WVP = viewProjection;
            hlod.mappedTransform->WorldInverseTranspose = MakeIdentity4x4();
            const Vector3 center{(hlod.boundsMin.x + hlod.boundsMax.x) * 0.5f,
                (hlod.boundsMin.y + hlod.boundsMax.y) * 0.5f,
                (hlod.boundsMin.z + hlod.boundsMax.z) * 0.5f};
            const Vector3 extent{(hlod.boundsMax.x - hlod.boundsMin.x) * 0.5f,
                (hlod.boundsMax.y - hlod.boundsMin.y) * 0.5f,
                (hlod.boundsMax.z - hlod.boundsMin.z) * 0.5f};
            hlodPackets_.push_back({"hlod:" + key.StableName(), "hlod:" + key.StableName(),
                0, 0, hlod.indexCount, hlod.vertexBuffer, hlod.indexBuffer,
                hlod.transformResource->GetGPUVirtualAddress(), center, Length(extent),
                SphereVisible(center, Length(extent), viewProjection)});
        }
        stats_.residentHlodGpuBytes += hlod.gpuBytes;
    }
    for (auto iterator = residentHlods_.begin(); iterator != residentHlods_.end();) {
        if (!hlodWanted.contains(iterator->first) &&
            frameIndex_ > iterator->second.lastUsedFrame + policy_.inactiveHlodRetentionFrames) {
            RetireHlod(iterator->second, scheduledFenceValue);
            iterator = residentHlods_.erase(iterator);
        } else ++iterator;
    }
    stats_.cells = static_cast<uint32_t>(cells_.size());
    stats_.sourceResidentEntities = static_cast<uint32_t>(sourceResidentEntities_.size());
    stats_.queuedHlodBuilds = static_cast<uint32_t>(pendingBuilds_.size());
    stats_.pendingGpuRetirements = static_cast<uint32_t>(pendingResources_.size());
    if (!diagnostics_.empty() && errorMessage != nullptr) *errorMessage = diagnostics_.front();
    return true;
}

const char* ToString(EditorWorldPartitionCellState state) noexcept {
    switch (state) {
    case EditorWorldPartitionCellState::Unloaded: return "Unloaded";
    case EditorWorldPartitionCellState::Loading: return "Loading";
    case EditorWorldPartitionCellState::SourceResident: return "SourceResident";
    case EditorWorldPartitionCellState::HlodResident: return "HLODResident";
    case EditorWorldPartitionCellState::Unloading: return "Unloading";
    }
    return "Unloaded";
}

} // namespace editor
