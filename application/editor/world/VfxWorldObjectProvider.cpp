#include "VfxWorldObjectProvider.h"

#include "../../EffectRuntime.h"

#include <algorithm>
#include <utility>

namespace editor {
namespace {

EditorObjectHandle MakeHandle(
    const EditorDocumentId& document,
    std::string_view provider,
    std::string_view guid,
    EditorDomainId domain,
    uint64_t localIndex,
    uint32_t generation,
    std::string displayName) {
    EditorObjectHandle handle{};
    handle.domain = domain;
    handle.stableId = BuildEditorWorldStableId(document, provider, guid);
    handle.localIndex = localIndex;
    handle.generation = generation;
    handle.displayName = std::move(displayName);
    return handle;
}

EditorWorldObjectRecord MakeVirtual(
    const EditorDocumentId& document,
    std::string_view provider,
    std::string_view guid,
    std::string displayName,
    std::string sortKey,
    const EditorObjectHandle& parent = {}) {
    EditorWorldObjectRecord record{};
    record.document = document;
    record.providerId = std::string(provider);
    record.objectGuid = std::string(guid);
    record.displayName = std::move(displayName);
    record.typeName = "Folder";
    record.sortKey = std::move(sortKey);
    record.handle = MakeHandle(
        document, provider, guid, EditorDomainId::Unknown, 0, 0, record.displayName);
    record.parent = parent;
    record.virtualNode = true;
    return record;
}

} // namespace

void VfxWorldObjectProvider::Bind(const EffectRuntime* runtime, EditorDocumentId document) {
    runtime_ = runtime;
    document_ = std::move(document);
}

bool VfxWorldObjectProvider::Enumerate(
    EditorWorldProviderEnumeration* output,
    std::string* errorMessage) const {
    if (output == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "VFX world enumeration output is null.";
        return false;
    }
    output->objects.clear();
    output->diagnostics.clear();
    if (runtime_ == nullptr || !document_.IsValid()) return true;

    const std::string_view provider = ProviderId();
    EditorWorldObjectRecord root = MakeVirtual(
        document_, provider, "root", "VFX", "00");
    const EditorObjectHandle rootHandle = root.handle;
    output->objects.push_back(std::move(root));
    EditorWorldObjectRecord assetFolder = MakeVirtual(
        document_, provider, "folder-assets", "Effect Assets", "01", rootHandle);
    EditorWorldObjectRecord instanceFolder = MakeVirtual(
        document_, provider, "folder-instances", "Runtime Instances", "02", rootHandle);
    const EditorObjectHandle assetParent = assetFolder.handle;
    const EditorObjectHandle instanceParent = instanceFolder.handle;
    output->objects.push_back(std::move(assetFolder));
    output->objects.push_back(std::move(instanceFolder));

    std::vector<std::string> assetNames;
    assetNames.reserve(runtime_->Assets().size());
    for (const auto& [name, asset] : runtime_->Assets()) {
        (void)asset;
        assetNames.push_back(name);
    }
    std::sort(assetNames.begin(), assetNames.end());
    for (std::size_t index = 0; index < assetNames.size(); ++index) {
        const std::string& name = assetNames[index];
        const std::string guid = MakeDeterministicEditorWorldGuid(
            document_.assetGuid, "vfx-asset", name, 0);
        EditorWorldObjectRecord record{};
        record.document = document_;
        record.providerId = std::string(provider);
        record.objectGuid = guid;
        record.displayName = name;
        record.typeName = "VFX Effect Asset";
        record.sortKey = "01:" + name;
        record.handle = MakeHandle(
            document_, provider, guid, EditorDomainId::VfxEffectAsset,
            index, 0, record.displayName);
        record.parent = assetParent;
        record.locked = true;
        output->objects.push_back(std::move(record));
    }

    const uint32_t generation = static_cast<uint32_t>(runtime_->ParticlePoolResetSerial());
    std::vector<const EffectInstance*> instances;
    instances.reserve(runtime_->Instances().size());
    for (const EffectInstance& instance : runtime_->Instances()) instances.push_back(&instance);
    std::sort(instances.begin(), instances.end(), [](const auto* lhs, const auto* rhs) {
        return lhs->id < rhs->id;
    });
    for (std::size_t index = 0; index < instances.size(); ++index) {
        const EffectInstance& value = *instances[index];
        const std::string guid = "instance-" + std::to_string(value.id);
        EditorWorldObjectRecord record{};
        record.document = document_;
        record.providerId = std::string(provider);
        record.objectGuid = guid;
        record.displayName = value.assetName + " #" + std::to_string(value.id);
        record.typeName = "VFX Runtime Instance";
        record.sortKey = "02:" + std::to_string(value.id);
        record.handle = MakeHandle(
            document_, provider, guid, EditorDomainId::VfxEffectInstance,
            index, generation, record.displayName);
        record.parent = instanceParent;
        record.runtimeOnly = true;
        record.locked = true;
        output->objects.push_back(std::move(record));
    }
    return true;
}

bool VfxWorldObjectProvider::Resolve(
    const EditorObjectHandle& handle,
    EditorWorldObjectRecord* record) const {
    EditorWorldProviderEnumeration enumeration{};
    if (!Enumerate(&enumeration, nullptr)) return false;
    for (const EditorWorldObjectRecord& candidate : enumeration.objects) {
        if (!candidate.handle.SameObject(handle)) continue;
        if (record != nullptr) *record = candidate;
        return true;
    }
    return false;
}

} // namespace editor
