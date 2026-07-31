#include "EditorSceneRuntimeInstantiation.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace editor {
namespace {

constexpr uint64_t kFnvOffset = 1469598103934665603ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;

void HashBytes(uint64_t& hash, std::string_view value) noexcept {
    for (const unsigned char byte : value) {
        hash ^= static_cast<uint64_t>(byte);
        hash *= kFnvPrime;
    }
    hash ^= 0xffu;
    hash *= kFnvPrime;
}

EditorSceneRuntimeInstantiationResult Failure(std::string message) {
    EditorSceneRuntimeInstantiationResult result{};
    result.message = std::move(message);
    return result;
}

using RuntimeRecordsByType = std::unordered_map<
    std::string,
    std::vector<EditorSceneRuntimeComponentRecord>>;

struct RuntimeScenePlan {
    RuntimeRecordsByType recordsByType;
    std::vector<EditorSceneRuntimeObjectSource> sources;
    std::vector<std::string> warnings;
};

bool BuildRuntimeScenePlan(
    const EditorScene& scene,
    const EditorSceneComponentRegistry& components,
    EditorSceneRuntimeComponentFactoryRegistry& factories,
    RuntimeScenePlan& outPlan,
    std::string& outError) {
    outPlan = {};
    const EditorSceneValidationReport structuralValidation =
        scene.Validate(&components);
    if (!structuralValidation.Succeeded()) {
        outError =
            "Runtime Scene validation failed: " +
            structuralValidation.errors.front();
        return false;
    }
    outPlan.warnings = structuralValidation.warnings;

    EditorSceneValidationReport registryValidation{};
    const std::vector<bool> runtimeActivation =
        scene.EvaluateRuntimeActivation();
    for (std::size_t entityIndex = 0;
         entityIndex < scene.entities.size();
         ++entityIndex) {
        const EditorSceneEntity& entity =
            scene.entities[entityIndex];
        const bool runtimeActive =
            entityIndex < runtimeActivation.size() &&
            runtimeActivation[entityIndex];
        for (const EditorSceneComponent& component : entity.components) {
            components.ValidateComponent(
                component, registryValidation, entity.guid);
            if (!runtimeActive || !component.enabled) continue;

            const EditorSceneComponentDescriptor* descriptor =
                components.Find(component.typeId);
            IEditorSceneRuntimeComponentFactory* factory =
                factories.Find(component.typeId);
            if (factory == nullptr) {
                if (descriptor != nullptr &&
                    descriptor->runtimePolicy ==
                        EditorSceneRuntimeInstantiationPolicy::Required) {
                    outError =
                        "Required Runtime Scene Component Factory is missing: " +
                        component.typeId;
                    return false;
                }
                if (descriptor != nullptr &&
                    descriptor->runtimePolicy ==
                        EditorSceneRuntimeInstantiationPolicy::Optional) {
                    outPlan.warnings.push_back(
                        "Optional Runtime Scene Component has no Factory: " +
                        component.typeId);
                }
                continue;
            }

            EditorSceneRuntimeComponentRecord record{};
            record.stableId = entity.guid + ":" + component.typeId;
            record.entity = &entity;
            record.component = &component;
            record.sourceHash =
                HashEditorSceneRuntimeComponent(entity, component);
            outPlan.sources.push_back(
                EditorSceneRuntimeObjectSource{
                    record.stableId,
                    entity.guid,
                    component.typeId,
                    record.sourceHash});
            outPlan.recordsByType[component.typeId].push_back(
                std::move(record));
        }
    }
    if (!registryValidation.Succeeded()) {
        outError =
            "Runtime Scene Component validation failed: " +
            registryValidation.errors.front();
        return false;
    }
    outPlan.warnings.insert(
        outPlan.warnings.end(),
        registryValidation.warnings.begin(),
        registryValidation.warnings.end());

    std::sort(
        outPlan.sources.begin(),
        outPlan.sources.end(),
        [](const auto& lhs, const auto& rhs) {
            return lhs.stableId < rhs.stableId;
        });
    for (auto& [typeId, records] : outPlan.recordsByType) {
        std::sort(
            records.begin(),
            records.end(),
            [](const auto& lhs, const auto& rhs) {
                return lhs.stableId < rhs.stableId;
            });
    }
    outError.clear();
    return true;
}

std::vector<IEditorSceneRuntimeComponentFactory*> OrderedFactories(
    EditorSceneRuntimeComponentFactoryRegistry& factories,
    const std::unordered_set<std::string>& typeIds) {
    std::vector<IEditorSceneRuntimeComponentFactory*> ordered;
    ordered.reserve(typeIds.size());
    for (const std::string& typeId : typeIds) {
        if (IEditorSceneRuntimeComponentFactory* factory =
                factories.Find(typeId)) {
            ordered.push_back(factory);
        }
    }
    std::sort(
        ordered.begin(),
        ordered.end(),
        [](const auto* lhs, const auto* rhs) {
            if (lhs->Priority() != rhs->Priority()) {
                return lhs->Priority() < rhs->Priority();
            }
            return lhs->TypeId() < rhs->TypeId();
        });
    return ordered;
}

std::unordered_set<std::string> TypeIdsForPlan(
    const RuntimeScenePlan& plan) {
    std::unordered_set<std::string> typeIds;
    typeIds.reserve(plan.recordsByType.size());
    for (const auto& [typeId, records] : plan.recordsByType) {
        if (!records.empty()) typeIds.insert(typeId);
    }
    return typeIds;
}

const std::vector<EditorSceneRuntimeComponentRecord>& RecordsFor(
    const RuntimeScenePlan& plan,
    std::string_view typeId) {
    static const std::vector<EditorSceneRuntimeComponentRecord> empty;
    const auto found = plan.recordsByType.find(std::string(typeId));
    return found == plan.recordsByType.end() ? empty : found->second;
}

std::string DeltaTypeId(const EditorSceneRuntimeObjectDelta& delta) {
    return delta.kind == EditorSceneRuntimeObjectDeltaKind::Removed
        ? delta.previous.source.componentTypeId
        : delta.desired.componentTypeId;
}

} // namespace

EditorSceneRuntimeComponentFactoryRegistry::
    ~EditorSceneRuntimeComponentFactoryRegistry() {
    Clear();
}

bool EditorSceneRuntimeComponentFactoryRegistry::Register(
    std::unique_ptr<IEditorSceneRuntimeComponentFactory> factory,
    std::string* errorMessage) {
    const auto fail = [&](std::string message) {
        if (errorMessage != nullptr) *errorMessage = std::move(message);
        return false;
    };
    if (factory == nullptr || factory->TypeId().empty()) {
        return fail("Runtime Scene Component Factory requires a Type ID.");
    }
    const std::string typeId(factory->TypeId());
    if (Find(typeId) != nullptr) {
        return fail("Runtime Scene Component Factory is already registered: " + typeId);
    }
    factories_.push_back(std::move(factory));
    return true;
}

bool EditorSceneRuntimeComponentFactoryRegistry::Remove(std::string_view typeId) {
    const auto found = std::find_if(
        factories_.begin(),
        factories_.end(),
        [&](const auto& factory) {
            return factory != nullptr && factory->TypeId() == typeId;
        });
    if (found == factories_.end()) return false;
    (*found)->Destroy();
    factories_.erase(found);
    return true;
}

void EditorSceneRuntimeComponentFactoryRegistry::Clear() {
    for (auto iterator = factories_.rbegin(); iterator != factories_.rend(); ++iterator) {
        if (*iterator != nullptr) (*iterator)->Destroy();
    }
    factories_.clear();
}

IEditorSceneRuntimeComponentFactory*
EditorSceneRuntimeComponentFactoryRegistry::Find(std::string_view typeId) {
    return const_cast<IEditorSceneRuntimeComponentFactory*>(
        static_cast<const EditorSceneRuntimeComponentFactoryRegistry&>(*this).Find(typeId));
}

const IEditorSceneRuntimeComponentFactory*
EditorSceneRuntimeComponentFactoryRegistry::Find(std::string_view typeId) const {
    const auto found = std::find_if(
        factories_.begin(),
        factories_.end(),
        [&](const auto& factory) {
            return factory != nullptr && factory->TypeId() == typeId;
        });
    return found == factories_.end() ? nullptr : found->get();
}

EditorSceneRuntimeInstantiationService::
    ~EditorSceneRuntimeInstantiationService() {
    Stop();
}

bool EditorSceneRuntimeInstantiationService::Bind(
    const EditorSceneComponentRegistry* components,
    EditorSceneRuntimeComponentFactoryRegistry* factories,
    std::string* errorMessage) {
    const auto fail = [&](std::string message) {
        if (errorMessage != nullptr) *errorMessage = std::move(message);
        return false;
    };
    if (active_) {
        return fail(
            "Cannot rebind Runtime Scene Instantiation while a session is active.");
    }
    if (components == nullptr || factories == nullptr) {
        return fail(
            "Runtime Scene Instantiation requires Component and Factory registries.");
    }
    components_ = components;
    factories_ = factories;
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

EditorSceneRuntimeInstantiationResult
EditorSceneRuntimeInstantiationService::Begin(
    const EditorScene& scene,
    const EditorSceneRuntimeServiceRegistry& services) {
    if (active_) {
        return Failure("Runtime Scene Instantiation session is already active.");
    }
    if (components_ == nullptr || factories_ == nullptr) {
        return Failure("Runtime Scene Instantiation registries are not bound.");
    }

    RuntimeScenePlan plan;
    std::string planError;
    if (!BuildRuntimeScenePlan(
            scene, *components_, *factories_, plan, planError)) {
        return Failure(std::move(planError));
    }

    std::vector<IEditorSceneRuntimeComponentFactory*> appliedFactories;
    const std::unordered_set<std::string> plannedTypes = TypeIdsForPlan(plan);
    const auto orderedFactories =
        OrderedFactories(*factories_, plannedTypes);
    for (IEditorSceneRuntimeComponentFactory* factory : orderedFactories) {
        EditorSceneRuntimeFactoryResult factoryResult =
            factory->Instantiate(
                scene, RecordsFor(plan, factory->TypeId()), services);
        plan.warnings.insert(
            plan.warnings.end(),
            factoryResult.warnings.begin(),
            factoryResult.warnings.end());
        if (!factoryResult.succeeded) {
            factory->Destroy();
            for (auto iterator = appliedFactories.rbegin();
                 iterator != appliedFactories.rend();
                 ++iterator) {
                (*iterator)->Destroy();
            }
            return Failure(
                factoryResult.message.empty()
                    ? "Runtime Scene Component Factory failed: " +
                        std::string(factory->TypeId())
                    : factoryResult.message);
        }
        if (factoryResult.applied) appliedFactories.push_back(factory);
    }

    std::string registryError;
    if (!runtimeObjects_.Synchronize(plan.sources, &registryError)) {
        for (auto iterator = appliedFactories.rbegin();
             iterator != appliedFactories.rend();
             ++iterator) {
            (*iterator)->Destroy();
        }
        return Failure(
            "Runtime Scene Object Registry rejected the initial Scene: " +
            registryError);
    }
    activeFactoryTypes_.clear();
    activeFactoryTypes_.reserve(appliedFactories.size());
    for (const auto* factory : appliedFactories) {
        activeFactoryTypes_.push_back(std::string(factory->TypeId()));
    }
    sourceRevision_ = scene.revision;
    activeSceneSnapshot_ = scene;
    active_ = true;

    EditorSceneRuntimeInstantiationResult result{};
    result.succeeded = true;
    result.applied = !appliedFactories.empty();
    result.componentCount = plan.sources.size();
    result.factoryCount = appliedFactories.size();
    result.addedCount = plan.sources.size();
    result.sourceRevision = sourceRevision_;
    result.warnings = std::move(plan.warnings);
    result.message = result.applied
        ? "Runtime Scene instantiated " +
            std::to_string(result.componentCount) + " Components through " +
            std::to_string(result.factoryCount) + " Factories."
        : "Runtime Scene contains no enabled Components owned by registered Factories.";
    return result;
}

EditorSceneRuntimeInstantiationResult
EditorSceneRuntimeInstantiationService::Reconcile(
    const EditorScene& scene,
    const EditorSceneRuntimeServiceRegistry& services) {
    if (!active_) {
        return Failure(
            "Runtime Scene Reconcile requires an active Instantiation session.");
    }
    if (components_ == nullptr || factories_ == nullptr) {
        return Failure("Runtime Scene Instantiation registries are not bound.");
    }

    RuntimeScenePlan desiredPlan;
    RuntimeScenePlan previousPlan;
    std::string planError;
    if (!BuildRuntimeScenePlan(
            scene, *components_, *factories_, desiredPlan, planError)) {
        return Failure(std::move(planError));
    }
    if (!BuildRuntimeScenePlan(
            activeSceneSnapshot_,
            *components_,
            *factories_,
            previousPlan,
            planError)) {
        return Failure(
            "Runtime Scene Reconcile could not rebuild its previous state: " +
            planError);
    }

    std::vector<EditorSceneRuntimeObjectDelta> deltas;
    std::string registryError;
    if (!runtimeObjects_.Diff(
            desiredPlan.sources, deltas, &registryError)) {
        return Failure(
            "Runtime Scene Object Registry rejected Reconcile input: " +
            registryError);
    }

    EditorSceneRuntimeInstantiationResult result{};
    result.succeeded = true;
    result.componentCount = desiredPlan.sources.size();
    result.sourceRevision = scene.revision;
    result.warnings = desiredPlan.warnings;
    for (const EditorSceneRuntimeObjectDelta& delta : deltas) {
        switch (delta.kind) {
        case EditorSceneRuntimeObjectDeltaKind::Added:
            ++result.addedCount;
            break;
        case EditorSceneRuntimeObjectDeltaKind::Modified:
            ++result.modifiedCount;
            break;
        case EditorSceneRuntimeObjectDeltaKind::Removed:
            ++result.removedCount;
            break;
        }
    }

    if (deltas.empty()) {
        sourceRevision_ = scene.revision;
        activeSceneSnapshot_ = scene;
        result.message =
            "Runtime Scene Reconcile found no Runtime Component changes.";
        return result;
    }

    std::unordered_set<std::string> changedTypes;
    changedTypes.reserve(deltas.size());
    for (const EditorSceneRuntimeObjectDelta& delta : deltas) {
        changedTypes.insert(DeltaTypeId(delta));
    }
    for (const std::string& typeId : changedTypes) {
        if (factories_->Find(typeId) == nullptr) {
            return Failure(
                "Runtime Scene Reconcile Factory is missing: " + typeId);
        }
    }

    const auto changedFactories =
        OrderedFactories(*factories_, changedTypes);
    std::vector<IEditorSceneRuntimeComponentFactory*> processedFactories;
    std::unordered_map<std::string, bool> newlyApplied;
    bool failed = false;
    std::string failureMessage;
    for (IEditorSceneRuntimeComponentFactory* factory : changedFactories) {
        factory->Destroy();
        processedFactories.push_back(factory);
        const auto& desiredRecords =
            RecordsFor(desiredPlan, factory->TypeId());
        if (desiredRecords.empty()) {
            newlyApplied.emplace(std::string(factory->TypeId()), false);
            continue;
        }

        EditorSceneRuntimeFactoryResult factoryResult =
            factory->Instantiate(scene, desiredRecords, services);
        result.warnings.insert(
            result.warnings.end(),
            factoryResult.warnings.begin(),
            factoryResult.warnings.end());
        if (!factoryResult.succeeded) {
            failed = true;
            failureMessage = factoryResult.message.empty()
                ? "Runtime Scene Reconcile Factory failed: " +
                    std::string(factory->TypeId())
                : factoryResult.message;
            break;
        }
        newlyApplied.emplace(
            std::string(factory->TypeId()), factoryResult.applied);
    }

    if (failed) {
        bool rollbackSucceeded = true;
        std::vector<std::string> rollbackWarnings;
        for (auto iterator = processedFactories.rbegin();
             iterator != processedFactories.rend();
             ++iterator) {
            IEditorSceneRuntimeComponentFactory* factory = *iterator;
            factory->Destroy();
            const auto& previousRecords =
                RecordsFor(previousPlan, factory->TypeId());
            if (previousRecords.empty()) continue;
            EditorSceneRuntimeFactoryResult restored =
                factory->Instantiate(
                    activeSceneSnapshot_,
                    previousRecords,
                    services);
            rollbackWarnings.insert(
                rollbackWarnings.end(),
                restored.warnings.begin(),
                restored.warnings.end());
            rollbackSucceeded =
                rollbackSucceeded && restored.succeeded;
        }
        result.warnings.insert(
            result.warnings.end(),
            rollbackWarnings.begin(),
            rollbackWarnings.end());

        if (rollbackSucceeded) {
            result.succeeded = false;
            result.message =
                failureMessage +
                " Previous Runtime Scene state was restored.";
            result.sourceRevision = sourceRevision_;
            return result;
        }

        std::unordered_set<std::string> cleanupTypes(
            activeFactoryTypes_.begin(), activeFactoryTypes_.end());
        cleanupTypes.insert(changedTypes.begin(), changedTypes.end());
        auto cleanupFactories =
            OrderedFactories(*factories_, cleanupTypes);
        for (auto iterator = cleanupFactories.rbegin();
             iterator != cleanupFactories.rend();
             ++iterator) {
            (*iterator)->Destroy();
        }
        activeFactoryTypes_.clear();
        runtimeObjects_.Clear();
        activeSceneSnapshot_ = {};
        sourceRevision_ = 0;
        active_ = false;
        result.succeeded = false;
        result.message =
            failureMessage +
            " Rollback also failed; the Runtime Scene session was stopped.";
        result.sourceRevision = 0;
        return result;
    }

    if (!runtimeObjects_.Synchronize(
            desiredPlan.sources, &registryError)) {
        return Failure(
            "Runtime Scene Object Registry failed to commit Reconcile: " +
            registryError);
    }

    std::unordered_set<std::string> activeTypes(
        activeFactoryTypes_.begin(), activeFactoryTypes_.end());
    for (const std::string& typeId : changedTypes) {
        activeTypes.erase(typeId);
        const auto found = newlyApplied.find(typeId);
        if (found != newlyApplied.end() && found->second) {
            activeTypes.insert(typeId);
        }
    }
    activeFactoryTypes_.clear();
    for (IEditorSceneRuntimeComponentFactory* factory :
         OrderedFactories(*factories_, activeTypes)) {
        activeFactoryTypes_.push_back(std::string(factory->TypeId()));
    }

    sourceRevision_ = scene.revision;
    activeSceneSnapshot_ = scene;
    result.applied = true;
    result.factoryCount = changedFactories.size();
    result.message =
        "Runtime Scene reconciled added=" +
        std::to_string(result.addedCount) +
        " modified=" + std::to_string(result.modifiedCount) +
        " removed=" + std::to_string(result.removedCount) +
        " through " + std::to_string(result.factoryCount) +
        " Factories.";
    return result;
}

void EditorSceneRuntimeInstantiationService::Stop() noexcept {
    if (!active_) return;
    if (factories_ != nullptr) {
        for (auto iterator = activeFactoryTypes_.rbegin();
             iterator != activeFactoryTypes_.rend();
             ++iterator) {
            if (IEditorSceneRuntimeComponentFactory* factory =
                    factories_->Find(*iterator)) {
                factory->Destroy();
            }
        }
    }
    activeFactoryTypes_.clear();
    runtimeObjects_.Clear();
    activeSceneSnapshot_ = {};
    sourceRevision_ = 0;
    active_ = false;
}

uint64_t HashEditorSceneRuntimeComponent(
    const EditorSceneEntity& entity,
    const EditorSceneComponent& component) noexcept {
    uint64_t hash = kFnvOffset;
    HashBytes(hash, entity.guid);
    HashBytes(hash, entity.parentGuid);
    hash ^= entity.runtimeEnabled ? 1ull : 0ull;
    hash *= kFnvPrime;
    HashBytes(hash, component.typeId);
    hash ^= component.enabled ? 1ull : 0ull;
    hash *= kFnvPrime;
    for (const EditorSceneProperty& property : component.properties) {
        HashBytes(hash, property.name);
        HashBytes(hash, property.value);
    }
    for (const EditorSceneObjectReference& reference : component.references) {
        HashBytes(hash, reference.property);
        HashBytes(hash, reference.entityGuid);
        HashBytes(hash, reference.assetGuid);
    }
    return hash;
}

} // namespace editor
