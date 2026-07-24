#include "EditorBuiltInRuntimeFactoryRegistration.h"

#include "EditorGameplaySpawnRuntimeFactory.h"
#include "EditorGimmickRuntimeFactory.h"
#include "EditorGimmickRuntimeEventBindingRegistry.h"
#include "EditorGimmickRuntimeEventSequenceRegistry.h"
#include "EditorMeshRendererRuntimeFactory.h"
#include "EditorPatrolRuntimeFactory.h"

#include <memory>
#include <utility>

namespace editor {
namespace {

template <typename Factory>
bool RegisterIfMissing(
    EditorSceneRuntimeComponentFactoryRegistry& registry,
    std::string* errorMessage) {
    const Factory probe;
    if (registry.Find(probe.TypeId()) != nullptr) return true;
    return registry.Register(
        std::make_unique<Factory>(), errorMessage);
}

} // namespace

bool RegisterBuiltInEditorSceneRuntimeFactories(
    EditorSceneRuntimeComponentFactoryRegistry& registry,
    std::string* errorMessage) {
    if (!RegisterIfMissing<EditorMeshRendererRuntimeFactory>(
            registry, errorMessage) ||
        !RegisterIfMissing<EditorGameplaySpawnRuntimeFactory>(
            registry, errorMessage) ||
        !RegisterIfMissing<EditorSplineRouteRuntimeFactory>(
            registry, errorMessage) ||
        !RegisterIfMissing<EditorPatrolRuntimeFactory>(
            registry, errorMessage) ||
        !RegisterIfMissing<EditorGimmickRuntimeFactory>(
            registry, errorMessage) ||
        !RegisterIfMissing<
            EditorGimmickEventBindingRuntimeFactory>(
                registry, errorMessage) ||
        !RegisterIfMissing<
            EditorGimmickEventSequenceRuntimeFactory>(
                registry, errorMessage)) {
        return false;
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

} // namespace editor
