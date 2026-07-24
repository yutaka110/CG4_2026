#pragma once

#include "EditorBlenderSceneImportService.h"
#include "EditorGameplaySpawnRuntimeService.h"
#include "EditorSceneRuntimeInstantiation.h"

#include <string_view>

namespace editor {

inline constexpr std::string_view kEditorGameplaySpawnRuntimeTargetServiceId =
    "runtime.gameplay-spawn.target";

class EditorGameplaySpawnRuntimeFactory final
    : public IEditorSceneRuntimeComponentFactory {
public:
    std::string_view TypeId() const noexcept override {
        return kEditorGameplaySpawnPointComponentType;
    }
    int32_t Priority() const noexcept override { return 100; }

    EditorSceneRuntimeFactoryResult Instantiate(
        const EditorScene& scene,
        const std::vector<EditorSceneRuntimeComponentRecord>& components,
        const EditorSceneRuntimeServiceRegistry& services) override;
    void Destroy() noexcept override;

    bool Active() const noexcept { return service_.Active(); }
    const EditorGameplaySpawnRuntimeService& Service() const noexcept {
        return service_;
    }

private:
    EditorGameplaySpawnRuntimeService service_{};
    EditorGameplaySpawnRuntimeTarget activeTarget_{};
};

} // namespace editor
