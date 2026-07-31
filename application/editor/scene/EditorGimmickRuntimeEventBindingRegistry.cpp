#include "EditorGimmickRuntimeEventBindingRegistry.h"

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace editor {
namespace {

void SetError(
    std::string* errorMessage,
    std::string message) {
    if (errorMessage != nullptr) {
        *errorMessage = std::move(message);
    }
}

bool RuntimeBindingLess(
    const EditorGimmickRuntimeEventBinding& left,
    const EditorGimmickRuntimeEventBinding& right) noexcept {
    if (left.sourceEntityGuid != right.sourceEntityGuid) {
        return left.sourceEntityGuid < right.sourceEntityGuid;
    }
    if (left.sourceEvent != right.sourceEvent) {
        return static_cast<uint32_t>(left.sourceEvent) <
            static_cast<uint32_t>(right.sourceEvent);
    }
    if (left.priority != right.priority) {
        return left.priority < right.priority;
    }
    return left.stableId < right.stableId;
}

} // namespace

bool EditorGimmickRuntimeEventBindingRegistry::Replace(
    std::vector<EditorGimmickRuntimeEventBinding> bindings,
    const EditorGimmickRuntimeWorld& world,
    std::string* errorMessage) {
    std::unordered_set<std::string> stableIds;
    stableIds.reserve(bindings.size());
    for (EditorGimmickRuntimeEventBinding& binding : bindings) {
        if (binding.stableId.empty() ||
            binding.bindingId.empty() ||
            binding.sourceEntityGuid.empty() ||
            binding.targetEntityGuid.empty() ||
            !stableIds.insert(binding.stableId).second) {
            SetError(
                errorMessage,
                "Runtime Gimmick Event Bindings require unique "
                "stable IDs and valid source/target identities.");
            return false;
        }
        const auto& previousBindings =
            !suspendedBindings_.empty()
            ? suspendedBindings_
            : bindings_;
        const auto previous = std::find_if(
            previousBindings.begin(),
            previousBindings.end(),
            [&](const EditorGimmickRuntimeEventBinding& candidate) {
                return candidate.stableId == binding.stableId &&
                    candidate.sourceHash == binding.sourceHash;
            });
        if (previous != previousBindings.end()) {
            binding.consumed = previous->consumed;
        }
        binding.resolved =
            world.FindByEntity(binding.targetEntityGuid) != nullptr;
    }
    std::sort(
        bindings.begin(),
        bindings.end(),
        RuntimeBindingLess);
    bindings_ = std::move(bindings);
    suspendedBindings_.clear();
    active_ = true;
    ++revision_;
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

void EditorGimmickRuntimeEventBindingRegistry::
SuspendForReconcile() noexcept {
    if (!active_ && bindings_.empty()) return;
    suspendedBindings_ = std::move(bindings_);
    bindings_.clear();
    active_ = false;
    ++revision_;
}

void EditorGimmickRuntimeEventBindingRegistry::
FinalizeReconcile() noexcept {
    if (active_ || suspendedBindings_.empty()) return;
    suspendedBindings_.clear();
    ++revision_;
}

void EditorGimmickRuntimeEventBindingRegistry::Reconcile(
    const EditorGimmickRuntimeWorld& world) noexcept {
    if (!active_) return;
    bool changed = false;
    for (EditorGimmickRuntimeEventBinding& binding : bindings_) {
        const bool resolved =
            world.FindByEntity(binding.targetEntityGuid) != nullptr;
        if (binding.resolved != resolved) {
            binding.resolved = resolved;
            changed = true;
        }
    }
    if (changed) ++revision_;
}

void EditorGimmickRuntimeEventBindingRegistry::Clear() noexcept {
    if (!active_ && bindings_.empty() &&
        suspendedBindings_.empty()) {
        return;
    }
    bindings_.clear();
    suspendedBindings_.clear();
    active_ = false;
    ++revision_;
}

std::vector<const EditorGimmickRuntimeEventBinding*>
EditorGimmickRuntimeEventBindingRegistry::FindBindings(
    std::string_view sourceEntityGuid,
    EditorGimmickRuntimeEventKind sourceEvent) const {
    std::vector<const EditorGimmickRuntimeEventBinding*> found;
    if (!active_) return found;
    for (const EditorGimmickRuntimeEventBinding& binding :
         bindings_) {
        if (binding.sourceEntityGuid == sourceEntityGuid &&
            binding.sourceEvent == sourceEvent &&
            binding.enabled &&
            (!binding.oneShot || !binding.consumed)) {
            found.push_back(&binding);
        }
    }
    return found;
}

const EditorGimmickRuntimeEventBinding*
EditorGimmickRuntimeEventBindingRegistry::Find(
    std::string_view stableId) const noexcept {
    const auto found = std::find_if(
        bindings_.begin(),
        bindings_.end(),
        [&](const EditorGimmickRuntimeEventBinding& binding) {
            return binding.stableId == stableId;
        });
    return found == bindings_.end() ? nullptr : &*found;
}

bool EditorGimmickRuntimeEventBindingRegistry::MarkConsumed(
    std::string_view stableId) noexcept {
    const auto found = std::find_if(
        bindings_.begin(),
        bindings_.end(),
        [&](const EditorGimmickRuntimeEventBinding& binding) {
            return binding.stableId == stableId;
        });
    if (found == bindings_.end() ||
        !found->oneShot ||
        found->consumed) {
        return false;
    }
    found->consumed = true;
    ++revision_;
    return true;
}

uint64_t
EditorGimmickRuntimeEventBindingRegistry::ConsumedCount()
    const noexcept {
    return static_cast<uint64_t>(std::count_if(
        bindings_.begin(),
        bindings_.end(),
        [](const EditorGimmickRuntimeEventBinding& binding) {
            return binding.consumed;
        }));
}

uint64_t
EditorGimmickRuntimeEventBindingRegistry::UnresolvedCount()
    const noexcept {
    return static_cast<uint64_t>(std::count_if(
        bindings_.begin(),
        bindings_.end(),
        [](const EditorGimmickRuntimeEventBinding& binding) {
            return !binding.resolved;
        }));
}

EditorSceneRuntimeFactoryResult
EditorGimmickEventBindingRuntimeFactory::Instantiate(
    const EditorScene&,
    const std::vector<EditorSceneRuntimeComponentRecord>&
        components,
    const EditorSceneRuntimeServiceRegistry& services) {
    EditorSceneRuntimeFactoryResult result{};
    EditorGimmickEventBindingRuntimeTarget* target =
        services.Find<EditorGimmickEventBindingRuntimeTarget>(
            kEditorGimmickEventBindingRuntimeTargetServiceId);
    if (target == nullptr ||
        target->registry == nullptr ||
        target->world == nullptr ||
        !target->world->Active()) {
        result.message =
            "Gimmick Event Binding Runtime Factory requires an "
            "active Gimmick World and Binding Registry.";
        return result;
    }

    std::vector<EditorGimmickRuntimeEventBinding> bindings;
    for (const EditorSceneRuntimeComponentRecord& record :
         components) {
        if (record.entity == nullptr ||
            record.component == nullptr) {
            result.message =
                "Gimmick Event Binding Runtime Factory received "
                "an invalid Component record.";
            return result;
        }
        EditorGimmickEventBindingComponent authored{};
        std::string parseError;
        if (!EditorGimmickEventBindingComponent::
                FromSceneComponent(
                    *record.component,
                    authored,
                    &parseError)) {
            result.message =
                "Runtime Gimmick Event Binding validation failed "
                "on Entity \"" +
                record.entity->name + "\": " + parseError;
            return result;
        }
        for (const EditorGimmickEventBinding& binding :
             authored.bindings) {
            EditorGimmickRuntimeEventBinding runtime{};
            runtime.stableId =
                record.stableId + "/binding/" + binding.id;
            runtime.bindingId = binding.id;
            runtime.sourceEntityGuid = record.entity->guid;
            runtime.sourceEvent = binding.sourceEvent;
            runtime.targetEntityGuid =
                binding.targetEntityGuid;
            runtime.targetCommand = binding.targetCommand;
            runtime.payload = binding.payload;
            runtime.sourceHash = record.sourceHash;
            runtime.priority = binding.priority;
            runtime.enabled = binding.enabled;
            runtime.oneShot = binding.oneShot;
            runtime.delaySeconds = binding.delaySeconds;
            runtime.repeatIntervalSeconds =
                binding.repeatIntervalSeconds;
            runtime.repeatCount = binding.repeatCount;
            runtime.resolved =
                target->world->FindByEntity(
                    runtime.targetEntityGuid) != nullptr;
            if (!runtime.resolved) {
                result.warnings.push_back(
                    "Gimmick Event Binding \"" +
                    binding.id +
                    "\" has an unresolved Runtime target.");
            }
            bindings.push_back(std::move(runtime));
        }
    }
    std::string registryError;
    if (!target->registry->Replace(
            std::move(bindings),
            *target->world,
            &registryError)) {
        result.message =
            "Gimmick Event Binding Registry rejected Factory "
            "output: " +
            registryError;
        return result;
    }
    activeRegistry_ = target->registry;
    result.succeeded = true;
    result.applied = true;
    result.message =
        "Runtime Gimmick Event Binding Registry instantiated " +
        std::to_string(activeRegistry_->Bindings().size()) +
        " deterministic Bindings.";
    return result;
}

void EditorGimmickEventBindingRuntimeFactory::Destroy() noexcept {
    if (activeRegistry_ != nullptr) {
        activeRegistry_->SuspendForReconcile();
    }
    activeRegistry_ = nullptr;
}

} // namespace editor
