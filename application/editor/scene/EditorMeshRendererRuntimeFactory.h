#pragma once

#include "../EditorAssetRegistry.h"
#include "EditorSceneRuntimeInstantiation.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

inline constexpr std::string_view kEditorMeshRendererRuntimeTargetServiceId =
    "runtime.mesh-renderer.target";

struct EditorMeshRendererRuntimeInstance {
    std::string stableId;
    std::string entityGuid;
    std::string assetGuid;
    uint64_t sourceHash = 0;
};

class EditorMeshRendererRuntimeWorld {
public:
    bool Replace(
        const EditorScene& sourceScene,
        std::vector<EditorMeshRendererRuntimeInstance> instances,
        std::string* errorMessage = nullptr);
    void Clear() noexcept;

    bool Active() const noexcept { return active_; }
    uint64_t Revision() const noexcept { return revision_; }
    const EditorScene& Scene() const noexcept { return scene_; }
    const std::vector<EditorMeshRendererRuntimeInstance>& Instances() const noexcept {
        return instances_;
    }
    const EditorMeshRendererRuntimeInstance* Find(
        std::string_view stableId) const;

private:
    EditorScene scene_{};
    std::vector<EditorMeshRendererRuntimeInstance> instances_;
    bool active_ = false;
    uint64_t revision_ = 0;
};

struct EditorMeshRendererRuntimeTarget {
    const EditorAssetRegistry* assets = nullptr;
    EditorMeshRendererRuntimeWorld* world = nullptr;
};

class EditorMeshRendererRuntimeFactory final
    : public IEditorSceneRuntimeComponentFactory {
public:
    std::string_view TypeId() const noexcept override {
        return kEditorMeshRendererComponentType;
    }
    int32_t Priority() const noexcept override { return 20; }

    EditorSceneRuntimeFactoryResult Instantiate(
        const EditorScene& scene,
        const std::vector<EditorSceneRuntimeComponentRecord>& components,
        const EditorSceneRuntimeServiceRegistry& services) override;
    void Destroy() noexcept override;

private:
    EditorMeshRendererRuntimeWorld* activeWorld_ = nullptr;
};

} // namespace editor
