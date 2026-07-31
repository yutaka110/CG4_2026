#include "EditorGimmickRuntimeFactory.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <utility>

namespace editor {
namespace {

bool SafeId(std::string_view id) {
    if (id.empty() || id.size() > 128) return false;
    return std::all_of(
        id.begin(), id.end(),
        [](unsigned char character) {
            return
                (character >= 'a' && character <= 'z') ||
                (character >= 'A' && character <= 'Z') ||
                (character >= '0' && character <= '9') ||
                character == '_' || character == '-' ||
                character == '.';
        });
}

void SetError(
    std::string* errorMessage,
    std::string message) {
    if (errorMessage != nullptr) {
        *errorMessage = std::move(message);
    }
}

bool ResolveEntityReferences(
    const EditorScene& scene,
    const EditorSceneEntity& owner,
    const EditorGimmickDefinition& definition,
    EditorGimmickRuntimeInstance& instance,
    std::string* errorMessage) {
    for (const EditorGimmickParameterDefinition& parameter :
         definition.parameters) {
        if (parameter.kind !=
            EditorGimmickParameterKind::EntityReference) {
            continue;
        }
        EditorGimmickEntityReferenceValue* runtimeReference =
            nullptr;
        for (EditorGimmickEntityReferenceValue& value :
             instance.entityReferences) {
            if (value.id == parameter.id) {
                runtimeReference = &value;
                break;
            }
        }
        if (runtimeReference == nullptr) {
            SetError(
                errorMessage,
                "Runtime Gimmick is missing Entity Reference storage: " +
                    definition.typeId + "." + parameter.id);
            return false;
        }
        if (runtimeReference->entityGuid.empty() &&
            parameter.entityReferenceDefaultsToSelf) {
            runtimeReference->entityGuid = owner.guid;
        }
        if (runtimeReference->entityGuid.empty()) {
            if (parameter.required) {
                SetError(
                    errorMessage,
                    "Required Runtime Gimmick Entity Reference is "
                    "unresolved: " +
                        definition.typeId + "." + parameter.id);
                return false;
            }
            continue;
        }

        const EditorSceneEntity* referenced =
            scene.FindEntity(runtimeReference->entityGuid);
        if (referenced == nullptr) {
            SetError(
                errorMessage,
                "Runtime Gimmick Entity Reference does not resolve: " +
                    definition.typeId + "." + parameter.id);
            return false;
        }
        if (!parameter.entityReferenceTargetComponentType.empty()) {
            const EditorSceneComponent* requiredComponent =
                scene.FindComponent(
                    *referenced,
                    parameter.entityReferenceTargetComponentType);
            if (requiredComponent == nullptr ||
                !requiredComponent->enabled ||
                !scene.IsRuntimeActiveInHierarchy(referenced->guid)) {
                SetError(
                    errorMessage,
                    "Runtime Gimmick Entity Reference target is not an "
                    "active Entity with Component \"" +
                        parameter.entityReferenceTargetComponentType +
                        "\": " + definition.typeId + "." +
                        parameter.id);
                return false;
            }
        }
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

class EditorBuiltInGimmickDefinitionRuntimeFactory final
    : public IEditorGimmickDefinitionRuntimeFactory {
public:
    EditorBuiltInGimmickDefinitionRuntimeFactory(
        std::string definitionTypeId,
        std::string runtimeFactoryId)
        : definitionTypeId_(std::move(definitionTypeId)),
          runtimeFactoryId_(std::move(runtimeFactoryId)) {}

    std::string_view RuntimeFactoryId() const noexcept override {
        return runtimeFactoryId_;
    }
    std::string_view DefinitionTypeId() const noexcept override {
        return definitionTypeId_;
    }

    bool Build(
        const EditorScene& scene,
        const EditorSceneRuntimeComponentRecord& source,
        const EditorGimmickDefinition& definition,
        const EditorGimmickComponent& authored,
        EditorGimmickRuntimeInstance& output,
        std::string* errorMessage) const override {
        if (source.entity == nullptr ||
            source.component == nullptr ||
            definition.typeId != definitionTypeId_ ||
            definition.runtimeFactoryId != runtimeFactoryId_ ||
            authored.definitionId != definitionTypeId_) {
            SetError(
                errorMessage,
                "Built-in Gimmick Definition Factory received an "
                "incompatible Definition or source Component.");
            return false;
        }
        EditorGimmickRuntimeInstance built{};
        built.stableId = source.stableId;
        built.entityGuid = source.entity->guid;
        built.definitionId = definition.typeId;
        built.runtimeFactoryId = runtimeFactoryId_;
        built.sourceHash = source.sourceHash;
        built.activationMode = authored.activationMode;
        built.oneShot = authored.oneShot;
        built.cooldown = authored.cooldown;
        built.parameters = authored.parameters;
        built.entityReferences = authored.entityReferences;
        if (!ResolveEntityReferences(
                scene,
                *source.entity,
                definition,
                built,
                errorMessage)) {
            return false;
        }
        if (!built.lifecycle.Configure(
                built.activationMode,
                built.oneShot,
                built.cooldown,
                true)) {
            SetError(
                errorMessage,
                "Runtime Gimmick Lifecycle rejected common settings.");
            return false;
        }
        built.behavior =
            CreateBuiltInEditorGimmickRuntimeBehavior(
                definition.typeId);
        if (built.behavior == nullptr ||
            !built.behavior->Initialize(
                built, errorMessage)) {
            if (errorMessage != nullptr &&
                errorMessage->empty()) {
                *errorMessage =
                    "Runtime Gimmick Behavior initialization failed.";
            }
            return false;
        }
        output = std::move(built);
        if (errorMessage != nullptr) errorMessage->clear();
        return true;
    }

private:
    std::string definitionTypeId_;
    std::string runtimeFactoryId_;
};

} // namespace

EditorGimmickRuntimeInstance::EditorGimmickRuntimeInstance() =
    default;
EditorGimmickRuntimeInstance::~EditorGimmickRuntimeInstance() =
    default;
EditorGimmickRuntimeInstance::EditorGimmickRuntimeInstance(
    EditorGimmickRuntimeInstance&&) noexcept = default;
EditorGimmickRuntimeInstance&
EditorGimmickRuntimeInstance::operator=(
    EditorGimmickRuntimeInstance&&) noexcept = default;

const EditorGimmickParameterValue*
EditorGimmickRuntimeInstance::FindParameter(
    std::string_view parameterId) const noexcept {
    const auto found = std::find_if(
        parameters.begin(),
        parameters.end(),
        [&](const EditorGimmickParameterValue& value) {
            return value.id == parameterId;
        });
    return found == parameters.end() ? nullptr : &*found;
}

const EditorGimmickEntityReferenceValue*
EditorGimmickRuntimeInstance::FindEntityReference(
    std::string_view parameterId) const noexcept {
    const auto found = std::find_if(
        entityReferences.begin(),
        entityReferences.end(),
        [&](const EditorGimmickEntityReferenceValue& value) {
            return value.id == parameterId;
        });
    return found == entityReferences.end() ? nullptr : &*found;
}

bool EditorGimmickDefinitionRuntimeFactoryRegistry::Register(
    std::unique_ptr<IEditorGimmickDefinitionRuntimeFactory> factory,
    std::string* errorMessage) {
    const auto fail = [&](std::string message) {
        SetError(errorMessage, std::move(message));
        return false;
    };
    if (factory == nullptr ||
        !SafeId(factory->RuntimeFactoryId()) ||
        !SafeId(factory->DefinitionTypeId())) {
        return fail(
            "Gimmick Definition Runtime Factory requires safe Runtime "
            "Factory and Definition Type IDs.");
    }
    if (Find(factory->RuntimeFactoryId()) != nullptr) {
        return fail(
            "Gimmick Definition Runtime Factory ID is already "
            "registered: " +
            std::string(factory->RuntimeFactoryId()));
    }
    if (FindByDefinition(factory->DefinitionTypeId()) != nullptr) {
        return fail(
            "Gimmick Definition already has a Runtime Factory: " +
            std::string(factory->DefinitionTypeId()));
    }
    factories_.push_back(std::move(factory));
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

bool EditorGimmickDefinitionRuntimeFactoryRegistry::Remove(
    std::string_view runtimeFactoryId) {
    const auto found = std::find_if(
        factories_.begin(),
        factories_.end(),
        [&](const auto& factory) {
            return factory != nullptr &&
                factory->RuntimeFactoryId() == runtimeFactoryId;
        });
    if (found == factories_.end()) return false;
    factories_.erase(found);
    return true;
}

void EditorGimmickDefinitionRuntimeFactoryRegistry::Clear() {
    factories_.clear();
}

IEditorGimmickDefinitionRuntimeFactory*
EditorGimmickDefinitionRuntimeFactoryRegistry::Find(
    std::string_view runtimeFactoryId) {
    return const_cast<IEditorGimmickDefinitionRuntimeFactory*>(
        static_cast<
            const EditorGimmickDefinitionRuntimeFactoryRegistry&>(
                *this)
            .Find(runtimeFactoryId));
}

const IEditorGimmickDefinitionRuntimeFactory*
EditorGimmickDefinitionRuntimeFactoryRegistry::Find(
    std::string_view runtimeFactoryId) const {
    const auto found = std::find_if(
        factories_.begin(),
        factories_.end(),
        [&](const auto& factory) {
            return factory != nullptr &&
                factory->RuntimeFactoryId() == runtimeFactoryId;
        });
    return found == factories_.end() ? nullptr : found->get();
}

const IEditorGimmickDefinitionRuntimeFactory*
EditorGimmickDefinitionRuntimeFactoryRegistry::FindByDefinition(
    std::string_view definitionTypeId) const {
    const auto found = std::find_if(
        factories_.begin(),
        factories_.end(),
        [&](const auto& factory) {
            return factory != nullptr &&
                factory->DefinitionTypeId() == definitionTypeId;
        });
    return found == factories_.end() ? nullptr : found->get();
}

std::vector<const IEditorGimmickDefinitionRuntimeFactory*>
EditorGimmickDefinitionRuntimeFactoryRegistry::Ordered() const {
    std::vector<const IEditorGimmickDefinitionRuntimeFactory*>
        ordered;
    ordered.reserve(factories_.size());
    for (const auto& factory : factories_) {
        if (factory != nullptr) ordered.push_back(factory.get());
    }
    std::sort(
        ordered.begin(),
        ordered.end(),
        [](const auto* left, const auto* right) {
            return left->RuntimeFactoryId() <
                right->RuntimeFactoryId();
        });
    return ordered;
}

bool EditorGimmickDefinitionRuntimeFactoryRegistry::
ValidateAgainstDefinitions(
    const EditorGimmickDefinitionRegistry& definitions,
    std::string* errorMessage) const {
    for (const EditorGimmickDefinition* definition :
         definitions.Ordered()) {
        if (definition == nullptr) continue;
        const IEditorGimmickDefinitionRuntimeFactory* factory =
            Find(definition->runtimeFactoryId);
        if (factory == nullptr) {
            SetError(
                errorMessage,
                "Gimmick Definition Runtime Factory is missing: " +
                    definition->runtimeFactoryId);
            return false;
        }
        if (factory->DefinitionTypeId() != definition->typeId) {
            SetError(
                errorMessage,
                "Gimmick Definition Runtime Factory mapping does not "
                "match Definition \"" +
                    definition->typeId + "\".");
            return false;
        }
    }
    for (const auto& factory : factories_) {
        if (factory == nullptr) continue;
        const EditorGimmickDefinition* definition =
            definitions.Find(factory->DefinitionTypeId());
        if (definition == nullptr ||
            definition->runtimeFactoryId !=
                factory->RuntimeFactoryId()) {
            SetError(
                errorMessage,
                "Registered Gimmick Runtime Factory has no matching "
                "Definition: " +
                    std::string(factory->RuntimeFactoryId()));
            return false;
        }
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

bool EditorGimmickRuntimeWorld::Replace(
    const EditorScene& sourceScene,
    std::vector<EditorGimmickRuntimeInstance> instances,
    std::string* errorMessage) {
    std::unordered_set<std::string> stableIds;
    std::unordered_set<std::string> entityGuids;
    stableIds.reserve(instances.size());
    entityGuids.reserve(instances.size());
    for (const EditorGimmickRuntimeInstance& instance :
         instances) {
        if (instance.stableId.empty() ||
            instance.entityGuid.empty() ||
            instance.definitionId.empty() ||
            instance.runtimeFactoryId.empty() ||
            instance.behavior == nullptr ||
            !std::isfinite(instance.cooldown) ||
            instance.cooldown < 0.0f ||
            !stableIds.insert(instance.stableId).second ||
            !entityGuids.insert(instance.entityGuid).second) {
            SetError(
                errorMessage,
                "Runtime Gimmick instances require unique identities "
                "and valid common settings.");
            return false;
        }
        const EditorSceneEntity* entity =
            sourceScene.FindEntity(instance.entityGuid);
        const EditorSceneComponent* component =
            entity != nullptr
            ? sourceScene.FindComponent(
                *entity, kEditorGimmickComponentType)
            : nullptr;
        if (component == nullptr || !component->enabled ||
            !sourceScene.IsRuntimeActiveInHierarchy(
                instance.entityGuid)) {
            SetError(
                errorMessage,
                "Runtime Gimmick source is not an enabled, "
                "hierarchy-active gameplay.gimmick Component.");
            return false;
        }
    }

    const std::vector<EditorGimmickRuntimeInstance>& previous =
        !suspendedInstances_.empty()
        ? suspendedInstances_
        : instances_;
    std::vector<std::string> automaticTargets;
    automaticTargets.reserve(instances.size());
    for (EditorGimmickRuntimeInstance& instance : instances) {
        const auto previousInstance = std::find_if(
            previous.begin(),
            previous.end(),
            [&](const EditorGimmickRuntimeInstance& candidate) {
                return candidate.stableId == instance.stableId &&
                    candidate.definitionId == instance.definitionId &&
                    candidate.runtimeFactoryId ==
                        instance.runtimeFactoryId;
            });
        if (previousInstance != previous.end()) {
            EditorGimmickRuntimeLifecycle lifecycle =
                previousInstance->lifecycle;
            if (!lifecycle.Reconcile(
                    instance.activationMode,
                    instance.oneShot,
                    instance.cooldown)) {
                SetError(
                    errorMessage,
                    "Runtime Gimmick Lifecycle could not reconcile "
                    "authored settings.");
                return false;
            }
            std::unique_ptr<IEditorGimmickRuntimeBehavior>
                behavior =
                    previousInstance->behavior != nullptr
                    ? previousInstance->behavior->Clone()
                    : nullptr;
            if (behavior == nullptr ||
                !behavior->Reconcile(
                    instance, errorMessage)) {
                if (errorMessage != nullptr &&
                    errorMessage->empty()) {
                    *errorMessage =
                        "Runtime Gimmick Behavior could not reconcile "
                        "authored settings.";
                }
                return false;
            }
            instance.lifecycle = lifecycle;
            instance.behavior = std::move(behavior);
            instance.lastCommandSequence =
                previousInstance->lastCommandSequence;
            instance.lastCommandKind =
                previousInstance->lastCommandKind;
            instance.lastCommandSourceEntityGuid =
                previousInstance->
                    lastCommandSourceEntityGuid;
        } else if (instance.activationMode ==
            EditorGimmickActivationMode::Automatic) {
            automaticTargets.push_back(instance.entityGuid);
        }
    }
    std::sort(
        instances.begin(),
        instances.end(),
        [](const auto& left, const auto& right) {
            return left.stableId < right.stableId;
        });
    instances_ = std::move(instances);
    suspendedInstances_.clear();
    active_ = true;
    ++revision_;
    for (std::string& entityGuid : automaticTargets) {
        EnqueueCommand(
            std::move(entityGuid),
            EditorGimmickRuntimeCommandKind::Activate);
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

void EditorGimmickRuntimeWorld::SuspendForReconcile() noexcept {
    if (!active_ && instances_.empty()) return;
    suspendedInstances_ = std::move(instances_);
    instances_.clear();
    active_ = false;
    ++revision_;
}

void EditorGimmickRuntimeWorld::Clear() noexcept {
    if (!active_ && instances_.empty() &&
        suspendedInstances_.empty() &&
        commands_.PendingCount() == 0 &&
        commands_.ProcessingCount() == 0) {
        return;
    }
    instances_.clear();
    suspendedInstances_.clear();
    commands_.Clear();
    active_ = false;
    ++revision_;
}

bool EditorGimmickRuntimeWorld::EnqueueCommand(
    std::string targetEntityGuid,
    EditorGimmickRuntimeCommandKind kind,
    std::string sourceEntityGuid,
    std::string payload,
    std::string* errorMessage) {
    if (!active_) {
        SetError(
            errorMessage,
            "Runtime Gimmick World is not active.");
        return false;
    }
    return commands_.Enqueue(
        EditorGimmickRuntimeCommand{
            0,
            0,
            kind,
            std::move(targetEntityGuid),
            std::move(sourceEntityGuid),
            std::move(payload)},
        errorMessage);
}

void EditorGimmickRuntimeWorld::Update(float deltaTime) {
    if (!active_ || !std::isfinite(deltaTime) ||
        deltaTime < 0.0f) {
        return;
    }
    for (EditorGimmickRuntimeInstance& instance :
         instances_) {
        instance.lifecycle.Update(deltaTime);
    }
    if (!commands_.BeginFrame(nullptr)) return;
    EditorGimmickRuntimeCommand command{};
    while (commands_.Pop(command)) {
        EditorGimmickRuntimeInstance* target =
            FindByEntity(command.targetEntityGuid);
        if (target == nullptr ||
            target->behavior == nullptr) {
            continue;
        }
        target->lastCommandSequence = command.sequence;
        target->lastCommandKind = command.kind;
        target->lastCommandSourceEntityGuid =
            command.sourceEntityGuid;
        if (command.kind ==
            EditorGimmickRuntimeCommandKind::Enable) {
            target->lifecycle.SetEnabled(true);
            continue;
        }
        if (command.kind ==
            EditorGimmickRuntimeCommandKind::Disable) {
            target->lifecycle.SetEnabled(false);
            continue;
        }
        target->behavior->HandleCommand(
            command, *target, commands_);
    }
    for (EditorGimmickRuntimeInstance& instance :
         instances_) {
        if (instance.behavior != nullptr) {
            instance.behavior->Update(
                deltaTime, instance, commands_);
        }
    }
    ++revision_;
}

EditorGimmickRuntimeInstance*
EditorGimmickRuntimeWorld::Find(
    std::string_view stableId) {
    return const_cast<EditorGimmickRuntimeInstance*>(
        static_cast<const EditorGimmickRuntimeWorld&>(*this)
            .Find(stableId));
}

const EditorGimmickRuntimeInstance*
EditorGimmickRuntimeWorld::Find(
    std::string_view stableId) const {
    const auto found = std::lower_bound(
        instances_.begin(),
        instances_.end(),
        stableId,
        [](const EditorGimmickRuntimeInstance& instance,
           std::string_view value) {
            return instance.stableId < value;
        });
    return found != instances_.end() &&
            found->stableId == stableId
        ? &*found
        : nullptr;
}

EditorGimmickRuntimeInstance*
EditorGimmickRuntimeWorld::FindByEntity(
    std::string_view entityGuid) {
    return const_cast<EditorGimmickRuntimeInstance*>(
        static_cast<const EditorGimmickRuntimeWorld&>(*this)
            .FindByEntity(entityGuid));
}

const EditorGimmickRuntimeInstance*
EditorGimmickRuntimeWorld::FindByEntity(
    std::string_view entityGuid) const {
    const auto found = std::find_if(
        instances_.begin(),
        instances_.end(),
        [&](const EditorGimmickRuntimeInstance& instance) {
            return instance.entityGuid == entityGuid;
        });
    return found == instances_.end() ? nullptr : &*found;
}

EditorSceneRuntimeFactoryResult
EditorGimmickRuntimeFactory::Instantiate(
    const EditorScene& scene,
    const std::vector<EditorSceneRuntimeComponentRecord>&
        components,
    const EditorSceneRuntimeServiceRegistry& services) {
    EditorSceneRuntimeFactoryResult result{};
    EditorGimmickRuntimeTarget* target =
        services.Find<EditorGimmickRuntimeTarget>(
            kEditorGimmickRuntimeTargetServiceId);
    if (target == nullptr || target->definitions == nullptr ||
        target->definitionFactories == nullptr ||
        target->world == nullptr) {
        result.message =
            "Gimmick Runtime Factory requires Definition Registry, "
            "Definition Factory Registry, and Runtime World services.";
        return result;
    }
    std::string validationError;
    if (!target->definitionFactories->
            ValidateAgainstDefinitions(
                *target->definitions,
                &validationError)) {
        result.message =
            "Gimmick Runtime Factory Registry is incomplete: " +
            validationError;
        return result;
    }

    std::vector<EditorGimmickRuntimeInstance> instances;
    instances.reserve(components.size());
    for (const EditorSceneRuntimeComponentRecord& record :
         components) {
        if (record.entity == nullptr ||
            record.component == nullptr) {
            result.message =
                "Gimmick Runtime Factory received an invalid "
                "Component record.";
            return result;
        }
        EditorGimmickComponent authored{};
        std::string componentError;
        if (!EditorGimmickComponent::FromSceneComponent(
                *record.component,
                authored,
                *target->definitions,
                &componentError,
                EditorGimmickValidationPolicy::Runtime)) {
            result.message =
                "Runtime Gimmick validation failed on Entity \"" +
                record.entity->name + "\": " + componentError;
            return result;
        }
        const EditorGimmickDefinition* definition =
            target->definitions->Find(authored.definitionId);
        const IEditorGimmickDefinitionRuntimeFactory*
            definitionFactory =
                definition != nullptr
                ? target->definitionFactories->Find(
                    definition->runtimeFactoryId)
                : nullptr;
        if (definition == nullptr ||
            definitionFactory == nullptr ||
            definitionFactory->DefinitionTypeId() !=
                definition->typeId) {
            result.message =
                "Runtime Gimmick Definition Factory cannot resolve "
                "Definition \"" +
                authored.definitionId + "\".";
            return result;
        }
        EditorGimmickRuntimeInstance instance{};
        if (!definitionFactory->Build(
                scene,
                record,
                *definition,
                authored,
                instance,
                &componentError)) {
            result.message =
                "Runtime Gimmick Definition Factory \"" +
                definition->runtimeFactoryId +
                "\" failed: " + componentError;
            return result;
        }
        instances.push_back(std::move(instance));
    }

    std::string worldError;
    if (!target->world->Replace(
            scene, std::move(instances), &worldError)) {
        result.message =
            "Gimmick Runtime World rejected Factory output: " +
            worldError;
        return result;
    }
    activeWorld_ = target->world;
    result.succeeded = true;
    result.applied = true;
    result.message =
        "Runtime Gimmick World instantiated " +
        std::to_string(activeWorld_->Instances().size()) +
        " Gimmicks through Definition-specific Factories.";
    return result;
}

void EditorGimmickRuntimeFactory::Destroy() noexcept {
    if (activeWorld_ != nullptr) {
        activeWorld_->SuspendForReconcile();
    }
    activeWorld_ = nullptr;
}

bool RegisterBuiltInEditorGimmickDefinitionRuntimeFactories(
    EditorGimmickDefinitionRuntimeFactoryRegistry& registry,
    const EditorGimmickDefinitionRegistry& definitions,
    std::string* errorMessage) {
    for (const EditorGimmickDefinition* definition :
         definitions.Ordered()) {
        if (definition == nullptr) continue;
        const IEditorGimmickDefinitionRuntimeFactory* existing =
            registry.Find(definition->runtimeFactoryId);
        if (existing != nullptr) {
            if (existing->DefinitionTypeId() !=
                definition->typeId) {
                SetError(
                    errorMessage,
                    "Existing Gimmick Runtime Factory mapping "
                    "conflicts with Definition \"" +
                        definition->typeId + "\".");
                return false;
            }
            continue;
        }
        if (!registry.Register(
                std::make_unique<
                    EditorBuiltInGimmickDefinitionRuntimeFactory>(
                    definition->typeId,
                    definition->runtimeFactoryId),
                errorMessage)) {
            return false;
        }
    }
    return registry.ValidateAgainstDefinitions(
        definitions, errorMessage);
}

} // namespace editor
