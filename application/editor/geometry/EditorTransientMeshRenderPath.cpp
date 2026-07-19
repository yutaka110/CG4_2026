#include "EditorTransientMeshRenderPath.h"

#include "EditorGeometryMesh.h"
#include "../mesh/EditorProductionMeshAsset.h"

#include "../../../externals/DirectXTex/d3dx12.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <sstream>
#include <utility>

namespace editor {
namespace {

void SetError(std::string* errorMessage, std::string message) {
    if (errorMessage != nullptr) *errorMessage = std::move(message);
}

const EditorSceneProperty* FindProperty(
    const EditorSceneComponent& component,
    std::string_view name) {
    const auto found = std::find_if(
        component.properties.begin(), component.properties.end(),
        [&](const EditorSceneProperty& property) { return property.name == name; });
    return found == component.properties.end() ? nullptr : &*found;
}

Vector3 ParseVector(
    const EditorSceneComponent* component,
    std::string_view name,
    Vector3 fallback) {
    if (component == nullptr) return fallback;
    const EditorSceneProperty* property = FindProperty(*component, name);
    if (property == nullptr) return fallback;
    std::istringstream stream(property->value);
    Vector3 value{};
    if (!(stream >> value.x >> value.y >> value.z) ||
        !std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z)) {
        return fallback;
    }
    return value;
}

bool ParseHash(const EditorSceneComponent& component, std::string_view name, uint64_t& value) {
    const EditorSceneProperty* property = FindProperty(component, name);
    if (property == nullptr) return false;
    try {
        std::size_t consumed = 0;
        value = std::stoull(property->value, &consumed);
        return consumed == property->value.size();
    } catch (...) {
        return false;
    }
}

bool HasAssetReference(const EditorSceneComponent& component) {
    return std::any_of(
        component.references.begin(), component.references.end(),
        [](const EditorSceneObjectReference& reference) {
            return reference.property == "asset" && !reference.assetGuid.empty();
        });
}

D3D12_RESOURCE_DESC BufferDescription(uint64_t bytes) {
    D3D12_RESOURCE_DESC description{};
    description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    description.Width = (std::max)(uint64_t{256}, (bytes + 255ull) & ~255ull);
    description.Height = 1;
    description.DepthOrArraySize = 1;
    description.MipLevels = 1;
    description.Format = DXGI_FORMAT_UNKNOWN;
    description.SampleDesc.Count = 1;
    description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return description;
}

bool CreateBuffer(
    ID3D12Device* device,
    D3D12_HEAP_TYPE heapType,
    uint64_t bytes,
    D3D12_RESOURCE_STATES initialState,
    Microsoft::WRL::ComPtr<ID3D12Resource>& output,
    std::string* errorMessage) {
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = heapType;
    const D3D12_RESOURCE_DESC description = BufferDescription(bytes);
    if (FAILED(device->CreateCommittedResource(
            &heap, D3D12_HEAP_FLAG_NONE, &description, initialState,
            nullptr, IID_PPV_ARGS(output.ReleaseAndGetAddressOf())))) {
        SetError(errorMessage, "Editor Transient Mesh failed to allocate a D3D12 buffer.");
        return false;
    }
    return true;
}

Matrix4x4 ResolveWorld(
    const EditorScene& scene,
    const EditorSceneEntity& entity,
    std::unordered_map<std::string, Matrix4x4>& cache,
    std::unordered_set<std::string>& visiting) {
    if (const auto found = cache.find(entity.guid); found != cache.end()) return found->second;
    if (!visiting.insert(entity.guid).second) return MakeIdentity4x4();
    const EditorSceneComponent* transform = scene.FindComponent(entity, kEditorTransformComponentType);
    Matrix4x4 world = MakeAffineMatrix(
        ParseVector(transform, "scale", {1.0f, 1.0f, 1.0f}),
        ParseVector(transform, "rotation", {}),
        ParseVector(transform, "translation", {}));
    if (!entity.parentGuid.empty()) {
        if (const EditorSceneEntity* parent = scene.FindEntity(entity.parentGuid)) {
            world = Multiply(world, ResolveWorld(scene, *parent, cache, visiting));
        }
    }
    visiting.erase(entity.guid);
    cache.insert_or_assign(entity.guid, world);
    return world;
}

bool ResolveVisibility(
    const EditorScene& scene,
    const EditorSceneEntity& entity,
    std::unordered_map<std::string, bool>& cache,
    std::unordered_set<std::string>& visiting) {
    if (const auto found = cache.find(entity.guid); found != cache.end()) return found->second;
    if (!visiting.insert(entity.guid).second) return false;
    bool visible = entity.visible;
    if (visible && !entity.parentGuid.empty()) {
        if (const EditorSceneEntity* parent = scene.FindEntity(entity.parentGuid)) {
            visible = ResolveVisibility(scene, *parent, cache, visiting);
        }
    }
    visiting.erase(entity.guid);
    cache.insert_or_assign(entity.guid, visible);
    return visible;
}

void GeometryBounds(const EditorGeometryMesh& geometry, Vector3& minimum, Vector3& maximum) {
    minimum = geometry.vertices.front().position;
    maximum = minimum;
    for (const EditorGeometryVertex& vertex : geometry.vertices) {
        minimum.x = (std::min)(minimum.x, vertex.position.x);
        minimum.y = (std::min)(minimum.y, vertex.position.y);
        minimum.z = (std::min)(minimum.z, vertex.position.z);
        maximum.x = (std::max)(maximum.x, vertex.position.x);
        maximum.y = (std::max)(maximum.y, vertex.position.y);
        maximum.z = (std::max)(maximum.z, vertex.position.z);
    }
}

} // namespace

bool EditorTransientMeshRenderPath::Sync(
    const EditorScene& scene,
    const EditorGeometryWorkspace* activeWorkspace,
    const Matrix4x4& viewProjection,
    ID3D12GraphicsCommandList* uploadCommandList,
    uint64_t completedFenceValue,
    uint64_t scheduledFenceValue,
    std::string* errorMessage,
    const std::unordered_set<std::string>* sourceResidentEntities) {
    CollectRetired(completedFenceValue);
    sources_.clear();
    renderPackets_.clear();
    overriddenEntities_.clear();
    diagnostics_.clear();
    const uint64_t uploadedBytes = stats_.uploadedGpuBytes;
    stats_ = {};
    stats_.uploadedGpuBytes = uploadedBytes;

    if (uploadCommandList != nullptr &&
        !EnsureDeviceAndMaterials(uploadCommandList, errorMessage)) return false;

    std::unordered_map<std::string, Matrix4x4> worldCache;
    std::unordered_map<std::string, bool> visibilityCache;
    std::unordered_set<std::string> worldVisiting;
    std::unordered_set<std::string> visibilityVisiting;
    std::unordered_set<std::string> activeEntities;

    for (const EditorSceneEntity& entity : scene.entities) {
        if (sourceResidentEntities != nullptr &&
            !sourceResidentEntities->contains(entity.guid)) continue;
        const EditorSceneComponent* component =
            scene.FindComponent(entity, kEditorMeshRendererComponentType);
        if (component == nullptr || !component->enabled) continue;

        EditorGeometryMesh decoded;
        const EditorGeometryMesh* geometry = nullptr;
        const bool workspaceTarget = activeWorkspace != nullptr &&
            activeWorkspace->EntityGuid() == entity.guid;
        const bool preview = workspaceTarget && activeWorkspace->HasPreview() &&
            activeWorkspace->DisplayMesh() != nullptr;
        if (preview) {
            geometry = activeWorkspace->DisplayMesh();
        } else if (workspaceTarget && activeWorkspace->AuthoredMesh() != nullptr) {
            geometry = activeWorkspace->AuthoredMesh();
        } else if (const EditorSceneProperty* property =
                FindProperty(*component, kEditorEditableGeometryProperty)) {
            std::string decodeError;
            if (!EditorGeometryMesh::Deserialize(property->value, decoded, &decodeError)) {
                diagnostics_.push_back(
                    "Editable Geometry on '" + entity.name + "' was rejected: " + decodeError);
                continue;
            }
            geometry = &decoded;
        }
        if (geometry == nullptr || geometry->vertices.empty() || geometry->triangles.empty()) continue;

        const uint64_t geometryHash = geometry->ContentHash();
        uint64_t bakedSourceHash = 0;
        const bool currentDurableAsset = HasAssetReference(*component) &&
            ParseHash(*component, kEditorBakedMeshSourceHashProperty, bakedSourceHash) &&
            bakedSourceHash == geometryHash;
        if (!preview && currentDurableAsset) continue;

        overriddenEntities_.insert(entity.guid);
        activeEntities.insert(entity.guid);
        const Matrix4x4 world = ResolveWorld(
            scene, entity, worldCache, worldVisiting);
        const bool visible = ResolveVisibility(
            scene, entity, visibilityCache, visibilityVisiting);
        sources_.push_back({entity.guid, geometryHash, world,
            static_cast<uint32_t>(geometry->vertices.size()),
            static_cast<uint32_t>(geometry->triangles.size()), preview, visible});
        ++stats_.sourceCount;
        if (preview) ++stats_.previewCount;

        if (!visible || device_ == nullptr || uploadCommandList == nullptr) continue;
        std::string gpuError;
        if (!EnsureMesh(entity.guid, *geometry, uploadCommandList,
                scheduledFenceValue, &gpuError) ||
            !EnsureTransform(entity.guid, world, viewProjection, &gpuError)) {
            diagnostics_.push_back(
                "Editable Geometry on '" + entity.name + "' GPU sync failed: " + gpuError);
            continue;
        }
        const auto mesh = gpuMeshes_.find(entity.guid);
        const auto transform = gpuTransforms_.find(entity.guid);
        if (mesh == gpuMeshes_.end() || transform == gpuTransforms_.end()) continue;
        Vector3 boundsMin{};
        Vector3 boundsMax{};
        GeometryBounds(*geometry, boundsMin, boundsMax);
        const Vector3 center{
            (boundsMin.x + boundsMax.x) * 0.5f,
            (boundsMin.y + boundsMax.y) * 0.5f,
            (boundsMin.z + boundsMax.z) * 0.5f};
        const Vector3 extent{
            (boundsMax.x - boundsMin.x) * 0.5f,
            (boundsMax.y - boundsMin.y) * 0.5f,
            (boundsMax.z - boundsMin.z) * 0.5f};
        const float radius = std::sqrt(
            extent.x * extent.x + extent.y * extent.y + extent.z * extent.z);
        for (const GpuSubmesh& submesh : mesh->second.submeshes) {
            EditorProductionSceneRenderPacket packet{};
            packet.entityGuid = entity.guid;
            packet.assetGuid = "editor-transient:" + entity.guid;
            packet.materialSlot = submesh.materialSlot;
            packet.indexCount = submesh.indexCount;
            packet.vertexBuffer = mesh->second.vertexBuffer;
            packet.indexBuffer = submesh.indexBuffer;
            packet.transformAddress = transform->second.resource->GetGPUVirtualAddress();
            packet.boundsCenter = center;
            packet.boundsRadius = radius;
            packet.cpuVisible = true;
            packet.materialAddressOverride = preview
                ? previewMaterial_->GetGPUVirtualAddress()
                : authoredMaterial_->GetGPUVirtualAddress();
            packet.editorTransient = true;
            renderPackets_.push_back(std::move(packet));
        }
    }

    for (auto iterator = gpuMeshes_.begin(); iterator != gpuMeshes_.end();) {
        if (activeEntities.contains(iterator->first)) {
            ++iterator;
        } else {
            RetireMesh(iterator->second, scheduledFenceValue);
            iterator = gpuMeshes_.erase(iterator);
        }
    }
    for (auto iterator = gpuTransforms_.begin(); iterator != gpuTransforms_.end();) {
        if (activeEntities.contains(iterator->first)) {
            ++iterator;
        } else {
            if (iterator->second.resource && iterator->second.mapped != nullptr) {
                iterator->second.resource->Unmap(0, nullptr);
                iterator->second.mapped = nullptr;
            }
            if (iterator->second.resource) {
                pendingResources_.push_back({iterator->second.resource, scheduledFenceValue});
            }
            iterator = gpuTransforms_.erase(iterator);
        }
    }
    stats_.renderPacketCount = static_cast<uint32_t>(renderPackets_.size());
    stats_.residentGpuMeshes = static_cast<uint32_t>(gpuMeshes_.size());
    stats_.pendingGpuRetirements = static_cast<uint32_t>(pendingResources_.size());
    for (const auto& [guid, mesh] : gpuMeshes_) {
        (void)guid;
        stats_.residentGpuBytes += mesh.bytes;
    }
    return true;
}

bool EditorTransientMeshRenderPath::EnsureDeviceAndMaterials(
    ID3D12GraphicsCommandList* commandList,
    std::string* errorMessage) {
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    if (FAILED(commandList->GetDevice(IID_PPV_ARGS(&device))) || device == nullptr) {
        SetError(errorMessage, "Editor Transient Mesh could not resolve the D3D12 device.");
        return false;
    }
    if (device_.Get() != device.Get()) {
        Shutdown();
        device_ = device;
    }
    if (authoredMaterial_ != nullptr && previewMaterial_ != nullptr) return true;
    if (!CreateBuffer(device_.Get(), D3D12_HEAP_TYPE_UPLOAD, sizeof(Material),
            D3D12_RESOURCE_STATE_GENERIC_READ, authoredMaterial_, errorMessage) ||
        !CreateBuffer(device_.Get(), D3D12_HEAP_TYPE_UPLOAD, sizeof(Material),
            D3D12_RESOURCE_STATE_GENERIC_READ, previewMaterial_, errorMessage)) return false;
    const auto initialize = [&](ID3D12Resource* resource, Vector4 color) {
        Material* material = nullptr;
        if (FAILED(resource->Map(0, nullptr, reinterpret_cast<void**>(&material))) ||
            material == nullptr) return false;
        *material = {};
        material->color = color;
        material->enableLighting = true;
        material->uvTransform = MakeIdentity4x4();
        material->shininess = 12.0f;
        material->environmentCoefficient = 0.15f;
        material->specularMode = 1;
        resource->Unmap(0, nullptr);
        return true;
    };
    if (!initialize(authoredMaterial_.Get(), {0.58f, 0.66f, 0.78f, 1.0f}) ||
        !initialize(previewMaterial_.Get(), {0.36f, 0.86f, 0.55f, 1.0f})) {
        SetError(errorMessage, "Editor Transient Mesh failed to initialize editor materials.");
        return false;
    }
    return true;
}

bool EditorTransientMeshRenderPath::EnsureMesh(
    std::string_view entityGuid,
    const EditorGeometryMesh& geometry,
    ID3D12GraphicsCommandList* commandList,
    uint64_t scheduledFenceValue,
    std::string* errorMessage) {
    const std::string key(entityGuid);
    const uint64_t geometryHash = geometry.ContentHash();
    if (const auto found = gpuMeshes_.find(key);
        found != gpuMeshes_.end() && found->second.geometryHash == geometryHash) return true;

    std::vector<VertexData> vertices;
    vertices.reserve(geometry.vertices.size());
    for (const EditorGeometryVertex& source : geometry.vertices) {
        vertices.push_back({{source.position.x, source.position.y, source.position.z, 1.0f},
            {source.u, source.v}, source.normal});
    }
    std::map<uint32_t, std::vector<uint32_t>> indicesByMaterial;
    for (const EditorGeometryTriangle& triangle : geometry.triangles) {
        auto& indices = indicesByMaterial[triangle.materialSlot];
        indices.push_back(triangle.vertices[0]);
        indices.push_back(triangle.vertices[1]);
        indices.push_back(triangle.vertices[2]);
    }
    if (vertices.empty() || indicesByMaterial.empty()) {
        SetError(errorMessage, "Editor Transient Mesh cannot upload empty Geometry.");
        return false;
    }

    GpuMesh built{};
    built.geometryHash = geometryHash;
    const uint64_t vertexBytes = vertices.size() * sizeof(VertexData);
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexUpload;
    if (!CreateBuffer(device_.Get(), D3D12_HEAP_TYPE_DEFAULT, vertexBytes,
            D3D12_RESOURCE_STATE_COMMON, built.vertexResource, errorMessage) ||
        !CreateBuffer(device_.Get(), D3D12_HEAP_TYPE_UPLOAD, vertexBytes,
            D3D12_RESOURCE_STATE_GENERIC_READ, vertexUpload, errorMessage)) return false;
    void* mapped = nullptr;
    if (FAILED(vertexUpload->Map(0, nullptr, &mapped))) {
        SetError(errorMessage, "Editor Transient Mesh failed to map vertex staging memory.");
        return false;
    }
    std::memcpy(mapped, vertices.data(), static_cast<std::size_t>(vertexBytes));
    vertexUpload->Unmap(0, nullptr);
    D3D12_RESOURCE_BARRIER vertexToCopy = CD3DX12_RESOURCE_BARRIER::Transition(
        built.vertexResource.Get(), D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COPY_DEST);
    commandList->ResourceBarrier(1, &vertexToCopy);
    commandList->CopyBufferRegion(
        built.vertexResource.Get(), 0, vertexUpload.Get(), 0, vertexBytes);
    D3D12_RESOURCE_BARRIER vertexToRead = CD3DX12_RESOURCE_BARRIER::Transition(
        built.vertexResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    commandList->ResourceBarrier(1, &vertexToRead);
    built.vertexBuffer = {built.vertexResource->GetGPUVirtualAddress(),
        static_cast<UINT>(vertexBytes), sizeof(VertexData)};
    built.bytes = vertexBytes;
    pendingResources_.push_back({vertexUpload, scheduledFenceValue});

    for (const auto& [materialSlot, indices] : indicesByMaterial) {
        const uint64_t indexBytes = indices.size() * sizeof(uint32_t);
        GpuSubmesh submesh{};
        Microsoft::WRL::ComPtr<ID3D12Resource> indexUpload;
        if (!CreateBuffer(device_.Get(), D3D12_HEAP_TYPE_DEFAULT, indexBytes,
                D3D12_RESOURCE_STATE_COMMON, submesh.indexResource, errorMessage) ||
            !CreateBuffer(device_.Get(), D3D12_HEAP_TYPE_UPLOAD, indexBytes,
                D3D12_RESOURCE_STATE_GENERIC_READ, indexUpload, errorMessage)) return false;
        if (FAILED(indexUpload->Map(0, nullptr, &mapped))) {
            SetError(errorMessage, "Editor Transient Mesh failed to map index staging memory.");
            return false;
        }
        std::memcpy(mapped, indices.data(), static_cast<std::size_t>(indexBytes));
        indexUpload->Unmap(0, nullptr);
        D3D12_RESOURCE_BARRIER indexToCopy = CD3DX12_RESOURCE_BARRIER::Transition(
            submesh.indexResource.Get(), D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_COPY_DEST);
        commandList->ResourceBarrier(1, &indexToCopy);
        commandList->CopyBufferRegion(
            submesh.indexResource.Get(), 0, indexUpload.Get(), 0, indexBytes);
        D3D12_RESOURCE_BARRIER indexToRead = CD3DX12_RESOURCE_BARRIER::Transition(
            submesh.indexResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_INDEX_BUFFER);
        commandList->ResourceBarrier(1, &indexToRead);
        submesh.indexBuffer = {submesh.indexResource->GetGPUVirtualAddress(),
            static_cast<UINT>(indexBytes), DXGI_FORMAT_R32_UINT};
        submesh.materialSlot = materialSlot;
        submesh.indexCount = static_cast<uint32_t>(indices.size());
        submesh.bytes = indexBytes;
        built.bytes += indexBytes;
        pendingResources_.push_back({indexUpload, scheduledFenceValue});
        built.submeshes.push_back(std::move(submesh));
    }
    stats_.uploadedGpuBytes += built.bytes;
    if (auto found = gpuMeshes_.find(key); found != gpuMeshes_.end()) {
        RetireMesh(found->second, scheduledFenceValue);
        found->second = std::move(built);
    } else {
        gpuMeshes_.emplace(key, std::move(built));
    }
    return true;
}

bool EditorTransientMeshRenderPath::EnsureTransform(
    std::string_view entityGuid,
    const Matrix4x4& world,
    const Matrix4x4& viewProjection,
    std::string* errorMessage) {
    const std::string key(entityGuid);
    auto found = gpuTransforms_.find(key);
    if (found == gpuTransforms_.end()) {
        GpuTransform transform{};
        if (!CreateBuffer(device_.Get(), D3D12_HEAP_TYPE_UPLOAD, sizeof(TransformationMatrix),
                D3D12_RESOURCE_STATE_GENERIC_READ, transform.resource, errorMessage)) return false;
        void* mapped = nullptr;
        if (FAILED(transform.resource->Map(0, nullptr, &mapped))) {
            SetError(errorMessage, "Editor Transient Mesh failed to map a transform buffer.");
            return false;
        }
        transform.mapped = static_cast<TransformationMatrix*>(mapped);
        found = gpuTransforms_.emplace(key, std::move(transform)).first;
    }
    found->second.mapped->World = world;
    found->second.mapped->WVP = Multiply(world, viewProjection);
    Matrix4x4 worldCopy = world;
    found->second.mapped->WorldInverseTranspose = Transpose(Inverse(worldCopy));
    return true;
}

void EditorTransientMeshRenderPath::RetireMesh(GpuMesh& mesh, uint64_t fenceValue) {
    if (mesh.vertexResource) pendingResources_.push_back({mesh.vertexResource, fenceValue});
    for (GpuSubmesh& submesh : mesh.submeshes) {
        if (submesh.indexResource) pendingResources_.push_back({submesh.indexResource, fenceValue});
    }
    mesh.submeshes.clear();
    mesh.vertexResource.Reset();
}

void EditorTransientMeshRenderPath::CollectRetired(uint64_t completedFenceValue) {
    std::erase_if(pendingResources_, [&](const PendingResource& pending) {
        return pending.retireFenceValue <= completedFenceValue;
    });
}

void EditorTransientMeshRenderPath::Shutdown() {
    for (auto& [guid, transform] : gpuTransforms_) {
        (void)guid;
        if (transform.resource && transform.mapped != nullptr) transform.resource->Unmap(0, nullptr);
    }
    gpuTransforms_.clear();
    gpuMeshes_.clear();
    pendingResources_.clear();
    sources_.clear();
    renderPackets_.clear();
    overriddenEntities_.clear();
    diagnostics_.clear();
    stats_ = {};
    authoredMaterial_.Reset();
    previewMaterial_.Reset();
    device_.Reset();
}

} // namespace editor
