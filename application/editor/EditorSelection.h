#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

enum class EditorDomainId : uint32_t {
    Unknown = 0,
    Asset,
    VfxEffectAsset,
    VfxEffectInstance,
    CourseTerrainPlacement,
    CourseRockCluster,
    CourseCameraKey,
    CourseEventMarker,
    CourseTerrainMaterialPreset,
    TerrainGeneration,
    PostProcessPass,
    RenderPreset,
    CameraRig,
    GameplayTuning,
    RenderGraphPass,
    SceneEntity,
    SequencerKey,
    MaterialGraphNode,
    VfxGraphNode,
    AnimationStateMachineNode,
    GameplayVisualScriptNode,
    AiAuthoringNode,
    NavigationAuthoring,
    CourseEnemyPlacement,
    CourseRailControlPoint,
    CourseRailSegment,
    CourseWaveDefinition,
};

struct EditorObjectHandle {
    EditorDomainId domain = EditorDomainId::Unknown;
    std::string stableId;
    uint64_t localIndex = 0;
    uint32_t generation = 0;
    std::string displayName;

    bool SameObject(const EditorObjectHandle& other) const;
};

class EditorSelection {
public:
    void Clear();
    void SetPrimary(EditorObjectHandle handle);
    void Add(EditorObjectHandle handle);
    void Remove(const EditorObjectHandle& handle);
    void Toggle(EditorObjectHandle handle);
    void Set(std::vector<EditorObjectHandle> handles);

    bool Empty() const { return handles_.empty(); }
    std::size_t Count() const { return handles_.size(); }
    const EditorObjectHandle* Primary() const;
    const std::vector<EditorObjectHandle>& Handles() const { return handles_; }
    bool Contains(const EditorObjectHandle& handle) const;

    uint32_t Revision() const { return revision_; }

private:
    void Touch();

    std::vector<EditorObjectHandle> handles_;
    uint32_t revision_ = 0;
};

const char* ToString(EditorDomainId domain);
std::string BuildStableIndexedId(std::string_view prefix, uint64_t index);

} // namespace editor
