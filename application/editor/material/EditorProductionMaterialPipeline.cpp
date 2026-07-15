#include "EditorProductionMaterialPipeline.h"

#include "../documents/EditorMaterialGraphDocumentProvider.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <set>
#include <sstream>
#include <unordered_set>

namespace editor {
namespace {

void SetError(std::string* errorMessage, std::string message) {
    if (errorMessage != nullptr) *errorMessage = std::move(message);
}

uint64_t Mix(uint64_t hash, uint64_t value) {
    hash ^= value + 0x9e3779b97f4a7c15ull + (hash << 6u) + (hash >> 2u);
    return hash;
}

uint64_t HashText(std::string_view text) {
    uint64_t hash = 1469598103934665603ull;
    for (const unsigned char value : text) {
        hash ^= value;
        hash *= 1099511628211ull;
    }
    return hash;
}

uint64_t FileTimestamp(const std::filesystem::path& path) {
    std::error_code error;
    const auto value = std::filesystem::last_write_time(path, error);
    return error ? 0ull : static_cast<uint64_t>(value.time_since_epoch().count());
}

bool Finite(float value) { return std::isfinite(value); }

float Clamp(float value, float minimum, float maximum) {
    return (std::max)(minimum, (std::min)(maximum, value));
}

const EditorSceneProperty* Property(
    const EditorSceneComponent& component,
    std::string_view name) {
    const auto found = std::find_if(component.properties.begin(), component.properties.end(),
        [&](const EditorSceneProperty& value) { return value.name == name; });
    return found == component.properties.end() ? nullptr : &*found;
}

float FloatProperty(const EditorSceneComponent& component, std::string_view name, float fallback) {
    const EditorSceneProperty* property = Property(component, name);
    if (property == nullptr) return fallback;
    std::istringstream stream(property->value);
    float result = fallback;
    return (stream >> result) && Finite(result) ? result : fallback;
}

Vector3 VectorProperty(
    const EditorSceneComponent& component,
    std::string_view name,
    Vector3 fallback) {
    const EditorSceneProperty* property = Property(component, name);
    if (property == nullptr) return fallback;
    std::istringstream stream(property->value);
    Vector3 result{};
    if (!(stream >> result.x >> result.y >> result.z) ||
        !Finite(result.x) || !Finite(result.y) || !Finite(result.z)) return fallback;
    return result;
}

Vector4 ColorProperty(
    const EditorSceneComponent& component,
    std::string_view name,
    Vector4 fallback) {
    const EditorSceneProperty* property = Property(component, name);
    if (property == nullptr) return fallback;
    std::istringstream stream(property->value);
    Vector4 result{};
    if (!(stream >> result.x >> result.y >> result.z)) return fallback;
    if (!(stream >> result.w)) result.w = 1.0f;
    if (!Finite(result.x) || !Finite(result.y) || !Finite(result.z) || !Finite(result.w)) return fallback;
    result.x = Clamp(result.x, 0.0f, 100.0f);
    result.y = Clamp(result.y, 0.0f, 100.0f);
    result.z = Clamp(result.z, 0.0f, 100.0f);
    result.w = Clamp(result.w, 0.0f, 1.0f);
    return result;
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

bool ParseSlot(std::string_view property, uint32_t& slot) {
    if (property == "material") {
        slot = 0;
        return true;
    }
    constexpr std::string_view prefix = "material:";
    if (!property.starts_with(prefix)) return false;
    try {
        std::size_t consumed = 0;
        const unsigned long value = std::stoul(std::string(property.substr(prefix.size())), &consumed);
        if (consumed != property.size() - prefix.size() || value > 255) return false;
        slot = static_cast<uint32_t>(value);
        return true;
    } catch (...) {
        return false;
    }
}

bool CreateUploadBuffer(
    ID3D12Device* device,
    uint64_t byteSize,
    Microsoft::WRL::ComPtr<ID3D12Resource>& resource,
    std::string* errorMessage) {
    if (device == nullptr) {
        SetError(errorMessage, "E-7 has no D3D12 device.");
        return false;
    }
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC description{};
    description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    description.Width = (byteSize + 255ull) & ~255ull;
    description.Height = 1;
    description.DepthOrArraySize = 1;
    description.MipLevels = 1;
    description.SampleDesc.Count = 1;
    description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if (FAILED(device->CreateCommittedResource(
            &heap, D3D12_HEAP_FLAG_NONE, &description,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(resource.ReleaseAndGetAddressOf())))) {
        SetError(errorMessage, "E-7 failed to create a mapped constant buffer.");
        return false;
    }
    return true;
}

template <class T>
bool MapResource(ID3D12Resource* resource, T*& mapped, std::string* errorMessage) {
    void* value = nullptr;
    if (resource == nullptr || FAILED(resource->Map(0, nullptr, &value))) {
        SetError(errorMessage, "E-7 failed to map a constant buffer.");
        return false;
    }
    mapped = static_cast<T*>(value);
    return true;
}

struct LightCandidate {
    const EditorSceneEntity* entity = nullptr;
    const EditorSceneComponent* component = nullptr;
    Matrix4x4 world = MakeIdentity4x4();
    float priority = 0.0f;
};

} // namespace

bool EncodeEditorMaterialInstance(
    const EditorMaterialInstanceAsset& asset,
    std::string& output,
    std::string* errorMessage) {
    if (asset.schemaVersion != kEditorMaterialInstanceSchemaVersion ||
        !IsDurableEditorAssetGuid(asset.assetGuid) ||
        !IsDurableEditorAssetGuid(asset.parentMaterialGuid) ||
        !Finite(asset.baseColor.x) || !Finite(asset.baseColor.y) ||
        !Finite(asset.baseColor.z) || !Finite(asset.baseColor.w) ||
        !Finite(asset.roughness) || !Finite(asset.metallic) ||
        !Finite(asset.environmentCoefficient) ||
        (!asset.albedoTextureGuid.empty() && !IsDurableEditorAssetGuid(asset.albedoTextureGuid)) ||
        (!asset.normalTextureGuid.empty() && !IsDurableEditorAssetGuid(asset.normalTextureGuid))) {
        SetError(errorMessage, "Material Instance identity or numeric overrides are invalid.");
        return false;
    }
    std::ostringstream stream;
    stream << "MATERIAL_INSTANCE " << kEditorMaterialInstanceSchemaVersion << '\n';
    stream << "ASSET " << asset.assetGuid << ' ' << std::quoted(asset.name) << '\n';
    stream << "PARENT " << asset.parentMaterialGuid << '\n';
    stream << std::setprecision(9);
    stream << "BASE_COLOR " << asset.baseColor.x << ' ' << asset.baseColor.y << ' '
           << asset.baseColor.z << ' ' << asset.baseColor.w << '\n';
    stream << "SURFACE " << Clamp(asset.roughness, 0.0f, 1.0f) << ' '
           << Clamp(asset.metallic, 0.0f, 1.0f) << ' '
           << Clamp(asset.environmentCoefficient, 0.0f, 1.0f) << '\n';
    stream << "TEXTURES "
           << (asset.albedoTextureGuid.empty() ? "-" : asset.albedoTextureGuid) << ' '
           << (asset.normalTextureGuid.empty() ? "-" : asset.normalTextureGuid) << '\n';
    stream << "REVISION " << asset.revision << "\nEND\n";
    output = stream.str();
    return true;
}

bool DecodeEditorMaterialInstance(
    std::string_view content,
    EditorMaterialInstanceAsset& output,
    std::string* errorMessage) {
    std::istringstream stream{std::string(content)};
    std::string kind;
    EditorMaterialInstanceAsset decoded{};
    uint32_t schema = 0;
    if (!(stream >> kind >> schema) || kind != "MATERIAL_INSTANCE" ||
        schema != kEditorMaterialInstanceSchemaVersion) {
        SetError(errorMessage, "Material Instance header is unsupported.");
        return false;
    }
    decoded.schemaVersion = schema;
    bool hasAsset = false;
    bool hasParent = false;
    bool hasEnd = false;
    while (stream >> kind) {
        if (kind == "ASSET") {
            if (!(stream >> decoded.assetGuid >> std::quoted(decoded.name))) break;
            hasAsset = true;
        } else if (kind == "PARENT") {
            if (!(stream >> decoded.parentMaterialGuid)) break;
            hasParent = true;
        } else if (kind == "BASE_COLOR") {
            if (!(stream >> decoded.baseColor.x >> decoded.baseColor.y >>
                    decoded.baseColor.z >> decoded.baseColor.w)) break;
        } else if (kind == "SURFACE") {
            if (!(stream >> decoded.roughness >> decoded.metallic >>
                    decoded.environmentCoefficient)) break;
        } else if (kind == "TEXTURES") {
            if (!(stream >> decoded.albedoTextureGuid >> decoded.normalTextureGuid)) break;
            if (decoded.albedoTextureGuid == "-") decoded.albedoTextureGuid.clear();
            if (decoded.normalTextureGuid == "-") decoded.normalTextureGuid.clear();
        } else if (kind == "REVISION") {
            if (!(stream >> decoded.revision)) break;
        } else if (kind == "END") {
            hasEnd = true;
            break;
        } else {
            SetError(errorMessage, "Material Instance contains an unknown record: " + kind);
            return false;
        }
    }
    if (!hasAsset || !hasParent || !hasEnd ||
        !IsDurableEditorAssetGuid(decoded.assetGuid) ||
        !IsDurableEditorAssetGuid(decoded.parentMaterialGuid) ||
        !Finite(decoded.baseColor.x) || !Finite(decoded.baseColor.y) ||
        !Finite(decoded.baseColor.z) || !Finite(decoded.baseColor.w) ||
        !Finite(decoded.roughness) || !Finite(decoded.metallic) ||
        !Finite(decoded.environmentCoefficient) || decoded.roughness < 0.0f ||
        decoded.roughness > 1.0f || decoded.metallic < 0.0f || decoded.metallic > 1.0f ||
        decoded.environmentCoefficient < 0.0f || decoded.environmentCoefficient > 1.0f ||
        (!decoded.albedoTextureGuid.empty() && !IsDurableEditorAssetGuid(decoded.albedoTextureGuid)) ||
        (!decoded.normalTextureGuid.empty() && !IsDurableEditorAssetGuid(decoded.normalTextureGuid))) {
        SetError(errorMessage, "Material Instance is incomplete or has invalid overrides.");
        return false;
    }
    output = std::move(decoded);
    return true;
}

bool LoadEditorMaterialInstance(
    const std::filesystem::path& path,
    EditorMaterialInstanceAsset& output,
    std::string* errorMessage) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        SetError(errorMessage, "Could not open Material Instance: " + path.string());
        return false;
    }
    const std::string content{std::istreambuf_iterator<char>(file), {}};
    if (content.size() > 4u * 1024u * 1024u) {
        SetError(errorMessage, "Material Instance exceeds the 4 MiB safety limit.");
        return false;
    }
    return DecodeEditorMaterialInstance(content, output, errorMessage);
}

bool EditorProductionMaterialPipeline::Initialize(
    ID3D12Device* device,
    std::string* errorMessage) {
    Shutdown();
    if (device == nullptr) {
        SetError(errorMessage, "E-7 requires a D3D12 device.");
        return false;
    }
    device_ = device;
    return EnsureLightResources(errorMessage);
}

void EditorProductionMaterialPipeline::Shutdown() {
    for (auto& [guid, material] : materials_) {
        (void)guid;
        if (material.resource && material.mapped != nullptr) material.resource->Unmap(0, nullptr);
    }
    if (directionalResource_ && mappedDirectional_ != nullptr) directionalResource_->Unmap(0, nullptr);
    if (pointResource_ && mappedPoint_ != nullptr) pointResource_->Unmap(0, nullptr);
    if (spotResource_ && mappedSpot_ != nullptr) spotResource_->Unmap(0, nullptr);
    materials_.clear();
    bindings_.clear();
    pendingResources_.clear();
    directionalResource_.Reset();
    pointResource_.Reset();
    spotResource_.Reset();
    mappedDirectional_ = nullptr;
    mappedPoint_ = nullptr;
    mappedSpot_ = nullptr;
    lighting_ = {};
    stats_ = {};
    diagnostics_.clear();
    device_.Reset();
}

bool EditorProductionMaterialPipeline::EnsureLightResources(std::string* errorMessage) {
    if (device_ == nullptr) return true;
    if (!directionalResource_ &&
        (!CreateUploadBuffer(device_.Get(), sizeof(DirectionalLight), directionalResource_, errorMessage) ||
         !MapResource(directionalResource_.Get(), mappedDirectional_, errorMessage))) return false;
    if (!pointResource_ &&
        (!CreateUploadBuffer(device_.Get(), sizeof(PointLight), pointResource_, errorMessage) ||
         !MapResource(pointResource_.Get(), mappedPoint_, errorMessage))) return false;
    if (!spotResource_ &&
        (!CreateUploadBuffer(device_.Get(), sizeof(SpotLight), spotResource_, errorMessage) ||
         !MapResource(spotResource_.Get(), mappedSpot_, errorMessage))) return false;
    return true;
}

bool EditorProductionMaterialPipeline::ResolveMaterial(
    const EditorAssetRecord& record,
    const EditorAssetRegistry& registry,
    uint64_t scheduledFenceValue,
    ResidentMaterial*& output,
    std::string* errorMessage) {
    output = nullptr;
    const uint64_t timestamp = record.sourceTimestamp != 0
        ? record.sourceTimestamp : FileTimestamp(record.sourcePath);
    EditorMaterialInstanceAsset asset{};
    if (!LoadEditorMaterialInstance(record.sourcePath, asset, errorMessage) ||
        asset.assetGuid != record.guid) {
        if (errorMessage != nullptr && errorMessage->empty())
            *errorMessage = "Material Instance durable GUID does not match registry metadata.";
        return false;
    }
    const EditorAssetRecord* parent = registry.FindByGuid(asset.parentMaterialGuid);
    if (parent == nullptr || parent->kind != EditorAssetKind::MaterialGraph || parent->missing) {
        SetError(errorMessage, "Material Instance parent Material Graph is unavailable.");
        return false;
    }
    EditorDocumentContent graphContent{};
    EditorMaterialGraphAsset graph{};
    std::string graphError;
    EditorMaterialGraphDocumentProvider graphProvider;
    const bool graphReady = graphProvider.ReadSource(parent->sourcePath, &graphContent, &graphError) &&
        EditorMaterialGraphDocumentProvider::Decode(graphContent, &graph, &graphError);
    const EditorMaterialCompileArtifact artifact = graphReady
        ? CompileEditorMaterialGraph(graph, BuildEditorMaterialGraphSchema())
        : EditorMaterialCompileArtifact{};
    if (!graphReady || !artifact.succeeded) {
        SetError(errorMessage, graphError.empty()
            ? "Material Instance parent Material Graph did not compile."
            : graphError);
        return false;
    }
    const uint64_t parentTimestamp = parent->sourceTimestamp != 0
        ? parent->sourceTimestamp : FileTimestamp(parent->sourcePath);
    uint64_t variantHash = artifact.sourceFingerprint;
    variantHash = Mix(variantHash, static_cast<uint64_t>(graph.domain));
    variantHash = Mix(variantHash, static_cast<uint64_t>(graph.blendMode));
    variantHash = Mix(variantHash, static_cast<uint64_t>(graph.shadingModel));

    auto found = materials_.find(record.guid);
    if (found != materials_.end() && found->second.sourceTimestamp == timestamp &&
        found->second.parentTimestamp == parentTimestamp &&
        found->second.shaderVariantHash == variantHash) {
        output = &found->second;
        return true;
    }
    ResidentMaterial built{};
    built.asset = std::move(asset);
    built.sourceTimestamp = timestamp;
    built.parentTimestamp = parentTimestamp;
    built.shaderVariantHash = variantHash;
    built.shaderSource.materialAssetGuid = record.guid;
    built.shaderSource.shaderVariantHash = variantHash;
    built.shaderSource.graphSourceFingerprint = artifact.sourceFingerprint;
    built.shaderSource.domain = graph.domain;
    built.shaderSource.blendMode = graph.blendMode;
    built.shaderSource.shadingModel = graph.shadingModel;
    built.shaderSource.graphHlslSource = artifact.hlslSource;
    built.shaderSource.textureAssetGuids = artifact.textureAssetGuids;
    if (device_ != nullptr) {
        if (!CreateUploadBuffer(device_.Get(), sizeof(Material), built.resource, errorMessage) ||
            !MapResource(built.resource.Get(), built.mapped, errorMessage)) return false;
        built.mapped->color = built.asset.baseColor;
        built.mapped->enableLighting = graph.shadingModel == EditorMaterialShadingModel::Lit;
        built.mapped->uvTransform = MakeIdentity4x4();
        built.mapped->shininess = 1.0f + (1.0f - built.asset.roughness) * 127.0f;
        built.mapped->environmentCoefficient = Clamp(
            (std::max)(built.asset.environmentCoefficient, built.asset.metallic), 0.0f, 1.0f);
        built.mapped->specularMode = 1;
    }
    if (found != materials_.end()) {
        if (found->second.resource) {
            if (found->second.mapped != nullptr) found->second.resource->Unmap(0, nullptr);
            pendingResources_.push_back({found->second.resource, scheduledFenceValue});
        }
        found->second = std::move(built);
        output = &found->second;
        ++stats_.hotReloads;
    } else {
        output = &materials_.emplace(record.guid, std::move(built)).first->second;
    }
    return true;
}

bool EditorProductionMaterialPipeline::Sync(
    const EditorScene& scene,
    const EditorAssetRegistry& registry,
    uint64_t completedFenceValue,
    uint64_t scheduledFenceValue,
    std::string* errorMessage,
    const std::unordered_set<std::string>* sourceResidentEntities) {
    CollectRetired(completedFenceValue);
    bindings_.clear();
    diagnostics_.clear();
    const uint64_t priorHotReloads = stats_.hotReloads;
    stats_ = {};
    stats_.hotReloads = priorHotReloads;
    if (device_ != nullptr && !EnsureLightResources(errorMessage)) return false;

    std::unordered_set<std::string> activeMaterials;
    for (const EditorSceneEntity& entity : scene.entities) {
        if (sourceResidentEntities != nullptr &&
            !sourceResidentEntities->contains(entity.guid)) continue;
        const EditorSceneComponent* renderer = scene.FindComponent(entity, kEditorMeshRendererComponentType);
        if (renderer == nullptr || !renderer->enabled) continue;
        bool hasSlot = false;
        for (const EditorSceneObjectReference& reference : renderer->references) {
            uint32_t slot = 0;
            if (!ParseSlot(reference.property, slot)) continue;
            hasSlot = true;
            ++stats_.requestedBindings;
            EditorProductionMaterialBinding binding{};
            binding.entityGuid = entity.guid;
            binding.materialSlot = slot;
            binding.materialAssetGuid = reference.assetGuid;
            const EditorAssetRecord* record = registry.FindByGuid(reference.assetGuid);
            ResidentMaterial* material = nullptr;
            std::string materialError;
            if (record != nullptr && record->kind == EditorAssetKind::MaterialInstance &&
                !record->missing && ResolveMaterial(*record, registry, scheduledFenceValue,
                    material, &materialError) && material != nullptr) {
                binding.parentMaterialGuid = material->asset.parentMaterialGuid;
                binding.shaderVariantHash = material->shaderVariantHash;
                binding.materialAddress = material->resource
                    ? material->resource->GetGPUVirtualAddress() : 0;
                binding.albedoTextureGuid = material->asset.albedoTextureGuid;
                binding.normalTextureGuid = material->asset.normalTextureGuid;
                binding.fallback = false;
                activeMaterials.insert(record->guid);
                ++stats_.resolvedBindings;
            } else {
                ++stats_.fallbackBindings;
                diagnostics_.push_back("Mesh Entity '" + entity.name + "' material slot " +
                    std::to_string(slot) + " uses fallback: " +
                    (materialError.empty() ? "Material Instance is unresolved." : materialError));
            }
            bindings_.push_back(std::move(binding));
        }
        if (!hasSlot) {
            ++stats_.requestedBindings;
            ++stats_.fallbackBindings;
            EditorProductionMaterialBinding fallback{};
            fallback.entityGuid = entity.guid;
            bindings_.push_back(std::move(fallback));
        }
    }
    for (auto iterator = materials_.begin(); iterator != materials_.end();) {
        if (activeMaterials.contains(iterator->first)) {
            ++iterator;
            continue;
        }
        if (iterator->second.resource) {
            if (iterator->second.mapped != nullptr) iterator->second.resource->Unmap(0, nullptr);
            pendingResources_.push_back({iterator->second.resource, scheduledFenceValue});
        }
        iterator = materials_.erase(iterator);
    }

    std::unordered_map<std::string, Matrix4x4> worldMatrices;
    std::unordered_set<std::string> visiting;
    const auto resolveWorld = [&](const auto& self, const EditorSceneEntity& entity) -> Matrix4x4 {
        if (const auto cached = worldMatrices.find(entity.guid); cached != worldMatrices.end())
            return cached->second;
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
        worldMatrices.insert_or_assign(entity.guid, world);
        return world;
    };
    std::vector<LightCandidate> directional;
    std::vector<LightCandidate> point;
    std::vector<LightCandidate> spot;
    for (const EditorSceneEntity& entity : scene.entities) {
        if (sourceResidentEntities != nullptr &&
            !sourceResidentEntities->contains(entity.guid)) continue;
        if (!entity.visible) continue;
        const auto append = [&](std::string_view type, std::vector<LightCandidate>& output) {
            const EditorSceneComponent* component = scene.FindComponent(entity, type);
            if (component != nullptr && component->enabled) output.push_back(
                {&entity, component, resolveWorld(resolveWorld, entity),
                    FloatProperty(*component, "priority", 0.0f)});
        };
        append(kEditorDirectionalLightComponentType, directional);
        append(kEditorPointLightComponentType, point);
        append(kEditorSpotLightComponentType, spot);
    }
    const auto order = [](const LightCandidate& a, const LightCandidate& b) {
        if (a.priority != b.priority) return a.priority > b.priority;
        return a.entity->guid < b.entity->guid;
    };
    std::sort(directional.begin(), directional.end(), order);
    std::sort(point.begin(), point.end(), order);
    std::sort(spot.begin(), spot.end(), order);
    lighting_ = {};
    lighting_.directionalCount = static_cast<uint32_t>(directional.size());
    lighting_.pointCount = static_cast<uint32_t>(point.size());
    lighting_.spotCount = static_cast<uint32_t>(spot.size());
    lighting_.directional.color = {1.0f, 1.0f, 1.0f, 1.0f};
    lighting_.directional.direction = {0.0f, -1.0f, 0.0f};
    lighting_.directional.intensity = 0.0f;
    lighting_.point.color = {1.0f, 1.0f, 1.0f, 1.0f};
    lighting_.point.intensity = 0.0f;
    lighting_.point.radius = 10.0f;
    lighting_.point.decay = 2.0f;
    lighting_.spot.color = {1.0f, 1.0f, 1.0f, 1.0f};
    lighting_.spot.intensity = 0.0f;
    lighting_.spot.distance = 10.0f;
    lighting_.spot.decay = 2.0f;
    lighting_.spot.cosAngle = std::cos(0.5f);
    if (!directional.empty()) {
        const auto& light = *directional.front().component;
        lighting_.directional.color = ColorProperty(light, "color", lighting_.directional.color);
        lighting_.directional.direction = NormalizeOr(
            VectorProperty(light, "direction", {0.0f, -1.0f, 0.0f}), {0.0f, -1.0f, 0.0f});
        lighting_.directional.intensity = Clamp(FloatProperty(light, "intensity", 1.0f), 0.0f, 100000.0f);
    }
    if (!point.empty()) {
        const auto& candidate = point.front();
        const auto& light = *candidate.component;
        lighting_.point.color = ColorProperty(light, "color", lighting_.point.color);
        lighting_.point.position = {candidate.world.m[3][0], candidate.world.m[3][1], candidate.world.m[3][2]};
        lighting_.point.intensity = Clamp(FloatProperty(light, "intensity", 1.0f), 0.0f, 100000.0f);
        lighting_.point.radius = Clamp(FloatProperty(light, "radius", 10.0f), 0.001f, 1000000.0f);
        lighting_.point.decay = Clamp(FloatProperty(light, "decay", 2.0f), 0.0f, 64.0f);
    }
    if (!spot.empty()) {
        const auto& candidate = spot.front();
        const auto& light = *candidate.component;
        lighting_.spot.color = ColorProperty(light, "color", lighting_.spot.color);
        lighting_.spot.position = {candidate.world.m[3][0], candidate.world.m[3][1], candidate.world.m[3][2]};
        lighting_.spot.direction = NormalizeOr(
            VectorProperty(light, "direction", {0.0f, -1.0f, 0.0f}), {0.0f, -1.0f, 0.0f});
        lighting_.spot.intensity = Clamp(FloatProperty(light, "intensity", 1.0f), 0.0f, 100000.0f);
        lighting_.spot.distance = Clamp(FloatProperty(light, "distance", 10.0f), 0.001f, 1000000.0f);
        lighting_.spot.decay = Clamp(FloatProperty(light, "decay", 2.0f), 0.0f, 64.0f);
        const float angleDegrees = Clamp(FloatProperty(light, "angle", 30.0f), 0.1f, 89.9f);
        lighting_.spot.cosAngle = std::cos(angleDegrees * 3.14159265358979323846f / 180.0f);
    }
    if (directional.size() > 1 || point.size() > 1 || spot.size() > 1) {
        diagnostics_.push_back("E-7 selected the highest-priority light for the current single-light shader contract.");
    }
    if (mappedDirectional_ != nullptr) *mappedDirectional_ = lighting_.directional;
    if (mappedPoint_ != nullptr) *mappedPoint_ = lighting_.point;
    if (mappedSpot_ != nullptr) *mappedSpot_ = lighting_.spot;
    lighting_.directionalAddress = directionalResource_ ? directionalResource_->GetGPUVirtualAddress() : 0;
    lighting_.pointAddress = pointResource_ ? pointResource_->GetGPUVirtualAddress() : 0;
    lighting_.spotAddress = spotResource_ ? spotResource_->GetGPUVirtualAddress() : 0;

    stats_.residentMaterialInstances = static_cast<uint32_t>(materials_.size());
    std::set<uint64_t> variants;
    for (const auto& [guid, material] : materials_) {
        (void)guid;
        variants.insert(material.shaderVariantHash);
        if (material.resource) stats_.residentGpuBytes += 256;
    }
    stats_.residentShaderVariants = static_cast<uint32_t>(variants.size());
    stats_.pendingGpuRetirements = static_cast<uint32_t>(pendingResources_.size());
    if (!diagnostics_.empty() && errorMessage != nullptr) *errorMessage = diagnostics_.front();
    return true;
}

const EditorProductionMaterialBinding* EditorProductionMaterialPipeline::Resolve(
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

const EditorProductionMaterialShaderSource*
EditorProductionMaterialPipeline::ResolveShaderSource(
    std::string_view materialAssetGuid) const {
    const auto found = materials_.find(std::string(materialAssetGuid));
    return found == materials_.end() ? nullptr : &found->second.shaderSource;
}

void EditorProductionMaterialPipeline::CollectRetired(uint64_t completedFenceValue) {
    std::erase_if(pendingResources_, [&](const PendingResource& pending) {
        return pending.retireFenceValue <= completedFenceValue;
    });
}

} // namespace editor
