#pragma once

#include "EditorGimmickComponent.h"
#include "EditorGimmickRuntimeBehavior.h"
#include "EditorGimmickRuntimeCommandQueue.h"
#include "EditorGimmickRuntimeLifecycle.h"
#include "EditorSceneRuntimeInstantiation.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

inline constexpr std::string_view kEditorGimmickRuntimeTargetServiceId =
    "runtime.gimmick.target";

struct EditorGimmickRuntimeInstance {
    EditorGimmickRuntimeInstance();
    ~EditorGimmickRuntimeInstance();
    EditorGimmickRuntimeInstance(
        EditorGimmickRuntimeInstance&&) noexcept;
    EditorGimmickRuntimeInstance& operator=(
        EditorGimmickRuntimeInstance&&) noexcept;
    EditorGimmickRuntimeInstance(
        const EditorGimmickRuntimeInstance&) = delete;
    EditorGimmickRuntimeInstance& operator=(
        const EditorGimmickRuntimeInstance&) = delete;

    std::string stableId;
    std::string entityGuid;
    std::string definitionId;
    std::string runtimeFactoryId;
    uint64_t sourceHash = 0;
    EditorGimmickActivationMode activationMode =
        EditorGimmickActivationMode::Interaction;
    bool oneShot = false;
    float cooldown = 0.0f;
    std::vector<EditorGimmickParameterValue> parameters;
    std::vector<EditorGimmickEntityReferenceValue>
        entityReferences;
    EditorGimmickRuntimeLifecycle lifecycle;
    std::unique_ptr<IEditorGimmickRuntimeBehavior> behavior;
    uint64_t lastCommandSequence = 0;
    EditorGimmickRuntimeCommandKind lastCommandKind =
        EditorGimmickRuntimeCommandKind::Activate;
    std::string lastCommandSourceEntityGuid;

    const EditorGimmickParameterValue* FindParameter(
        std::string_view parameterId) const noexcept;
    const EditorGimmickEntityReferenceValue* FindEntityReference(
        std::string_view parameterId) const noexcept;
};

class IEditorGimmickDefinitionRuntimeFactory {
public:
    virtual ~IEditorGimmickDefinitionRuntimeFactory() = default;

    virtual std::string_view RuntimeFactoryId() const noexcept = 0;
    virtual std::string_view DefinitionTypeId() const noexcept = 0;
    virtual bool Build(
        const EditorScene& scene,
        const EditorSceneRuntimeComponentRecord& source,
        const EditorGimmickDefinition& definition,
        const EditorGimmickComponent& authored,
        EditorGimmickRuntimeInstance& output,
        std::string* errorMessage = nullptr) const = 0;
};

class EditorGimmickDefinitionRuntimeFactoryRegistry {
public:
    bool Register(
        std::unique_ptr<IEditorGimmickDefinitionRuntimeFactory>
            factory,
        std::string* errorMessage = nullptr);
    bool Remove(std::string_view runtimeFactoryId);
    void Clear();

    IEditorGimmickDefinitionRuntimeFactory* Find(
        std::string_view runtimeFactoryId);
    const IEditorGimmickDefinitionRuntimeFactory* Find(
        std::string_view runtimeFactoryId) const;
    const IEditorGimmickDefinitionRuntimeFactory* FindByDefinition(
        std::string_view definitionTypeId) const;
    std::vector<const IEditorGimmickDefinitionRuntimeFactory*>
    Ordered() const;
    std::size_t Count() const noexcept {
        return factories_.size();
    }

    bool ValidateAgainstDefinitions(
        const EditorGimmickDefinitionRegistry& definitions,
        std::string* errorMessage = nullptr) const;

private:
    std::vector<
        std::unique_ptr<IEditorGimmickDefinitionRuntimeFactory>>
        factories_;
};

class EditorGimmickRuntimeWorld {
public:
    bool Replace(
        const EditorScene& sourceScene,
        std::vector<EditorGimmickRuntimeInstance> instances,
        std::string* errorMessage = nullptr);
    void SuspendForReconcile() noexcept;
    void Clear() noexcept;
    bool EnqueueCommand(
        std::string targetEntityGuid,
        EditorGimmickRuntimeCommandKind kind,
        std::string sourceEntityGuid = {},
        std::string payload = {},
        std::string* errorMessage = nullptr);
    void Update(float deltaTime);

    EditorGimmickRuntimeInstance* Find(
        std::string_view stableId);
    const EditorGimmickRuntimeInstance* Find(
        std::string_view stableId) const;
    EditorGimmickRuntimeInstance* FindByEntity(
        std::string_view entityGuid);
    const EditorGimmickRuntimeInstance* FindByEntity(
        std::string_view entityGuid) const;
    const std::vector<EditorGimmickRuntimeInstance>&
    Instances() const noexcept {
        return instances_;
    }
    bool Active() const noexcept { return active_; }
    uint64_t Revision() const noexcept { return revision_; }
    const EditorGimmickRuntimeCommandQueue&
    Commands() const noexcept {
        return commands_;
    }

private:
    std::vector<EditorGimmickRuntimeInstance> instances_;
    std::vector<EditorGimmickRuntimeInstance>
        suspendedInstances_;
    EditorGimmickRuntimeCommandQueue commands_;
    bool active_ = false;
    uint64_t revision_ = 0;
};

struct EditorGimmickRuntimeTarget {
    const EditorGimmickDefinitionRegistry* definitions = nullptr;
    EditorGimmickDefinitionRuntimeFactoryRegistry*
        definitionFactories = nullptr;
    EditorGimmickRuntimeWorld* world = nullptr;
};

class EditorGimmickRuntimeFactory final
    : public IEditorSceneRuntimeComponentFactory {
public:
    std::string_view TypeId() const noexcept override {
        return kEditorGimmickComponentType;
    }
    int32_t Priority() const noexcept override { return 160; }

    EditorSceneRuntimeFactoryResult Instantiate(
        const EditorScene& scene,
        const std::vector<EditorSceneRuntimeComponentRecord>&
            components,
        const EditorSceneRuntimeServiceRegistry& services) override;
    void Destroy() noexcept override;

private:
    EditorGimmickRuntimeWorld* activeWorld_ = nullptr;
};

bool RegisterBuiltInEditorGimmickDefinitionRuntimeFactories(
    EditorGimmickDefinitionRuntimeFactoryRegistry& registry,
    const EditorGimmickDefinitionRegistry& definitions =
        BuiltInEditorGimmickDefinitionRegistry(),
    std::string* errorMessage = nullptr);

} // namespace editor
