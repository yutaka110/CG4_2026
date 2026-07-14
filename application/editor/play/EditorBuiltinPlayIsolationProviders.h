#pragma once

#include "IEditorPlayIsolationProvider.h"
#include "../../EffectRuntime.h"
#include "../../PostProcessStack.h"

struct CourseAsset;
struct TerrainAuthoringState;

namespace editor {

inline constexpr const char* kCoursePlayIsolationProviderId = "editor.playIsolation.course";
inline constexpr const char* kTerrainPlayIsolationProviderId = "editor.playIsolation.terrain";
inline constexpr const char* kVfxPlayIsolationProviderId = "editor.playIsolation.vfx";
inline constexpr const char* kPostProcessPlayIsolationProviderId = "editor.playIsolation.postProcess";

using EditorVfxAuthoringSnapshot = std::unordered_map<std::string, EffectAsset>;
using EditorPostProcessAuthoringSnapshot = std::vector<PostProcessPass>;

uint64_t EditorCourseAuthoringFingerprint(const CourseAsset& course);
uint64_t EditorTerrainAuthoringFingerprint(const TerrainAuthoringState& terrain);
uint64_t EditorVfxAuthoringFingerprint(const EffectRuntime& runtime);
uint64_t EditorPostProcessAuthoringFingerprint(const PostProcessStack& stack);

class EditorCoursePlayIsolationProvider final : public IEditorPlayIsolationProvider {
public:
    void Bind(CourseAsset* course) noexcept { course_ = course; }

    std::string_view Id() const noexcept override { return kCoursePlayIsolationProviderId; }
    std::string_view Label() const noexcept override { return "Course"; }
    int Order() const noexcept override { return 100; }
    bool Available() const noexcept override { return course_ != nullptr; }
    bool Capture(EditorPlaySnapshot& snapshot, EditorError* error) const override;
    bool Restore(const EditorPlaySnapshot& snapshot, EditorError* error) const override;
    bool BuildRuntimeChangeSet(
        const EditorPlaySnapshot& snapshot,
        EditorRuntimeChangeSet& changes,
        EditorError* error) const override;
    uint64_t AuthoringFingerprint() const override;

private:
    CourseAsset* course_ = nullptr;
};

class EditorTerrainPlayIsolationProvider final : public IEditorPlayIsolationProvider {
public:
    void Bind(TerrainAuthoringState* terrain) noexcept { terrain_ = terrain; }

    std::string_view Id() const noexcept override { return kTerrainPlayIsolationProviderId; }
    std::string_view Label() const noexcept override { return "Terrain"; }
    int Order() const noexcept override { return 200; }
    bool Available() const noexcept override { return terrain_ != nullptr; }
    bool Capture(EditorPlaySnapshot& snapshot, EditorError* error) const override;
    bool Restore(const EditorPlaySnapshot& snapshot, EditorError* error) const override;
    bool BuildRuntimeChangeSet(
        const EditorPlaySnapshot& snapshot,
        EditorRuntimeChangeSet& changes,
        EditorError* error) const override;
    uint64_t AuthoringFingerprint() const override;

private:
    TerrainAuthoringState* terrain_ = nullptr;
};

class EditorVfxPlayIsolationProvider final : public IEditorPlayIsolationProvider {
public:
    void Bind(EffectRuntime* runtime) noexcept { runtime_ = runtime; }
    std::string_view Id() const noexcept override { return kVfxPlayIsolationProviderId; }
    std::string_view Label() const noexcept override { return "VFX Authoring Assets"; }
    int Order() const noexcept override { return 300; }
    bool Available() const noexcept override { return runtime_ != nullptr; }
    bool Capture(EditorPlaySnapshot& snapshot, EditorError* error) const override;
    bool Restore(const EditorPlaySnapshot& snapshot, EditorError* error) const override;
    bool BuildRuntimeChangeSet(const EditorPlaySnapshot&, EditorRuntimeChangeSet&, EditorError*) const override;
    uint64_t AuthoringFingerprint() const override;
private:
    EffectRuntime* runtime_ = nullptr;
};

class EditorPostProcessPlayIsolationProvider final : public IEditorPlayIsolationProvider {
public:
    void Bind(PostProcessStack* stack) noexcept { stack_ = stack; }
    std::string_view Id() const noexcept override { return kPostProcessPlayIsolationProviderId; }
    std::string_view Label() const noexcept override { return "Post-process"; }
    int Order() const noexcept override { return 400; }
    bool Available() const noexcept override { return stack_ != nullptr; }
    bool Capture(EditorPlaySnapshot& snapshot, EditorError* error) const override;
    bool Restore(const EditorPlaySnapshot& snapshot, EditorError* error) const override;
    bool BuildRuntimeChangeSet(const EditorPlaySnapshot&, EditorRuntimeChangeSet&, EditorError*) const override;
    uint64_t AuthoringFingerprint() const override;
private:
    PostProcessStack* stack_ = nullptr;
};

} // namespace editor
