#include "EditorCoreRegressionTests.h"

#include "EditorAssetMutationExecutor.h"
#include "EditorAssetMutationSafety.h"
#include "EditorAssetFallbackIconAtlas.h"
#include "EditorAssetImportService.h"
#include "EditorAssetMeshThumbnailPreviewRenderer.h"
#include "EditorAssetPreviewSceneRenderer.h"
#include "EditorAssetPreviewRenderTarget.h"
#include "EditorAssetReferenceDiagnosticsAdapter.h"
#include "EditorAssetRegistry.h"
#include "EditorAssetSelection.h"
#include "EditorAssetThumbnailCache.h"
#include "EditorAssetThumbnailDiagnosticsAdapter.h"
#include "EditorAssetThumbnailService.h"
#include "EditorAssetThumbnailTextureLoader.h"
#include "EditorThumbnailUploadRetirementQueue.h"
#include "EditorDirtyStateService.h"
#include "EditorDetailsEditController.h"
#include "EditorCompositePropertyAccessor.h"
#include "EditorBuiltinDetailsSectionProviders.h"
#include "EditorDetailsSectionProvider.h"
#include "EditorLayoutPersistenceService.h"
#include "EditorPanelLayoutService.h"
#include "EditorPanelRegistry.h"
#include "EditorPlaySessionLifecycleService.h"
#include "EditorPlaySessionRuntimeControlService.h"
#include "EditorRailRuntimePause.h"
#include "EditorRuntimeAuthoringApplyService.h"
#include "EditorRuntimeWatchBuilder.h"
#include "EditorMenuBar.h"
#include "EditorToolbar.h"
#include "EditorToolRegistration.h"
#include "EditorPropertyClipboardService.h"
#include "EditorPropertyEditSession.h"
#include "EditorPropertyEditService.h"
#include "EditorProductionPropertyAdapter.h"
#include "EditorPropertyRegistry.h"
#include "EditorSelection.h"
#include "EditorTransactionStack.h"
#include "EditorValidationService.h"
#include "ExistingFeatureProtection.h"

#include "../AppEditorToolModules.h"
#include "../AppRuntimeState.h"
#include "../EffectAssetLoader.h"
#include "../EffectRuntime.h"
#include "../EffectSystem.h"
#include "../PostProcessStack.h"
#include "../course/CourseAsset.h"
#include "../course/CourseCollisionSystem.h"
#include "../course/CourseSpawnRuntime.h"
#include "../course/PlayerCombatFeelSystem.h"
#include "../course/SectionCheckpointSystem.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace editor {
namespace {

struct RegressionCase {
    std::string name;
    std::function<void()> body;
};

class RegressionRunner {
public:
    explicit RegressionRunner(std::ostream& log)
        : log_(log) {}

    void Run(const RegressionCase& testCase) {
        currentCase_ = testCase.name;
        ++caseCount_;
        try {
            testCase.body();
            log_ << "[PASS] " << testCase.name << '\n';
        } catch (const std::exception& error) {
            ++failedCount_;
            log_ << "[FAIL] " << testCase.name << " :: " << error.what() << '\n';
        } catch (...) {
            ++failedCount_;
            log_ << "[FAIL] " << testCase.name << " :: unknown exception\n";
        }
        currentCase_.clear();
    }

    void Expect(bool condition, std::string_view message) {
        if (condition) {
            return;
        }
        std::ostringstream stream;
        if (!currentCase_.empty()) {
            stream << currentCase_ << ": ";
        }
        stream << message;
        throw std::runtime_error(stream.str());
    }

    uint32_t CaseCount() const { return caseCount_; }
    uint32_t FailedCount() const { return failedCount_; }

private:
    std::ostream& log_;
    std::string currentCase_;
    uint32_t caseCount_ = 0;
    uint32_t failedCount_ = 0;
};

class FakeThumbnailGpuBackend final : public EditorAssetGpuThumbnailBackend {
public:
    bool AllocateThumbnail(
        const EditorAssetGpuThumbnailAllocationRequest& request,
        EditorAssetGpuThumbnailAllocation& outAllocation,
        std::string& outError) override {
        (void)outError;
        const uint32_t descriptorIndex = nextDescriptorIndex_++;
        const uint64_t textureId = 0x70000000ull + descriptorIndex;
        allocated_[request.key] = textureId;
        ++allocationCount_;
        outAllocation.resourceId = textureId;
        outAllocation.displayTextureId = textureId;
        outAllocation.descriptorIndex = descriptorIndex;
        outAllocation.shaderResourceView = true;
        outAllocation.detail = "Fake GPU thumbnail SRV descriptor is allocated.";
        return true;
    }

    void ReleaseThumbnail(std::string_view key, uint64_t resourceId) override {
        (void)resourceId;
        if (allocated_.erase(std::string(key)) > 0) {
            ++releaseCount_;
        }
    }

    uint32_t AllocationCount() const { return allocationCount_; }
    uint32_t ReleaseCount() const { return releaseCount_; }

private:
    uint32_t nextDescriptorIndex_ = 4000;
    uint32_t allocationCount_ = 0;
    uint32_t releaseCount_ = 0;
    std::unordered_map<std::string, uint64_t> allocated_;
};

EditorObjectHandle MakeCourseObject(uint64_t index) {
    EditorObjectHandle handle{};
    handle.domain = EditorDomainId::CourseTerrainPlacement;
    handle.stableId = BuildStableIndexedId("course-terrain", index);
    handle.localIndex = index;
    handle.generation = 7;
    handle.displayName = "Course Terrain #" + std::to_string(index);
    return handle;
}

EditorAssetRecord MakeAsset(
    EditorAssetKind kind,
    std::string id,
    std::string sourcePath,
    bool hasMetadata = false,
    std::string guid = {}) {
    EditorAssetRecord record{};
    record.kind = kind;
    record.id = std::move(id);
    record.displayName = record.id;
    record.logicalPath = record.id;
    record.sourcePath = std::move(sourcePath);
    record.metadataPath = record.sourcePath.empty() ? std::string{} : record.sourcePath + ".meta";
    record.hasMetadata = hasMetadata;
    record.guid = std::move(guid);
    record.referenceable = true;
    return record;
}

void WriteTextFile(const std::filesystem::path& path, const std::string& text) {
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("failed to write " + path.generic_string());
    }
    file << text;
}

void WriteBinaryFile(const std::filesystem::path& path, const std::vector<unsigned char>& bytes) {
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("failed to write " + path.generic_string());
    }
    file.write(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
}

std::vector<unsigned char> MakeBmpPreviewHeader(uint32_t width, uint32_t height) {
    std::vector<unsigned char> bytes(26, 0);
    bytes[0] = 'B';
    bytes[1] = 'M';
    const auto writeLe32 = [&](std::size_t offset, uint32_t value) {
        bytes[offset + 0] = static_cast<unsigned char>(value & 0xff);
        bytes[offset + 1] = static_cast<unsigned char>((value >> 8) & 0xff);
        bytes[offset + 2] = static_cast<unsigned char>((value >> 16) & 0xff);
        bytes[offset + 3] = static_cast<unsigned char>((value >> 24) & 0xff);
    };
    writeLe32(18, width);
    writeLe32(22, height);
    return bytes;
}

std::vector<unsigned char> MakeTgaPreviewImage(uint16_t width, uint16_t height) {
    std::vector<unsigned char> bytes(18 + static_cast<size_t>(width) * height * 4, 0);
    bytes[2] = 2;
    bytes[12] = static_cast<unsigned char>(width & 0xff);
    bytes[13] = static_cast<unsigned char>((width >> 8) & 0xff);
    bytes[14] = static_cast<unsigned char>(height & 0xff);
    bytes[15] = static_cast<unsigned char>((height >> 8) & 0xff);
    bytes[16] = 32;
    bytes[17] = 0x28;
    for (uint16_t y = 0; y < height; ++y) {
        for (uint16_t x = 0; x < width; ++x) {
            const size_t offset = 18 + (static_cast<size_t>(y) * width + x) * 4;
            bytes[offset + 0] = static_cast<unsigned char>(40 + x);
            bytes[offset + 1] = static_cast<unsigned char>(80 + y);
            bytes[offset + 2] = 180;
            bytes[offset + 3] = 255;
        }
    }
    return bytes;
}

void RemoveTreeIfPresent(const std::filesystem::path& path) {
    std::error_code error;
    if (std::filesystem::exists(path, error)) {
        std::filesystem::remove_all(path, error);
    }
}

class RegressionPropertyAccessor final : public EditorPropertyAccessor {
public:
    RegressionPropertyAccessor(EditorObjectHandle target, std::string propertyPath, float value)
        : target_(std::move(target))
        , propertyPath_(std::move(propertyPath)) {
        value_.floatValue = value;
    }

    RegressionPropertyAccessor(
        EditorObjectHandle target,
        std::string propertyPath,
        EditorPropertyValue value)
        : target_(std::move(target))
        , propertyPath_(std::move(propertyPath))
        , value_(std::move(value)) {
    }

    bool CanAccess(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor) const override {
        return object.SameObject(target_) && descriptor.name == propertyPath_;
    }

    bool Get(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor,
        EditorPropertyValue& outValue) const override {
        if (!CanAccess(object, descriptor)) {
            return false;
        }
        outValue = value_;
        return true;
    }

    bool Set(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor,
        const EditorPropertyValue& value,
        std::string* errorMessage) override {
        if (!CanAccess(object, descriptor)) {
            if (errorMessage != nullptr) {
                *errorMessage = "regression property is inaccessible";
            }
            return false;
        }
        value_ = value;
        return true;
    }

    float Value() const { return value_.floatValue; }
    const EditorPropertyValue& PropertyValue() const { return value_; }

private:
    EditorObjectHandle target_{};
    std::string propertyPath_;
    EditorPropertyValue value_{};
};

class RegressionMultiPropertyAccessor final : public EditorPropertyAccessor {
public:
    explicit RegressionMultiPropertyAccessor(EditorObjectHandle target)
        : target_(std::move(target)) {
    }

    void SetInitial(std::string propertyPath, EditorPropertyValue value) {
        values_.push_back(PropertySlot{std::move(propertyPath), std::move(value)});
    }

    bool CanAccess(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor) const override {
        return object.SameObject(target_) && FindSlot(descriptor.name) != nullptr;
    }

    bool Get(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor,
        EditorPropertyValue& outValue) const override {
        const PropertySlot* slot = object.SameObject(target_) ? FindSlot(descriptor.name) : nullptr;
        if (slot == nullptr) {
            return false;
        }
        outValue = slot->value;
        return true;
    }

    bool Set(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor,
        const EditorPropertyValue& value,
        std::string* errorMessage) override {
        PropertySlot* slot = object.SameObject(target_) ? FindSlot(descriptor.name) : nullptr;
        if (slot == nullptr) {
            if (errorMessage != nullptr) {
                *errorMessage = "regression batch property is inaccessible";
            }
            return false;
        }
        slot->value = value;
        return true;
    }

    const EditorPropertyValue& Value(const std::string& propertyPath) const {
        const PropertySlot* slot = FindSlot(propertyPath);
        if (slot == nullptr) {
            throw std::runtime_error("missing regression property");
        }
        return slot->value;
    }

private:
    struct PropertySlot {
        std::string path;
        EditorPropertyValue value;
    };

    PropertySlot* FindSlot(const std::string& propertyPath) {
        auto it = std::find_if(
            values_.begin(),
            values_.end(),
            [&](const PropertySlot& slot) {
                return slot.path == propertyPath;
            });
        return it != values_.end() ? &*it : nullptr;
    }

    const PropertySlot* FindSlot(const std::string& propertyPath) const {
        auto it = std::find_if(
            values_.begin(),
            values_.end(),
            [&](const PropertySlot& slot) {
                return slot.path == propertyPath;
            });
        return it != values_.end() ? &*it : nullptr;
    }

    EditorObjectHandle target_{};
    std::vector<PropertySlot> values_;
};

class RegressionMultiTargetPropertyAccessor final : public EditorPropertyAccessor {
public:
    void SetInitial(
        EditorObjectHandle target,
        std::string propertyPath,
        EditorPropertyValue value) {
        values_.push_back(PropertySlot{std::move(target), std::move(propertyPath), std::move(value)});
    }

    bool CanAccess(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor) const override {
        return FindSlot(object, descriptor.name) != nullptr;
    }

    bool Get(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor,
        EditorPropertyValue& outValue) const override {
        const PropertySlot* slot = FindSlot(object, descriptor.name);
        if (slot == nullptr) {
            return false;
        }
        outValue = slot->value;
        return true;
    }

    bool Set(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor,
        const EditorPropertyValue& value,
        std::string* errorMessage) override {
        PropertySlot* slot = FindSlot(object, descriptor.name);
        if (slot == nullptr) {
            if (errorMessage != nullptr) {
                *errorMessage = "regression multi-target property is inaccessible";
            }
            return false;
        }
        slot->value = value;
        return true;
    }

    const EditorPropertyValue& Value(
        const EditorObjectHandle& object,
        const std::string& propertyPath) const {
        const PropertySlot* slot = FindSlot(object, propertyPath);
        if (slot == nullptr) {
            throw std::runtime_error("missing regression multi-target property");
        }
        return slot->value;
    }

private:
    struct PropertySlot {
        EditorObjectHandle target;
        std::string path;
        EditorPropertyValue value;
    };

    PropertySlot* FindSlot(
        const EditorObjectHandle& object,
        const std::string& propertyPath) {
        auto it = std::find_if(
            values_.begin(),
            values_.end(),
            [&](const PropertySlot& slot) {
                return slot.target.SameObject(object) && slot.path == propertyPath;
            });
        return it != values_.end() ? &*it : nullptr;
    }

    const PropertySlot* FindSlot(
        const EditorObjectHandle& object,
        const std::string& propertyPath) const {
        auto it = std::find_if(
            values_.begin(),
            values_.end(),
            [&](const PropertySlot& slot) {
                return slot.target.SameObject(object) && slot.path == propertyPath;
            });
        return it != values_.end() ? &*it : nullptr;
    }

    std::vector<PropertySlot> values_;
};

void TestTransactionStack(RegressionRunner& runner) {
    EditorTransactionStack stack;
    stack.SetMaxHistory(2);
    const EditorObjectHandle target = MakeCourseObject(3);

    stack.PushPropertyDelta(
        "Move Terrain",
        target,
        "CourseTerrainPlacement.distance",
        "float",
        "10",
        "20");
    stack.PushSnapshot("Snapshot A", target, "before-a", "after-a");
    stack.PushSnapshot("Snapshot B", target, "before-b", "after-b");

    runner.Expect(stack.UndoDepth() == 2, "max history should trim old undo entries");
    runner.Expect(stack.RedoDepth() == 0, "redo stack should be empty after push");

    std::vector<EditorTransactionApplyMode> applyModes;
    const bool undoOk = stack.Undo(
        [&](const EditorTransactionRecord& record, EditorTransactionApplyMode mode) {
            runner.Expect(record.label == "Snapshot B", "undo should apply newest transaction");
            applyModes.push_back(mode);
            return true;
        });
    runner.Expect(undoOk, "undo should succeed");
    runner.Expect(stack.UndoDepth() == 1 && stack.RedoDepth() == 1, "undo/redo depths should update");

    const bool redoOk = stack.Redo(
        [&](const EditorTransactionRecord& record, EditorTransactionApplyMode mode) {
            runner.Expect(record.label == "Snapshot B", "redo should reapply newest transaction");
            applyModes.push_back(mode);
            return true;
        });
    runner.Expect(redoOk, "redo should succeed");
    runner.Expect(applyModes.size() == 2, "undo and redo callbacks should both run");
    runner.Expect(applyModes[0] == EditorTransactionApplyMode::Undo, "first callback should be undo");
    runner.Expect(applyModes[1] == EditorTransactionApplyMode::Redo, "second callback should be redo");

    EditorPropertyChange staged{};
    staged.target = target;
    staged.propertyPath = "CourseTerrainPlacement.scale";
    staged.displayName = "Scale";
    staged.valueType = "vec3";
    staged.beforeValue = "1,1,1";
    staged.afterValue = "2,2,2";
    stack.StagePropertyDelta(staged);
    runner.Expect(stack.HasStagedPropertyDelta(), "staged property delta should be visible");
    const EditorPropertyChange consumed = stack.ConsumeStagedPropertyDelta();
    runner.Expect(consumed.propertyPath == staged.propertyPath, "staged property delta should round-trip");
    runner.Expect(!stack.HasStagedPropertyDelta(), "staged property delta should clear after consume");

    EditorPropertyChange stagedB = staged;
    stagedB.propertyPath = "CourseTerrainPlacement.rotation";
    stagedB.displayName = "Rotation";
    stagedB.beforeValue = "0,0,0";
    stagedB.afterValue = "0,90,0";
    stack.StagePropertyDeltas({staged, stagedB});
    runner.Expect(stack.HasStagedPropertyDelta(), "staged multi-property delta should be visible");
    runner.Expect(stack.StagedPropertyDeltaCount() == 2, "staged multi-property delta should preserve count");
    std::vector<EditorPropertyChange> consumedBatch = stack.ConsumeStagedPropertyDeltas();
    runner.Expect(consumedBatch.size() == 2, "staged multi-property delta should round-trip");

    stack.PushMultiPropertyDelta("Transform Batch", target, consumedBatch);
    const EditorTransactionRecord* last = stack.LastTransaction();
    runner.Expect(last != nullptr, "multi-property delta should push a transaction");
    runner.Expect(
        last->payload.kind == EditorTransactionPayloadKind::MultiPropertyDelta,
        "multi-property delta should use batch payload kind");
    runner.Expect(last->payload.propertyChanges.size() == 2, "multi-property payload should preserve changes");

    EditorAssetRecord assetBefore = MakeAsset(
        EditorAssetKind::Mesh,
        "asset_before",
        "Resources/tests/asset_before.mesh",
        true,
        "guid-asset-before");
    EditorAssetRecord assetAfter = assetBefore;
    assetAfter.id = "asset_after";
    assetAfter.sourcePath = "Resources/tests/asset_after.mesh";
    EditorAssetMutationChange assetChange{};
    assetChange.kind = EditorAssetMutationKind::Rename;
    assetChange.beforeRecord = assetBefore;
    assetChange.afterRecord = assetAfter;
    EditorObjectHandle assetTarget{};
    assetTarget.domain = EditorDomainId::Asset;
    assetTarget.stableId = "Mesh:asset_before";
    stack.PushAssetMutation("Rename Asset", assetTarget, assetChange);
    last = stack.LastTransaction();
    runner.Expect(last != nullptr, "asset mutation should push a transaction");
    runner.Expect(
        last->payload.kind == EditorTransactionPayloadKind::AssetMutation,
        "asset mutation should use asset payload kind");
    runner.Expect(
        last->payload.assetMutation.beforeRecord.id == "asset_before" &&
            last->payload.assetMutation.afterRecord.id == "asset_after",
        "asset mutation payload should preserve before and after records");
}

void TestSelectionAndPropertyRegistry(RegressionRunner& runner) {
    EditorSelection selection;
    const EditorObjectHandle primary = MakeCourseObject(1);
    const EditorObjectHandle secondary = MakeCourseObject(2);
    selection.SetPrimary(primary);
    selection.Add(secondary);

    runner.Expect(selection.Count() == 2, "selection should contain primary and secondary");
    runner.Expect(selection.Primary() != nullptr, "selection should expose primary");
    runner.Expect(selection.Contains(primary), "selection should contain primary handle");
    runner.Expect(selection.Contains(secondary), "selection should contain secondary handle");

    EditorPropertyRegistry registry;
    RegisterBuiltInCourseObjectProperties(registry);
    runner.Expect(registry.Count() > 0, "built-in course properties should register descriptors");
    runner.Expect(
        registry.Find(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.meshId") != nullptr,
        "course terrain mesh asset reference descriptor should exist");

    uint32_t enumCount = 0;
    uint32_t enumWithOptions = 0;
    for (const EditorPropertyDescriptor& descriptor : registry.Descriptors()) {
        if (descriptor.kind == EditorPropertyKind::Enum) {
            ++enumCount;
            if (!descriptor.enumOptions.empty()) {
                ++enumWithOptions;
            }
        }
    }
    runner.Expect(enumCount > 0, "built-in registry should include enum descriptors");
    runner.Expect(enumCount == enumWithOptions, "enum descriptors should have combo options");

    EditorPropertyRegistry fullRegistry;
    RegisterBuiltInEditorProperties(fullRegistry);
    runner.Expect(
        fullRegistry.Count() > registry.Count(),
        "full built-in editor registry should expand beyond course descriptors");
    runner.Expect(
        fullRegistry.Find(EditorDomainId::VfxEffectAsset, "VfxEffectAsset.technique") != nullptr,
        "vfx effect asset descriptors should register");
    runner.Expect(
        fullRegistry.Find(EditorDomainId::TerrainGeneration, "TerrainGeneration.chunkLength") != nullptr,
        "terrain generation descriptors should register");
    runner.Expect(
        fullRegistry.Find(EditorDomainId::PostProcessPass, "PostProcessPass.intensity") != nullptr,
        "post-process pass descriptors should register");
    runner.Expect(
        fullRegistry.Find(EditorDomainId::RenderPreset, "RenderPreset.renderScale") != nullptr,
        "render preset descriptors should register");
    runner.Expect(
        fullRegistry.Find(EditorDomainId::CourseCameraKey, "CourseCameraKey.fov") != nullptr,
        "course camera key descriptors should register");
    runner.Expect(
        fullRegistry.Find(EditorDomainId::CourseEventMarker, "CourseEventMarker.payload") != nullptr,
        "course event marker descriptors should register");
    runner.Expect(
        fullRegistry.Find(EditorDomainId::GameplayTuning, "GameplayTuning.lockOnRange") != nullptr,
        "gameplay tuning descriptors should register");
    const EditorPropertyDescriptor* runtimeParticles =
        fullRegistry.Find(EditorDomainId::VfxEffectInstance, "VfxEffectInstance.particleCount");
    runner.Expect(
        runtimeParticles != nullptr &&
            runtimeParticles->readOnly &&
            runtimeParticles->runtimeOnly &&
            !runtimeParticles->readOnlyReason.empty(),
        "runtime-only descriptors should carry explicit read-only reasons");
    const EditorPropertyDescriptor* resettableRenderScale =
        fullRegistry.Find(EditorDomainId::RenderPreset, "RenderPreset.renderScale");
    runner.Expect(
        resettableRenderScale != nullptr &&
            resettableRenderScale->resettable &&
            !resettableRenderScale->defaultValue.empty(),
        "resettable descriptors should carry default values");
    const EditorPropertyDescriptor* resettableCourseDistance =
        fullRegistry.Find(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.distance");
    EditorPropertyValue defaultCourseDistance{};
    runner.Expect(
        resettableCourseDistance != nullptr &&
            resettableCourseDistance->resettable &&
            ParseEditorPropertyValue(
                *resettableCourseDistance,
                resettableCourseDistance->defaultValue,
                defaultCourseDistance) &&
            defaultCourseDistance.floatValue == 0.0f,
        "mutable course descriptors should expose parseable reset defaults");
}

void TestPropertyEditService(RegressionRunner& runner) {
    EditorPropertyRegistry registry;
    RegisterBuiltInCourseObjectProperties(registry);
    const EditorPropertyDescriptor* descriptor =
        registry.Find(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.distance");
    runner.Expect(descriptor != nullptr, "distance descriptor should exist");
    const EditorPropertyDescriptor* scaleDescriptor =
        registry.Find(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.scale");
    const EditorPropertyDescriptor* layerDescriptor =
        registry.Find(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.layer");
    const EditorPropertyDescriptor* idDescriptor =
        registry.Find(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.id");
    runner.Expect(scaleDescriptor != nullptr, "scale descriptor should exist");
    runner.Expect(layerDescriptor != nullptr, "layer descriptor should exist");
    runner.Expect(idDescriptor != nullptr, "id descriptor should exist");

    EditorPropertyValue parsed{};
    runner.Expect(
        ParseEditorPropertyValue(*descriptor, "42.500", parsed) && parsed.floatValue == 42.5f,
        "float property value should parse");
    runner.Expect(
        ParseEditorPropertyValue(*scaleDescriptor, "1.000, 2.000, 3.000", parsed) &&
            parsed.vec3Value.x == 1.0f &&
            parsed.vec3Value.y == 2.0f &&
            parsed.vec3Value.z == 3.0f,
        "vec3 property value should parse");
    runner.Expect(
        ParseEditorPropertyValue(*layerDescriptor, "hero_landmark", parsed) &&
            parsed.stringValue == "hero_landmark",
        "enum property value should parse");
    runner.Expect(
        ParseEditorPropertyValue(*idDescriptor, "terrain_alpha", parsed) &&
            parsed.stringValue == "terrain_alpha",
        "string property value should parse");
    runner.Expect(
        !ParseEditorPropertyValue(*descriptor, "not-a-float", parsed),
        "invalid float property value should fail parse");

    const EditorObjectHandle target = MakeCourseObject(4);
    RegressionPropertyAccessor accessor(target, descriptor->name, 100.0f);
    EditorTransactionStack transactions;
    EditorDirtyStateService dirtyState;
    EditorNotificationCenter notifications;
    EditorPropertyEditService editService;

    EditorPropertyValue requested{};
    requested.floatValue = 125.0f;
    const EditorPropertyEditResult result =
        editService.Apply(
            EditorPropertyEditRequest{
                &accessor,
                &transactions,
                &dirtyState,
                &notifications,
                target,
                descriptor,
                requested,
                true,
                true,
                "regression.propertyEdit"});

    runner.Expect(result.applied, "property edit service should apply editable value");
    runner.Expect(result.changed, "property edit service should report changed value");
    runner.Expect(accessor.Value() == 125.0f, "property edit service should write through accessor");
    runner.Expect(transactions.HasStagedPropertyDelta(), "property edit service should stage transaction delta");
    const EditorPropertyChange staged = transactions.ConsumeStagedPropertyDelta();
    runner.Expect(staged.propertyPath == descriptor->name, "staged delta should preserve property path");
    runner.Expect(staged.beforeValue == "100.000", "staged delta should record before value");
    runner.Expect(staged.afterValue == "125.000", "staged delta should record after value");
    runner.Expect(
        dirtyState.HasDirtyDomain(EditorDirtyDomain::CourseAuthoring),
        "property edit service should mark course authoring dirty");
    runner.Expect(notifications.Count() == 1, "first dirty property edit should push notification");

    const EditorPropertyEditResult unchanged =
        editService.Apply(
            EditorPropertyEditRequest{
                &accessor,
                &transactions,
                &dirtyState,
                &notifications,
                target,
                descriptor,
                requested,
                true,
                true,
                "regression.propertyEdit"});
    runner.Expect(unchanged.applied, "unchanged property edit should still apply safely");
    runner.Expect(!unchanged.changed, "unchanged property edit should not create mutation");
    runner.Expect(!transactions.HasStagedPropertyDelta(), "unchanged property edit should not stage delta");
    runner.Expect(notifications.Count() == 1, "unchanged property edit should not notify again");

    requested.floatValue = 150.0f;
    const EditorPropertyEditResult locked =
        editService.Apply(
            EditorPropertyEditRequest{
                &accessor,
                &transactions,
                &dirtyState,
                &notifications,
                target,
                descriptor,
                requested,
                false,
                true,
                "regression.propertyEdit"});
    runner.Expect(!locked.applied, "locked property edit should be rejected");
    runner.Expect(accessor.Value() == 125.0f, "locked property edit should not mutate accessor");
    runner.Expect(notifications.Count() == 2, "locked property edit should notify failure");

    EditorPropertyClipboardService clipboard;
    const EditorPropertyClipboardResult copyResult =
        clipboard.Copy(
            EditorPropertyClipboardCopyRequest{
                &accessor,
                &notifications,
                target,
                descriptor,
                false,
                "regression.propertyClipboard"});
    runner.Expect(copyResult.applied, "property clipboard should copy through accessor");
    runner.Expect(
        clipboard.HasValue() && clipboard.Summary() == "125.000",
        "property clipboard should preserve formatted value");
    runner.Expect(
        clipboard.CanPasteTo(*descriptor),
        "property clipboard should allow compatible paste");
    runner.Expect(
        !clipboard.CanPasteTo(*layerDescriptor),
        "property clipboard should reject incompatible descriptor kind");
    EditorPropertyValue pasteValue{};
    const EditorPropertyClipboardResult pasteValueResult =
        clipboard.BuildPasteValue(EditorPropertyClipboardPasteRequest{descriptor}, pasteValue);
    runner.Expect(
        pasteValueResult.applied && pasteValue.floatValue == 125.0f,
        "property clipboard should parse paste value for compatible descriptor");
    RegressionPropertyAccessor pasteAccessor(target, descriptor->name, 5.0f);
    EditorTransactionStack pasteTransactions;
    EditorDirtyStateService pasteDirtyState;
    const EditorPropertyEditResult pasteApplyResult =
        editService.Apply(
            EditorPropertyEditRequest{
                &pasteAccessor,
                &pasteTransactions,
                &pasteDirtyState,
                &notifications,
                target,
                descriptor,
                pasteValue,
                true,
                true,
                "regression.propertyClipboard"});
    runner.Expect(
        pasteApplyResult.applied && pasteApplyResult.changed && pasteAccessor.Value() == 125.0f,
        "property clipboard paste should apply through the official edit service");
    runner.Expect(
        pasteTransactions.HasStagedPropertyDelta(),
        "property clipboard paste should stage undoable property delta");

    EditorPropertyValue invalidScale{};
    invalidScale.vec3Value = {-1.0f, 1.0f, 1.0f};
    EditorPropertyValue validationScaleInitial{};
    validationScaleInitial.vec3Value = {1.0f, 1.0f, 1.0f};
    RegressionPropertyAccessor scaleValidationAccessor(
        target,
        scaleDescriptor->name,
        validationScaleInitial);
    const EditorPropertyEditResult invalidRange =
        editService.Apply(
            EditorPropertyEditRequest{
                &scaleValidationAccessor,
                &transactions,
                &dirtyState,
                &notifications,
                target,
                scaleDescriptor,
                invalidScale,
                true,
                true,
                "regression.propertyValidation"});
    runner.Expect(!invalidRange.applied, "range validation should reject invalid vector values");
    runner.Expect(
        invalidRange.message.find("outside the allowed range") != std::string::npos,
        "range validation should report an actionable reason");

    EditorPropertyValue invalidEnum{};
    invalidEnum.stringValue = "not_a_layer";
    EditorPropertyValue validationEnumInitial{};
    validationEnumInitial.stringValue = "gameplay_collision";
    RegressionPropertyAccessor enumValidationAccessor(
        target,
        layerDescriptor->name,
        validationEnumInitial);
    const EditorPropertyEditResult invalidEnumResult =
        editService.Apply(
            EditorPropertyEditRequest{
                &enumValidationAccessor,
                &transactions,
                &dirtyState,
                &notifications,
                target,
                layerDescriptor,
                invalidEnum,
                true,
                true,
                "regression.propertyValidation"});
    runner.Expect(!invalidEnumResult.applied, "enum validation should reject unregistered options");

    EditorPropertyValue initialDistance{};
    initialDistance.floatValue = 10.0f;
    EditorPropertyValue initialScale{};
    initialScale.vec3Value = {1.0f, 1.0f, 1.0f};
    RegressionMultiPropertyAccessor batchAccessor(target);
    batchAccessor.SetInitial(descriptor->name, initialDistance);
    batchAccessor.SetInitial(scaleDescriptor->name, initialScale);

    EditorPropertyValue batchDistance{};
    batchDistance.floatValue = 20.0f;
    EditorPropertyValue batchScale{};
    batchScale.vec3Value = {2.0f, 2.0f, 2.0f};
    const EditorPropertyBatchEditResult batchResult =
        editService.ApplyBatch(
            EditorPropertyBatchEditRequest{
                &batchAccessor,
                &transactions,
                &dirtyState,
                &notifications,
                {
                    EditorPropertyBatchEdit{target, descriptor, batchDistance},
                    EditorPropertyBatchEdit{target, scaleDescriptor, batchScale},
                },
                "Transform Batch",
                target,
                true,
                true,
                "regression.propertyBatch"});
    runner.Expect(batchResult.applied, "batch property edit should apply");
    runner.Expect(batchResult.changed, "batch property edit should report changes");
    runner.Expect(batchResult.changedCount == 2, "batch property edit should report changed count");
    runner.Expect(transactions.StagedPropertyDeltaCount() == 2, "batch property edit should stage multiple deltas");
    std::vector<EditorPropertyChange> batchChanges = transactions.ConsumeStagedPropertyDeltas();
    runner.Expect(batchChanges.size() == 2, "batch staged changes should consume as a group");
    runner.Expect(batchAccessor.Value(descriptor->name).floatValue == 20.0f, "batch edit should set float value");
    runner.Expect(
        batchAccessor.Value(scaleDescriptor->name).vec3Value.x == 2.0f,
        "batch edit should set vec3 value");

    EditorTransactionStack batchTransactions;
    batchTransactions.PushMultiPropertyDelta("Transform Batch", target, batchChanges);
    bool batchUndoApplied = false;
    runner.Expect(
        batchTransactions.Undo(
            [&](const EditorTransactionRecord& record, EditorTransactionApplyMode mode) {
                const EditorPropertyApplyDeltaResult deltaResult =
                    editService.ApplyDelta(
                        EditorPropertyApplyDeltaRequest{
                            &batchAccessor,
                            &dirtyState,
                            &notifications,
                            &registry,
                            &record,
                            mode,
                            true,
                            true,
                            "regression.propertyBatchDelta"});
                batchUndoApplied =
                    deltaResult.applied &&
                    batchAccessor.Value(descriptor->name).floatValue == 10.0f &&
                    batchAccessor.Value(scaleDescriptor->name).vec3Value.x == 1.0f;
                return deltaResult.applied;
            }),
        "multi-property delta undo should apply through edit service");
    runner.Expect(batchUndoApplied, "multi-property delta undo should restore before values");

    bool batchRedoApplied = false;
    runner.Expect(
        batchTransactions.Redo(
            [&](const EditorTransactionRecord& record, EditorTransactionApplyMode mode) {
                const EditorPropertyApplyDeltaResult deltaResult =
                    editService.ApplyDelta(
                        EditorPropertyApplyDeltaRequest{
                            &batchAccessor,
                            &dirtyState,
                            &notifications,
                            &registry,
                            &record,
                            mode,
                            true,
                            true,
                            "regression.propertyBatchDelta"});
                batchRedoApplied =
                    deltaResult.applied &&
                    batchAccessor.Value(descriptor->name).floatValue == 20.0f &&
                    batchAccessor.Value(scaleDescriptor->name).vec3Value.x == 2.0f;
                return deltaResult.applied;
            }),
        "multi-property delta redo should apply through edit service");
    runner.Expect(batchRedoApplied, "multi-property delta redo should restore after values");

    EditorPropertyEditSession session;
    EditorPropertyValue sessionDistanceBefore{};
    sessionDistanceBefore.floatValue = 30.0f;
    EditorPropertyValue sessionScaleBefore{};
    sessionScaleBefore.vec3Value = {3.0f, 3.0f, 3.0f};
    RegressionMultiPropertyAccessor sessionAccessor(target);
    sessionAccessor.SetInitial(descriptor->name, sessionDistanceBefore);
    sessionAccessor.SetInitial(scaleDescriptor->name, sessionScaleBefore);

    const EditorPropertyEditSessionResult beginSession =
        session.Begin(
            EditorPropertyEditSessionBeginRequest{
                &sessionAccessor,
                {
                    EditorPropertyEditSessionProperty{target, *descriptor},
                    EditorPropertyEditSessionProperty{target, *scaleDescriptor},
                },
                "Session Transform",
                target,
                true,
                true,
                "regression.propertySession"});
    runner.Expect(beginSession.applied, "property edit session should begin");

    EditorPropertyValue sessionDistancePreview{};
    sessionDistancePreview.floatValue = 40.0f;
    EditorPropertyValue sessionScalePreview{};
    sessionScalePreview.vec3Value = {4.0f, 4.0f, 4.0f};
    const EditorPropertyEditSessionResult previewSession =
        session.Preview(
            EditorPropertyEditSessionPreviewRequest{
                &sessionAccessor,
                {
                    EditorPropertyEditSessionValue{target, descriptor->name, sessionDistancePreview},
                    EditorPropertyEditSessionValue{target, scaleDescriptor->name, sessionScalePreview},
                },
                true,
                true,
                "regression.propertySession"});
    runner.Expect(previewSession.applied, "property edit session preview should apply");
    runner.Expect(previewSession.changed, "property edit session preview should report changed");
    runner.Expect(sessionAccessor.Value(descriptor->name).floatValue == 40.0f, "session preview should set float");
    const EditorPropertyEditSessionResult cancelSession =
        session.Cancel(
            EditorPropertyEditSessionCancelRequest{
                &sessionAccessor,
                true,
                "regression.propertySession"});
    runner.Expect(cancelSession.applied, "property edit session cancel should apply");
    runner.Expect(sessionAccessor.Value(descriptor->name).floatValue == 30.0f, "session cancel should restore float");
    runner.Expect(
        sessionAccessor.Value(scaleDescriptor->name).vec3Value.x == 3.0f,
        "session cancel should restore vec3");

    runner.Expect(
        session.Begin(
            EditorPropertyEditSessionBeginRequest{
                &sessionAccessor,
                {
                    EditorPropertyEditSessionProperty{target, *descriptor},
                    EditorPropertyEditSessionProperty{target, *scaleDescriptor},
                },
                "Session Transform",
                target,
                true,
                true,
                "regression.propertySession"})
            .applied,
        "property edit session should begin again");
    runner.Expect(
        session.Preview(
            EditorPropertyEditSessionPreviewRequest{
                &sessionAccessor,
                {
                    EditorPropertyEditSessionValue{target, descriptor->name, sessionDistancePreview},
                    EditorPropertyEditSessionValue{target, scaleDescriptor->name, sessionScalePreview},
                },
                true,
                true,
                "regression.propertySession"})
            .applied,
        "property edit session preview should apply before commit");
    const EditorPropertyEditSessionResult commitSession =
        session.Commit(
            EditorPropertyEditSessionCommitRequest{
                &sessionAccessor,
                &sessionAccessor,
                &transactions,
                &dirtyState,
                &notifications,
                true,
                true,
                "regression.propertySession"});
    runner.Expect(commitSession.applied, "property edit session commit should apply");
    runner.Expect(commitSession.changed, "property edit session commit should report changed");
    runner.Expect(transactions.StagedPropertyDeltaCount() == 2, "session commit should stage batch delta");
    runner.Expect(sessionAccessor.Value(descriptor->name).floatValue == 40.0f, "session commit should keep final float");

    EditorPropertyEditSession detailsSession;
    EditorPropertyValue detailsBefore{};
    detailsBefore.floatValue = 50.0f;
    RegressionPropertyAccessor detailsAccessor(target, descriptor->name, detailsBefore);
    EditorTransactionStack detailsTransactions;
    EditorDirtyStateService detailsDirtyState;
    EditorNotificationCenter detailsNotifications;
    const EditorDetailsEditControllerContext detailsContext{
        &detailsSession,
        &detailsAccessor,
        &detailsAccessor,
        &detailsTransactions,
        &detailsDirtyState,
        &detailsNotifications,
        true,
        true,
        "regression.details"};

    EditorPropertyValue detailsPreview{};
    detailsPreview.floatValue = 75.0f;
    const EditorPropertyEditSessionResult detailsPreviewResult =
        PreviewEditorDetailsPropertyEdit(
            detailsContext,
            target,
            *descriptor,
            detailsPreview);
    runner.Expect(detailsPreviewResult.applied, "details controller preview should apply");
    runner.Expect(detailsSession.IsActive(), "details controller preview should own active session");
    runner.Expect(detailsAccessor.Value() == 75.0f, "details controller preview should write accessor");
    runner.Expect(
        !detailsTransactions.HasStagedPropertyDelta(),
        "details controller preview should not stage a transaction");
    runner.Expect(
        !detailsDirtyState.HasDirtyDomain(EditorDirtyDomain::CourseAuthoring),
        "details controller preview should not mark dirty");

    const EditorPropertyEditSessionResult detailsCommitResult =
        CommitEditorDetailsPropertyEdit(detailsContext);
    runner.Expect(detailsCommitResult.applied, "details controller commit should apply");
    runner.Expect(detailsCommitResult.changed, "details controller commit should report changed");
    runner.Expect(detailsAccessor.Value() == 75.0f, "details controller commit should keep final value");
    runner.Expect(
        detailsTransactions.StagedPropertyDeltaCount() == 1,
        "details controller commit should stage exactly one delta");
    const EditorPropertyChange detailsChange = detailsTransactions.ConsumeStagedPropertyDelta();
    runner.Expect(detailsChange.propertyPath == descriptor->name, "details delta should preserve property path");
    runner.Expect(detailsChange.beforeValue == "50.000", "details delta should record before value");
    runner.Expect(detailsChange.afterValue == "75.000", "details delta should record after value");
    runner.Expect(
        detailsDirtyState.HasDirtyDomain(EditorDirtyDomain::CourseAuthoring),
        "details controller commit should mark course authoring dirty");

    EditorPropertyValue detailsImmediate{};
    detailsImmediate.floatValue = 90.0f;
    const EditorPropertyEditSessionResult detailsImmediateResult =
        ApplyEditorDetailsImmediatePropertyEdit(
            detailsContext,
            target,
            *descriptor,
            detailsImmediate);
    runner.Expect(detailsImmediateResult.applied, "details controller immediate edit should apply");
    runner.Expect(!detailsSession.IsActive(), "details controller immediate edit should close session");
    runner.Expect(detailsAccessor.Value() == 90.0f, "details controller immediate edit should set value");
    runner.Expect(
        detailsTransactions.StagedPropertyDeltaCount() == 1,
        "details controller immediate edit should stage one delta");
    detailsTransactions.ConsumeStagedPropertyDelta();

    const EditorObjectHandle multiTargetA = MakeCourseObject(41);
    const EditorObjectHandle multiTargetB = MakeCourseObject(42);
    EditorPropertyValue multiBeforeA{};
    multiBeforeA.floatValue = 5.0f;
    EditorPropertyValue multiBeforeB{};
    multiBeforeB.floatValue = 10.0f;
    RegressionMultiTargetPropertyAccessor multiDetailsAccessor;
    multiDetailsAccessor.SetInitial(multiTargetA, descriptor->name, multiBeforeA);
    multiDetailsAccessor.SetInitial(multiTargetB, descriptor->name, multiBeforeB);
    EditorPropertyEditSession multiDetailsSession;
    EditorTransactionStack multiDetailsTransactions;
    EditorDirtyStateService multiDetailsDirtyState;
    EditorNotificationCenter multiDetailsNotifications;
    const EditorDetailsEditControllerContext multiDetailsContext{
        &multiDetailsSession,
        &multiDetailsAccessor,
        &multiDetailsAccessor,
        &multiDetailsTransactions,
        &multiDetailsDirtyState,
        &multiDetailsNotifications,
        true,
        true,
        "regression.details.multi"};
    const std::vector<EditorObjectHandle> multiTargets{multiTargetA, multiTargetB};

    EditorPropertyValue multiRequested{};
    multiRequested.floatValue = 25.0f;
    const EditorPropertyEditSessionResult multiPreviewResult =
        PreviewEditorDetailsPropertyBatchEdit(
            multiDetailsContext,
            multiTargets,
            *descriptor,
            multiRequested);
    runner.Expect(multiPreviewResult.applied, "details controller multi preview should apply");
    runner.Expect(multiDetailsSession.IsActive(), "details controller multi preview should open session");
    runner.Expect(
        multiDetailsAccessor.Value(multiTargetA, descriptor->name).floatValue == 25.0f &&
            multiDetailsAccessor.Value(multiTargetB, descriptor->name).floatValue == 25.0f,
        "details controller multi preview should write all selected targets");
    runner.Expect(
        !multiDetailsTransactions.HasStagedPropertyDelta(),
        "details controller multi preview should not stage transactions");

    const EditorPropertyEditSessionResult multiCommitResult =
        CommitEditorDetailsPropertyEdit(multiDetailsContext);
    runner.Expect(multiCommitResult.applied, "details controller multi commit should apply");
    runner.Expect(multiCommitResult.changed, "details controller multi commit should report changes");
    runner.Expect(
        multiDetailsTransactions.StagedPropertyDeltaCount() == 2,
        "details controller multi commit should stage one delta per selected target");
    runner.Expect(
        multiDetailsDirtyState.HasDirtyDomain(EditorDirtyDomain::CourseAuthoring),
        "details controller multi commit should mark course authoring dirty");

    EditorPropertyValue resetValue{};
    runner.Expect(
        descriptor->resettable &&
            ParseEditorPropertyValue(*descriptor, descriptor->defaultValue, resetValue),
        "details reset foundation should parse descriptor defaults");
    const EditorPropertyEditSessionResult resetMultiResult =
        ApplyEditorDetailsImmediatePropertyBatchEdit(
            multiDetailsContext,
            multiTargets,
            *descriptor,
            resetValue);
    runner.Expect(resetMultiResult.applied, "details reset should apply through batch session");
    runner.Expect(
        multiDetailsAccessor.Value(multiTargetA, descriptor->name).floatValue == 0.0f &&
            multiDetailsAccessor.Value(multiTargetB, descriptor->name).floatValue == 0.0f,
        "details reset should write descriptor default to all selected targets");

    const EditorPropertyDescriptor* readOnlyDescriptor =
        registry.Find(EditorDomainId::VfxEffectInstance, "VfxEffectInstance.particleCount");
    if (readOnlyDescriptor == nullptr) {
        EditorPropertyRegistry fullRegistryForReadOnly;
        RegisterBuiltInEditorProperties(fullRegistryForReadOnly);
        readOnlyDescriptor =
            fullRegistryForReadOnly.Find(EditorDomainId::VfxEffectInstance, "VfxEffectInstance.particleCount");
        runner.Expect(readOnlyDescriptor != nullptr, "runtime read-only descriptor should exist");
        const EditorPropertyEditResult readOnlyResult =
            editService.Apply(
                EditorPropertyEditRequest{
                    &detailsAccessor,
                    &detailsTransactions,
                    &dirtyState,
                    &notifications,
                    target,
                    readOnlyDescriptor,
                    requested,
                    true,
                    true,
                    "regression.propertyReadOnly"});
        runner.Expect(!readOnlyResult.applied, "read-only edit should be rejected");
        runner.Expect(
            readOnlyResult.message.find("Runtime inspection only") != std::string::npos,
            "read-only edit should report descriptor read-only reason");
    }

    EditorTransactionStack deltaTransactions;
    deltaTransactions.PushPropertyDelta(
        "Distance Delta",
        target,
        descriptor->name,
        descriptor->valueType,
        "100.000",
        "125.000");
    bool undoApplied = false;
    runner.Expect(
        deltaTransactions.Undo(
            [&](const EditorTransactionRecord& record, EditorTransactionApplyMode mode) {
                const EditorPropertyApplyDeltaResult deltaResult =
                    editService.ApplyDelta(
                        EditorPropertyApplyDeltaRequest{
                            &accessor,
                            &dirtyState,
                            &notifications,
                            &registry,
                            &record,
                            mode,
                            true,
                            true,
                            "regression.propertyDelta"});
                undoApplied = deltaResult.applied && accessor.Value() == 100.0f;
                return deltaResult.applied;
            }),
        "property delta undo should apply through edit service");
    runner.Expect(undoApplied, "property delta undo should restore before value");

    bool redoApplied = false;
    runner.Expect(
        deltaTransactions.Redo(
            [&](const EditorTransactionRecord& record, EditorTransactionApplyMode mode) {
                const EditorPropertyApplyDeltaResult deltaResult =
                    editService.ApplyDelta(
                        EditorPropertyApplyDeltaRequest{
                            &accessor,
                            &dirtyState,
                            &notifications,
                            &registry,
                            &record,
                            mode,
                            true,
                            true,
                            "regression.propertyDelta"});
                redoApplied = deltaResult.applied && accessor.Value() == 125.0f;
                return deltaResult.applied;
            }),
        "property delta redo should apply through edit service");
    runner.Expect(redoApplied, "property delta redo should restore after value");

    EditorPropertyValue scaleValue{};
    scaleValue.vec3Value = {2.0f, 3.0f, 4.0f};
    RegressionPropertyAccessor scaleAccessor(target, scaleDescriptor->name, scaleValue);
    EditorTransactionRecord vecRecord{};
    vecRecord.target = target;
    vecRecord.payload.kind = EditorTransactionPayloadKind::PropertyDelta;
    vecRecord.payload.propertyPath = scaleDescriptor->name;
    vecRecord.payload.beforeSummary = "1.000, 1.000, 1.000";
    vecRecord.payload.afterSummary = "2.000, 3.000, 4.000";
    const EditorPropertyApplyDeltaResult vecUndo =
        editService.ApplyDelta(
            EditorPropertyApplyDeltaRequest{
                &scaleAccessor,
                &dirtyState,
                &notifications,
                &registry,
                &vecRecord,
                EditorTransactionApplyMode::Undo,
                true,
                true,
                "regression.propertyDelta"});
    runner.Expect(vecUndo.applied, "vec3 property delta undo should apply");
    runner.Expect(scaleAccessor.PropertyValue().vec3Value.x == 1.0f, "vec3 undo X should parse");
    runner.Expect(scaleAccessor.PropertyValue().vec3Value.y == 1.0f, "vec3 undo Y should parse");
    runner.Expect(scaleAccessor.PropertyValue().vec3Value.z == 1.0f, "vec3 undo Z should parse");

    EditorPropertyValue enumValue{};
    enumValue.stringValue = "hero_landmark";
    RegressionPropertyAccessor enumAccessor(target, layerDescriptor->name, enumValue);
    EditorTransactionRecord enumRecord{};
    enumRecord.target = target;
    enumRecord.payload.kind = EditorTransactionPayloadKind::PropertyDelta;
    enumRecord.payload.propertyPath = layerDescriptor->name;
    enumRecord.payload.beforeSummary = "gameplay_collision";
    enumRecord.payload.afterSummary = "hero_landmark";
    const EditorPropertyApplyDeltaResult enumUndo =
        editService.ApplyDelta(
            EditorPropertyApplyDeltaRequest{
                &enumAccessor,
                &dirtyState,
                &notifications,
                &registry,
                &enumRecord,
                EditorTransactionApplyMode::Undo,
                true,
                true,
                "regression.propertyDelta"});
    runner.Expect(enumUndo.applied, "enum property delta undo should apply");
    runner.Expect(
        enumAccessor.PropertyValue().stringValue == "gameplay_collision",
        "enum undo should restore before string");

    EditorTransactionRecord invalidRecord{};
    invalidRecord.target = target;
    invalidRecord.payload.kind = EditorTransactionPayloadKind::PropertyDelta;
    invalidRecord.payload.propertyPath = descriptor->name;
    invalidRecord.payload.beforeSummary = "not-a-float";
    invalidRecord.payload.afterSummary = "125.000";
    const uint32_t notificationCountBeforeInvalid = static_cast<uint32_t>(notifications.Count());
    const EditorPropertyApplyDeltaResult invalidUndo =
        editService.ApplyDelta(
            EditorPropertyApplyDeltaRequest{
                &accessor,
                &dirtyState,
                &notifications,
                &registry,
                &invalidRecord,
                EditorTransactionApplyMode::Undo,
                true,
                true,
                "regression.propertyDelta"});
    runner.Expect(!invalidUndo.applied, "invalid property delta should fail");
    runner.Expect(
        notifications.Count() == notificationCountBeforeInvalid + 1,
        "invalid property delta should notify failure");

    const EditorPropertyApplyDeltaResult lockedDelta =
        editService.ApplyDelta(
            EditorPropertyApplyDeltaRequest{
                &accessor,
                &dirtyState,
                &notifications,
                &registry,
                &enumRecord,
                EditorTransactionApplyMode::Redo,
                false,
                true,
                "regression.propertyDelta"});
    runner.Expect(!lockedDelta.applied, "locked property delta should be rejected");
}

void TestProductionPropertyAdapters(RegressionRunner& runner) {
    EditorPropertyRegistry registry;
    RegisterBuiltInEditorProperties(registry);

    EffectSystem effectSystem;
    EffectAsset effectAsset{};
    effectAsset.name = "spark_core";
    effectAsset.texture = "spark_a";
    effectAsset.lifetime = 1.0f;
    effectAsset.defaultParticle.spawnFrequency = 12.0f;
    effectSystem.RegisterAsset(effectAsset);
    EffectRuntime effectRuntime(&effectSystem);

    PostProcessStack postProcessStack;
    postProcessStack.ResetToVfxDefaults();
    runner.Expect(!postProcessStack.Passes().empty(), "post-process defaults should provide editable passes");

    AppRuntimeState runtimeState;
    runtimeState.terrain.settings.chunkLength = 80.0f;
    CourseAsset course;
    course.cameraKeys.push_back(CourseCameraKey{});
    CourseEventMarker event{};
    event.distance = 120.0f;
    event.type = "spawn";
    event.id = "wave_alpha";
    event.payload = "count=3";
    course.events.push_back(event);

    EditorProductionPropertyAdapter productionAccessor(
        &effectRuntime,
        &postProcessStack,
        &runtimeState,
        &course);
    EditorCompositePropertyAccessor compositeAccessor;
    compositeAccessor.Add(&productionAccessor);

    EditorObjectHandle vfxHandle{};
    vfxHandle.domain = EditorDomainId::VfxEffectAsset;
    vfxHandle.stableId = "vfx-asset:spark_core";
    vfxHandle.displayName = "VFX Asset: spark_core";

    EditorObjectHandle postHandle{};
    postHandle.domain = EditorDomainId::PostProcessPass;
    postHandle.stableId = "post-process:" + postProcessStack.Passes().front().name;
    postHandle.localIndex = 0;
    postHandle.displayName = "PostProcess: " + postProcessStack.Passes().front().name;

    EditorObjectHandle cameraHandle{};
    cameraHandle.domain = EditorDomainId::CourseCameraKey;
    cameraHandle.stableId = BuildStableIndexedId("course-camera-key", 0);
    cameraHandle.localIndex = 0;
    cameraHandle.displayName = "Course Camera Key #0";

    EditorObjectHandle eventHandle{};
    eventHandle.domain = EditorDomainId::CourseEventMarker;
    eventHandle.stableId = BuildStableIndexedId("course-event", 0);
    eventHandle.localIndex = 0;
    eventHandle.displayName = "Course Event: wave_alpha";

    EditorObjectHandle terrainHandle{};
    terrainHandle.domain = EditorDomainId::TerrainGeneration;
    terrainHandle.stableId = BuildStableIndexedId("terrain-generation", 0);
    terrainHandle.displayName = "Terrain Generation";

    const EditorPropertyDescriptor* vfxLifetime =
        registry.Find(EditorDomainId::VfxEffectAsset, "VfxEffectAsset.lifetime");
    const EditorPropertyDescriptor* vfxTechnique =
        registry.Find(EditorDomainId::VfxEffectAsset, "VfxEffectAsset.technique");
    const EditorPropertyDescriptor* postIntensity =
        registry.Find(EditorDomainId::PostProcessPass, "PostProcessPass.intensity");
    const EditorPropertyDescriptor* postStage =
        registry.Find(EditorDomainId::PostProcessPass, "PostProcessPass.stage");
    const EditorPropertyDescriptor* cameraFov =
        registry.Find(EditorDomainId::CourseCameraKey, "CourseCameraKey.fov");
    const EditorPropertyDescriptor* eventPayload =
        registry.Find(EditorDomainId::CourseEventMarker, "CourseEventMarker.payload");
    const EditorPropertyDescriptor* terrainChunkLength =
        registry.Find(EditorDomainId::TerrainGeneration, "TerrainGeneration.chunkLength");
    runner.Expect(vfxLifetime != nullptr, "vfx lifetime descriptor should exist");
    runner.Expect(vfxTechnique != nullptr, "vfx technique descriptor should exist");
    runner.Expect(postIntensity != nullptr, "post-process intensity descriptor should exist");
    runner.Expect(postStage != nullptr, "post-process stage descriptor should exist");
    runner.Expect(cameraFov != nullptr, "camera fov descriptor should exist");
    runner.Expect(eventPayload != nullptr, "course event payload descriptor should exist");
    runner.Expect(terrainChunkLength != nullptr, "terrain chunk length descriptor should exist");

    EditorTransactionStack transactions;
    EditorDirtyStateService dirtyState;
    EditorNotificationCenter notifications;
    EditorPropertyEditService editService;

    EditorPropertyValue requested{};
    requested.floatValue = 2.5f;
    EditorPropertyEditResult result =
        editService.Apply(
            EditorPropertyEditRequest{
                &compositeAccessor,
                &transactions,
                &dirtyState,
                &notifications,
                vfxHandle,
                vfxLifetime,
                requested,
                true,
                true,
                "regression.productionProperty"});
    runner.Expect(result.applied && result.changed, "vfx lifetime should edit through production accessor");
    runner.Expect(
        std::fabs(effectRuntime.Assets().at("spark_core").lifetime - 2.5f) < 0.001f,
        "vfx lifetime should mutate the effect asset");
    runner.Expect(
        dirtyState.HasDirtyDomain(EditorDirtyDomain::Property),
        "vfx edits should dirty generic property state instead of course authoring");

    requested.floatValue = 0.35f;
    result =
        editService.Apply(
            EditorPropertyEditRequest{
                &compositeAccessor,
                &transactions,
                &dirtyState,
                &notifications,
                postHandle,
                postIntensity,
                requested,
                true,
                true,
                "regression.productionProperty"});
    runner.Expect(result.applied && result.changed, "post-process intensity should edit through production accessor");
    runner.Expect(
        std::fabs(postProcessStack.Passes().front().intensity - 0.35f) < 0.001f,
        "post-process intensity should mutate the production pass");

    requested.floatValue = 60.0f;
    result =
        editService.Apply(
            EditorPropertyEditRequest{
                &compositeAccessor,
                &transactions,
                &dirtyState,
                &notifications,
                cameraHandle,
                cameraFov,
                requested,
                true,
                true,
                "regression.productionProperty"});
    runner.Expect(result.applied && result.changed, "course camera fov should edit through production accessor");
    runner.Expect(
        std::fabs(course.cameraKeys.front().fovY - (60.0f * 3.14159265358979323846f / 180.0f)) < 0.001f,
        "camera fov should be stored in radians");
    runner.Expect(
        dirtyState.HasDirtyDomain(EditorDirtyDomain::CourseAuthoring),
        "course camera edits should dirty course authoring");

    EditorPropertyValue eventValue{};
    eventValue.stringValue = "count=5 route=left";
    result =
        editService.Apply(
            EditorPropertyEditRequest{
                &compositeAccessor,
                &transactions,
                &dirtyState,
                &notifications,
                eventHandle,
                eventPayload,
                eventValue,
                true,
                true,
                "regression.productionProperty"});
    runner.Expect(result.applied && result.changed, "course event payload should edit through production accessor");
    runner.Expect(
        course.events.front().payload == "count=5 route=left",
        "course event payload should mutate the course asset");
    runner.Expect(
        dirtyState.HasDirtyDomain(EditorDirtyDomain::CourseAuthoring),
        "course event edits should dirty course authoring");

    requested.floatValue = 192.0f;
    result =
        editService.Apply(
            EditorPropertyEditRequest{
                &compositeAccessor,
                &transactions,
                &dirtyState,
                &notifications,
                terrainHandle,
                terrainChunkLength,
                requested,
                true,
                true,
                "regression.productionProperty"});
    runner.Expect(result.applied && result.changed, "terrain generation should edit through production accessor");
    runner.Expect(
        std::fabs(runtimeState.terrain.settings.chunkLength - 192.0f) < 0.001f,
        "terrain generation settings should mutate runtime authoring state");

    EditorPropertyValue enumValue{};
    enumValue.stringValue = "trail";
    const EditorPropertyEditResult readOnlyTechnique =
        editService.Apply(
            EditorPropertyEditRequest{
                &compositeAccessor,
                &transactions,
                &dirtyState,
                &notifications,
                vfxHandle,
                vfxTechnique,
                enumValue,
                true,
                true,
                "regression.productionProperty"});
    runner.Expect(!readOnlyTechnique.applied, "read-only vfx technique edits should be rejected");
    runner.Expect(
        readOnlyTechnique.message.find("typed VFX components") != std::string::npos,
        "read-only vfx technique should report the production replacement path");

    const EditorPropertyEditResult readOnlyStage =
        editService.Apply(
            EditorPropertyEditRequest{
                &compositeAccessor,
                &transactions,
                &dirtyState,
                &notifications,
                postHandle,
                postStage,
                enumValue,
                true,
                true,
                "regression.productionProperty"});
    runner.Expect(!readOnlyStage.applied, "read-only post-process stage edits should be rejected");
    runner.Expect(
        readOnlyStage.message.find("production post-process pipeline") != std::string::npos,
        "read-only post-process stage should report derived pipeline ownership");
}

void TestDetailsSectionProviders(RegressionRunner& runner) {
    EditorDetailsSectionProviderRegistry providers;
    RegisterBuiltInEditorDetailsSectionProviders(providers);

    runner.Expect(providers.Count() >= 4, "built-in details section providers should register");
    runner.Expect(
        !providers.FindByDomain(EditorDomainId::VfxEffectAsset).empty(),
        "vfx effect assets should expose a custom details section");
    runner.Expect(
        !providers.FindByDomain(EditorDomainId::PostProcessPass).empty(),
        "post-process passes should expose a custom details section");
    runner.Expect(
        !providers.FindByDomain(EditorDomainId::CourseEventMarker).empty(),
        "course event markers should expose a custom details section");
    runner.Expect(
        !providers.FindByDomain(EditorDomainId::RenderPreset).empty(),
        "render presets should expose a custom details section");
    runner.Expect(
        providers.FindByDomain(EditorDomainId::CourseTerrainPlacement).empty(),
        "domains without custom sections should remain on generic details coverage");
}

bool HasRuntimeWatchRecord(
    const EditorRuntimeInspector& inspector,
    std::string_view domain,
    std::string_view displayName) {
    for (const EditorRuntimeWatchRecord& record : inspector.Records()) {
        if (record.domain == domain && record.displayName == displayName) {
            return true;
        }
    }
    return false;
}

void TestRuntimeWatchBuilder(RegressionRunner& runner) {
    EditorRuntimeInspector inspector;
    EditorPlaySessionState playSession;
    EditorPlaySessionIsolationSnapshot snapshot;
    EditorPlaySessionLifecycleService lifecycle;
    EditorRailRuntimePause railPause;
    EditorSelection selection;
    EffectRuntime effectRuntime;
    std::vector<LoadedEffectAsset> loadedEffects;
    CourseAsset course;
    CourseSection section{};
    section.name = "intro";
    section.category = "warmup";
    section.startDistance = 0.0f;
    section.endDistance = 80.0f;
    course.sections.push_back(section);
    CourseEventMarker event{};
    event.id = "watch_event";
    event.type = "spawn";
    course.events.push_back(event);
    AppRuntimeState runtimeState;
    runtimeState.terrain.previewDistance = 12.0f;
    runtimeState.terrain.courseObjectEditRevision = 7;
    CourseSpawnRuntime spawnRuntime;
    CourseCollisionSystem collisionSystem;
    SectionCheckpointSystem checkpointSystem;
    PlayerCombatFeelSystem combatFeelSystem;
    checkpointSystem.Reset(&course, 12.0f);
    checkpointSystem.Update(&course, 12.0f);
    const EditorPlaySessionLifecycleRequest lifecycleRequest{
        &playSession,
        &snapshot,
        &course,
        &runtimeState,
        nullptr,
        "regression.runtimeWatch"};
    runner.Expect(
        lifecycle.Begin(lifecycleRequest, EditorPlaySessionMode::Simulating).succeeded,
        "runtime watch test should begin simulate session");
    playSession.PauseRuntime();
    railPause.Sync(
        EditorRailRuntimePauseInput{
            true,
            true,
            12.0f,
            0.0f});
    selection.SetPrimary(
        EditorObjectHandle{
            EditorDomainId::CourseTerrainPlacement,
            "course-terrain:0",
            0,
            7,
            "Course Terrain #0"});
    std::vector<ge3::graphics::RenderPassDebugInfo> passes;
    ge3::graphics::RenderPassDebugInfo pass{};
    pass.name = "Main Scene";
    pass.executed = true;
    pass.executionIndex = 3;
    pass.reason = "required";
    passes.push_back(pass);
    const std::string renderGraphDescription = "Main Scene";
    const std::string renderGraphError;

    BuildEditorRuntimeWatch(
        EditorRuntimeWatchBuildInput{
            &inspector,
            &effectRuntime,
            &loadedEffects,
            0,
            &playSession,
            &snapshot,
            &railPause,
            &selection,
            &runtimeState,
            &course,
            &spawnRuntime,
            &collisionSystem,
            &checkpointSystem,
            &combatFeelSystem,
            &renderGraphDescription,
            &renderGraphError,
            &passes,
            2,
            4,
            3,
            8,
            12.0f,
            0.0f,
            80.0f});

    runner.Expect(inspector.ReadOnly(), "runtime watch builder should preserve read-only inspector semantics");
    runner.Expect(
        HasRuntimeWatchRecord(inspector, "Editor", "Play Session"),
        "runtime watch should include play session state");
    runner.Expect(
        HasRuntimeWatchRecord(inspector, "Editor", "Selection"),
        "runtime watch should include selected object state");
    runner.Expect(
        HasRuntimeWatchRecord(inspector, "Course Runtime", "Course Director"),
        "runtime watch should include course director state");
    runner.Expect(
        HasRuntimeWatchRecord(inspector, "Course Runtime", "Section Checkpoint"),
        "runtime watch should include checkpoint state");
    runner.Expect(
        HasRuntimeWatchRecord(inspector, "VFX", "Effect Runtime"),
        "runtime watch should include VFX runtime state");
    runner.Expect(
        HasRuntimeWatchRecord(inspector, "Gameplay", "Spawn Runtime"),
        "runtime watch should include spawn runtime state");
    runner.Expect(
        HasRuntimeWatchRecord(inspector, "Gameplay", "Collision System"),
        "runtime watch should include collision system state");
    runner.Expect(
        HasRuntimeWatchRecord(inspector, "Gameplay", "Combat Feel"),
        "runtime watch should include combat feel state");
    runner.Expect(
        HasRuntimeWatchRecord(inspector, "RenderGraph", "Pass Summary") &&
            HasRuntimeWatchRecord(inspector, "RenderGraph", "Main Scene"),
        "runtime watch should include RenderGraph summary and executed pass rows");
}

void TestPlaySessionLifecycleService(RegressionRunner& runner) {
    EditorPlaySessionState playSession;
    EditorPlaySessionIsolationSnapshot snapshot;
    EditorPlaySessionLifecycleService lifecycle;
    EditorNotificationCenter notifications;
    CourseAsset course;
    CourseEventMarker event{};
    event.id = "runtime_event";
    event.payload = "authoring";
    course.events.push_back(event);
    AppRuntimeState runtimeState;
    runtimeState.terrain.previewSpeed = 40.0f;

    const EditorPlaySessionLifecycleRequest request{
        &playSession,
        &snapshot,
        &course,
        &runtimeState,
        &notifications,
        "regression.playSessionLifecycle"};

    const EditorPlaySessionLifecycleResult beginResult =
        lifecycle.Begin(request, EditorPlaySessionMode::Simulating);
    runner.Expect(beginResult.succeeded, "play lifecycle begin should capture authoring snapshot");
    runner.Expect(playSession.IsSimulating(), "play lifecycle begin should enter simulate mode");
    runner.Expect(
        playSession.RuntimeIsolationSnapshotActive() && snapshot.Captured(),
        "play lifecycle begin should mark snapshot isolation active");
    runner.Expect(
        snapshot.SessionSerial() == playSession.SessionSerial(),
        "play lifecycle snapshot should bind to the active session");

    const EditorPlaySessionLifecycleResult nestedBegin =
        lifecycle.Begin(request, EditorPlaySessionMode::Playing);
    runner.Expect(!nestedBegin.succeeded, "play lifecycle should reject nested begin");

    course.events.front().payload = "runtime-mutated";
    runtimeState.terrain.previewSpeed = 120.0f;
    playSession.TickFrame();
    runner.Expect(playSession.FrameCount() == 1, "active play lifecycle should tick frame");

    const EditorPlaySessionLifecycleResult stopResult = lifecycle.Stop(request);
    runner.Expect(stopResult.succeeded, "play lifecycle stop should restore authoring snapshot");
    runner.Expect(playSession.IsStopped(), "play lifecycle stop should return to stopped");
    runner.Expect(playSession.RuntimeIsolationRestored(), "play lifecycle stop should mark restored state");
    runner.Expect(snapshot.Restored(), "play lifecycle stop should mark snapshot restored");
    runner.Expect(
        course.events.front().payload == "authoring",
        "play lifecycle stop should restore course authoring data");
    runner.Expect(
        std::fabs(runtimeState.terrain.previewSpeed - 40.0f) < 0.001f,
        "play lifecycle stop should restore runtime authoring state");

    EditorPlaySessionState missingSnapshotSession;
    EditorPlaySessionIsolationSnapshot missingSnapshot;
    AppRuntimeState missingRuntime;
    CourseAsset missingCourse;
    const EditorPlaySessionLifecycleRequest missingSnapshotRequest{
        &missingSnapshotSession,
        &missingSnapshot,
        &missingCourse,
        &missingRuntime,
        &notifications,
        "regression.playSessionLifecycle"};
    const EditorPlaySessionLifecycleResult missingSnapshotBegin =
        lifecycle.Begin(missingSnapshotRequest, EditorPlaySessionMode::Playing);
    runner.Expect(missingSnapshotBegin.succeeded, "play lifecycle missing snapshot setup should begin");
    missingSnapshot.Clear();
    const EditorPlaySessionLifecycleResult missingSnapshotStop =
        lifecycle.Stop(missingSnapshotRequest);
    runner.Expect(!missingSnapshotStop.succeeded, "play lifecycle stop should require a captured snapshot");
}

void TestPlaySessionRuntimeControlService(RegressionRunner& runner) {
    EditorPlaySessionState playSession;
    EditorPlaySessionIsolationSnapshot snapshot;
    EditorPlaySessionLifecycleService lifecycle;
    EditorPlaySessionRuntimeControlService runtimeControl;
    EditorNotificationCenter notifications;
    CourseAsset course;
    CourseEventMarker event{};
    event.id = "runtime_control_event";
    event.payload = "authoring";
    course.events.push_back(event);
    AppRuntimeState runtimeState;
    runtimeState.terrain.previewSpeed = 32.0f;

    const EditorPlaySessionLifecycleRequest lifecycleRequest{
        &playSession,
        &snapshot,
        &course,
        &runtimeState,
        &notifications,
        "regression.runtimeControl.lifecycle"};
    runner.Expect(
        lifecycle.Begin(lifecycleRequest, EditorPlaySessionMode::Simulating).succeeded,
        "runtime control test should begin simulate session");

    const EditorPlaySessionRuntimeControlRequest controlRequest{
        &playSession,
        &snapshot,
        &course,
        &runtimeState,
        &notifications,
        "regression.runtimeControl"};
    const EditorPlaySessionRuntimeControlResult pauseResult =
        runtimeControl.Pause(controlRequest);
    runner.Expect(pauseResult.succeeded, "runtime control should pause an active session");
    runner.Expect(playSession.RuntimePaused(), "runtime control pause should mark runtime paused");
    runner.Expect(!playSession.ShouldAdvanceRuntimeFrame(), "paused runtime should block frame advance");

    const EditorPlaySessionRuntimeControlResult stepResult =
        runtimeControl.Step(controlRequest);
    runner.Expect(stepResult.succeeded, "runtime control should queue a single step");
    runner.Expect(playSession.ShouldAdvanceRuntimeFrame(), "queued step should allow one runtime frame");
    playSession.CompleteRuntimeFrameAdvance();
    runner.Expect(playSession.RuntimeFrameCount() == 1, "single step should advance one runtime frame");
    runner.Expect(playSession.RuntimePaused(), "single step should return to paused state");
    runner.Expect(!playSession.RuntimeStepRequested(), "single step should consume the step request");

    const EditorPlaySessionRuntimeControlResult resumeResult =
        runtimeControl.Resume(controlRequest);
    runner.Expect(resumeResult.succeeded, "runtime control should resume an active session");
    runner.Expect(!playSession.RuntimePaused(), "runtime control resume should clear paused state");
    runner.Expect(playSession.ShouldAdvanceRuntimeFrame(), "resumed runtime should advance frames");

    course.events.front().payload = "runtime-mutated";
    runtimeState.terrain.previewSpeed = 128.0f;
    const EditorPlaySessionRuntimeControlResult resetResult =
        runtimeControl.ResetRuntime(controlRequest);
    runner.Expect(resetResult.succeeded, "runtime control reset should restore the snapshot");
    runner.Expect(playSession.RuntimePaused(), "runtime control reset should leave runtime paused");
    runner.Expect(playSession.RuntimeFrameCount() == 0, "runtime control reset should reset runtime frame count");
    runner.Expect(playSession.RuntimeResetCount() == 1, "runtime control reset should record reset count");
    runner.Expect(
        course.events.front().payload == "authoring",
        "runtime control reset should restore course snapshot data");
    runner.Expect(
        std::fabs(runtimeState.terrain.previewSpeed - 32.0f) < 0.001f,
        "runtime control reset should restore terrain snapshot data");

    const EditorPlaySessionLifecycleResult stopResult = lifecycle.Stop(lifecycleRequest);
    runner.Expect(stopResult.succeeded, "runtime control test should still stop through lifecycle service");
}

void TestRuntimeAuthoringApplyService(RegressionRunner& runner) {
    EditorPlaySessionState playSession;
    EditorPlaySessionIsolationSnapshot snapshot;
    EditorPlaySessionLifecycleService lifecycle;
    EditorRuntimeAuthoringApplyService runtimeApply;
    EditorTransactionStack transactions;
    EditorDirtyStateService dirtyState;
    EditorNotificationCenter notifications;
    CourseAsset course;
    CourseEventMarker event{};
    event.id = "runtime_apply_event";
    event.payload = "authoring";
    course.events.push_back(event);
    AppRuntimeState runtimeState;
    runtimeState.terrain.previewSpeed = 40.0f;

    const EditorPlaySessionLifecycleRequest lifecycleRequest{
        &playSession,
        &snapshot,
        &course,
        &runtimeState,
        &notifications,
        "regression.runtimeApply.lifecycle"};
    runner.Expect(
        lifecycle.Begin(lifecycleRequest, EditorPlaySessionMode::Simulating).succeeded,
        "runtime apply test should begin simulate session");

    course.events.front().payload = "applied-runtime";
    runtimeState.terrain.previewSpeed = 88.0f;
    const EditorRuntimeAuthoringApplyRequest applyRequest{
        &playSession,
        &snapshot,
        &course,
        &runtimeState,
        &transactions,
        &dirtyState,
        &notifications,
        0,
        "regression.runtimeApply"};
    const EditorRuntimeAuthoringApplyResult applyResult = runtimeApply.Apply(applyRequest);
    runner.Expect(applyResult.succeeded && applyResult.changed, "runtime apply should accept changed runtime state");
    runner.Expect(
        dirtyState.HasDirtyDomain(EditorDirtyDomain::CourseAuthoring),
        "runtime apply should dirty course authoring");
    const EditorTransactionRecord* lastTransaction = transactions.LastTransaction();
    runner.Expect(
        lastTransaction != nullptr &&
            lastTransaction->payload.kind == EditorTransactionPayloadKind::RuntimeAuthoringApply,
        "runtime apply should push a runtime authoring transaction");
    runner.Expect(
        snapshot.CapturedTerrain() != nullptr &&
            std::fabs(snapshot.CapturedTerrain()->previewSpeed - 88.0f) < 0.001f,
        "runtime apply should adopt applied state as the new restore snapshot");

    course.events.front().payload = "not-applied-runtime";
    runtimeState.terrain.previewSpeed = 144.0f;
    const EditorPlaySessionLifecycleResult stopResult = lifecycle.Stop(lifecycleRequest);
    runner.Expect(stopResult.succeeded, "runtime apply stop should restore to latest applied snapshot");
    runner.Expect(course.events.front().payload == "applied-runtime", "stop should keep applied runtime course changes");
    runner.Expect(
        std::fabs(runtimeState.terrain.previewSpeed - 88.0f) < 0.001f,
        "stop should keep applied runtime terrain changes");

    bool undoApplied = false;
    runner.Expect(
        transactions.Undo(
            [&](const EditorTransactionRecord& record, EditorTransactionApplyMode mode) {
                const EditorRuntimeAuthoringApplyResult result =
                    runtimeApply.ApplyTransaction(
                        EditorRuntimeAuthoringApplyRequest{
                            nullptr,
                            nullptr,
                            &course,
                            &runtimeState,
                            &transactions,
                            &dirtyState,
                            &notifications,
                            0,
                            "regression.runtimeApply.undo"},
                        record,
                        mode);
                undoApplied = result.succeeded && course.events.front().payload == "authoring";
                return result.succeeded;
            }),
        "runtime apply transaction undo should run");
    runner.Expect(undoApplied, "runtime apply undo should restore original authoring data");

    bool redoApplied = false;
    runner.Expect(
        transactions.Redo(
            [&](const EditorTransactionRecord& record, EditorTransactionApplyMode mode) {
                const EditorRuntimeAuthoringApplyResult result =
                    runtimeApply.ApplyTransaction(
                        EditorRuntimeAuthoringApplyRequest{
                            nullptr,
                            nullptr,
                            &course,
                            &runtimeState,
                            &transactions,
                            &dirtyState,
                            &notifications,
                            0,
                            "regression.runtimeApply.redo"},
                        record,
                        mode);
                redoApplied = result.succeeded && course.events.front().payload == "applied-runtime";
                return result.succeeded;
            }),
        "runtime apply transaction redo should run");
    runner.Expect(redoApplied, "runtime apply redo should restore applied runtime data");

    runner.Expect(
        lifecycle.Begin(lifecycleRequest, EditorPlaySessionMode::Simulating).succeeded,
        "runtime apply validation test should begin a new simulate session");
    course.events.front().payload = "blocked-by-validation";
    runner.Expect(
        !runtimeApply.Apply(
            EditorRuntimeAuthoringApplyRequest{
                &playSession,
                &snapshot,
                &course,
                &runtimeState,
                &transactions,
                &dirtyState,
                &notifications,
                1,
                "regression.runtimeApply.validation"}).succeeded,
        "runtime apply should reject validation errors");
}

void TestAssetRegistryAndMutationSafety(RegressionRunner& runner) {
    std::string assetStage = "asset setup";
    try {
    EditorAssetRegistry registry;

    EditorAssetRecord mesh = MakeAsset(
        EditorAssetKind::Mesh,
        "mesh_rock_a",
        "Resources/meshes/rock_a.obj",
        true,
        "guid-mesh-rock-a");
    EditorAssetRecord course = MakeAsset(
        EditorAssetKind::Course,
        "course_alpha",
        "Resources/courses/course_alpha.json",
        true,
        "guid-course-alpha");
    course.dependencies.push_back("Mesh:mesh_rock_a");
    EditorAssetRecord legacyTexture = MakeAsset(
        EditorAssetKind::Texture,
        "texture_legacy",
        "Resources/textures/legacy.png",
        false);

    assetStage = "asset registration";
    runner.Expect(registry.Register(mesh), "mesh registration should succeed");
    runner.Expect(registry.Register(course), "course registration should succeed");
    runner.Expect(registry.Register(legacyTexture), "legacy texture registration should succeed");
    runner.Expect(registry.Count() == 3, "asset registry should track all records");
    runner.Expect(registry.FindByGuid("guid-mesh-rock-a") != nullptr, "GUID lookup should find mesh");
    runner.Expect(registry.CountWithDependencies() == 1, "dependency count should include course");
    runner.Expect(registry.CountWithProvisionalGuid() == 1, "legacy texture should receive provisional GUID");

    assetStage = "asset dependency graph";
    const EditorAssetRecord* meshRecord = registry.Find(EditorAssetKind::Mesh, "mesh_rock_a");
    runner.Expect(meshRecord != nullptr, "mesh record should be findable");
    runner.Expect(
        BuildEditorAssetDependencyToken(*meshRecord) == "Mesh:mesh_rock_a",
        "asset dependency token should be stable");
    EditorAssetDependencyToken parsedToken{};
    runner.Expect(
        ParseEditorAssetDependencyToken("Mesh:mesh_rock_a", parsedToken),
        "asset dependency token should parse");
    runner.Expect(
        parsedToken.kind == EditorAssetKind::Mesh && parsedToken.id == "mesh_rock_a",
        "parsed asset dependency token should preserve kind and id");
    runner.Expect(
        registry.FindDependents(*meshRecord).size() == 1,
        "dependency graph should find mesh dependents");
    runner.Expect(
        registry.FindDependencies(*registry.Find(EditorAssetKind::Course, "course_alpha")).size() == 1,
        "dependency graph should find course dependencies");
    runner.Expect(
        registry.CountDependents(*meshRecord) == 1,
        "dependency graph should count mesh dependents");
    assetStage = "asset mutation safety";
    const EditorAssetMutationSafetyReport moveReport =
        EvaluateEditorAssetMutationSafety(registry, *meshRecord, EditorAssetMutationKind::Move);
    runner.Expect(!moveReport.Blocked(), "move should allow durable assets with indexed dependents");
    runner.Expect(moveReport.HasWarnings(), "move should warn before rewriting indexed dependents");
    runner.Expect(moveReport.dependentCount == 1, "move report should include dependent count");

    const EditorAssetRecord* textureRecord = registry.Find(EditorAssetKind::Texture, "texture_legacy");
    runner.Expect(textureRecord != nullptr, "legacy texture record should be findable");
    const EditorAssetMutationSafetyReport renameReport =
        EvaluateEditorAssetMutationSafety(registry, *textureRecord, EditorAssetMutationKind::Rename);
    runner.Expect(renameReport.Blocked(), "rename should block without durable .meta GUID");

    assetStage = "asset durable rename safety";
    EditorAssetRecord audio = MakeAsset(
        EditorAssetKind::Audio,
        "audio_ok",
        "Resources/audio/ok.wav",
        true,
        "guid-audio-ok");
    runner.Expect(registry.Register(audio), "audio registration should succeed");
    const EditorAssetMutationSafetyReport safeRename =
        EvaluateEditorAssetMutationSafety(
            registry,
            *registry.Find(EditorAssetKind::Audio, "audio_ok"),
            EditorAssetMutationKind::Rename);
    runner.Expect(!safeRename.Blocked(), "durable asset without dependents should allow rename");

    assetStage = "asset broken dependency diagnostics";
    EditorAssetRecord brokenCourse = MakeAsset(
        EditorAssetKind::Course,
        "course_broken",
        "Resources/courses/course_broken.json",
        true,
        "guid-course-broken");
    brokenCourse.dependencies.push_back("Mesh:not_registered");
    brokenCourse.dependencies.push_back("bad-token");
    runner.Expect(registry.Register(brokenCourse), "broken course registration should succeed");
    const EditorAssetRecord* brokenRecord = registry.Find(EditorAssetKind::Course, "course_broken");
    runner.Expect(brokenRecord != nullptr, "broken course record should be findable");
    runner.Expect(
        registry.FindMissingDependencyTokens(*brokenRecord).size() == 1,
        "dependency graph should expose missing dependencies");
    runner.Expect(
        registry.FindMalformedDependencyTokens(*brokenRecord).size() == 1,
        "dependency graph should expose malformed dependencies");

    std::string thumbnailStage = "thumbnail setup";
    try {
        thumbnailStage = "thumbnail texture pixel decode";
        const std::filesystem::path thumbnailDecodeRoot = "logs/__editor_thumbnail_decode_regression";
        RemoveTreeIfPresent(thumbnailDecodeRoot);
        const std::filesystem::path thumbnailDecodeTexture = thumbnailDecodeRoot / "decode_texture.tga";
        WriteBinaryFile(thumbnailDecodeTexture, MakeTgaPreviewImage(16, 8));
        const std::filesystem::path thumbnailMeshObj = thumbnailDecodeRoot / "preview_mesh.obj";
        const std::filesystem::path thumbnailMeshMtl = thumbnailDecodeRoot / "preview_mesh.mtl";
        WriteTextFile(
            thumbnailMeshMtl,
            "newmtl warm_preview\n"
            "Kd 0.8 0.4 0.2\n"
            "map_Kd decode_texture.tga\n");
        WriteTextFile(
            thumbnailMeshObj,
            "mtllib preview_mesh.mtl\n"
            "v -1.0 0.0 -0.5\n"
            "v 1.0 0.0 -0.5\n"
            "v 0.0 1.5 0.5\n"
            "usemtl warm_preview\n"
            "f 1 2 3\n");
        EditorAssetRecord previewMeshRecord = MakeAsset(
            EditorAssetKind::Mesh,
            "preview_mesh_obj",
            thumbnailMeshObj.generic_string(),
            true,
            "guid-preview-mesh-obj");
        const EditorAssetPreviewInfo previewMeshInfo =
            EditorAssetPreviewProvider{}.BuildPreview(previewMeshRecord);
        runner.Expect(
            previewMeshInfo.readiness == EditorAssetPreviewReadiness::Ready &&
                previewMeshInfo.hasPreviewGeometry &&
                previewMeshInfo.hasMaterialBinding,
            "OBJ mesh preview should bind real geometry bounds and material metadata");
        runner.Expect(
            previewMeshInfo.vertexCount == 3 &&
                previewMeshInfo.faceCount == 1 &&
                previewMeshInfo.materialSlotCount == 1 &&
                previewMeshInfo.materialTextureCount == 1 &&
                previewMeshInfo.materialTextureTimestamp != 0 &&
                previewMeshInfo.boundsRadius > 0.0f &&
                previewMeshInfo.previewCameraDistance > previewMeshInfo.boundsRadius,
            "OBJ mesh preview should expose bounds, material texture stamp, and camera preset");
        EditorAssetThumbnailPixelData decodedPixels;
        std::string decodeError;
        runner.Expect(
            LoadEditorAssetTextureThumbnailPixels(
                thumbnailDecodeTexture.generic_string(),
                8,
                decodedPixels,
                decodeError),
            "texture thumbnail loader should decode valid TGA pixels");
        runner.Expect(
            decodedPixels.width == 8 &&
                decodedPixels.height == 4 &&
                decodedPixels.rowPitch == 32 &&
                decodedPixels.rgba8.size() == 128,
            "texture thumbnail loader should resize and normalize pixels to RGBA8");

        EditorAssetThumbnailCachePolicy cachePolicy{};
        cachePolicy.persistentCacheEnabled = true;
        cachePolicy.persistentRoot = thumbnailDecodeRoot / "cache";
        cachePolicy.maxMemoryBytes = 1024 * 1024;
        EditorAssetThumbnailCacheStore cacheStore;
        cacheStore.Configure(cachePolicy);
        EditorAssetGpuThumbnailAllocationRequest cacheRequest{};
        cacheRequest.key = "thumb:Texture:cache_regression";
        cacheRequest.kind = EditorAssetKind::Texture;
        cacheRequest.previewKind = EditorAssetPreviewKind::Texture;
        cacheRequest.assetId = "cache_regression";
        cacheRequest.sourcePath = thumbnailDecodeTexture.generic_string();
        cacheRequest.sourceTimestamp = 77;
        cacheRequest.previewGeneration = 2;
        cacheRequest.width = decodedPixels.width;
        cacheRequest.height = decodedPixels.height;
        std::string cacheDetail;
        const EditorAssetThumbnailCacheKey cacheKey =
            BuildEditorAssetThumbnailCacheKey(cacheRequest, cachePolicy.previewVersion);
        runner.Expect(
            cacheStore.Store(cacheKey, decodedPixels, false, cacheDetail),
            "thumbnail cache store should persist a valid pixel payload");
        EditorAssetThumbnailPixelData cacheHitPixels;
        runner.Expect(
            cacheStore.TryLoad(cacheKey, cacheHitPixels, cacheDetail) &&
                cacheHitPixels.rgba8 == decodedPixels.rgba8,
            "thumbnail cache should return a memory hit for stored payloads");
        runner.Expect(
            cacheStore.Telemetry().hits == 1 &&
                cacheStore.Telemetry().stores == 1,
            "thumbnail cache telemetry should count memory hits and stores");

        EditorAssetThumbnailCacheStore warmCacheStore;
        warmCacheStore.Configure(cachePolicy);
        EditorAssetThumbnailPixelData diskHitPixels;
        runner.Expect(
            warmCacheStore.TryLoad(cacheKey, diskHitPixels, cacheDetail) &&
                diskHitPixels.rgba8 == decodedPixels.rgba8,
            "thumbnail cache should return a disk hit for persistent payloads");

        EditorAssetThumbnailCachePolicy evictionPolicy{};
        evictionPolicy.maxMemoryBytes = 64;
        EditorAssetThumbnailCacheStore evictionCache;
        evictionCache.Configure(evictionPolicy);
        runner.Expect(
            evictionCache.Store(cacheKey, decodedPixels, false, cacheDetail),
            "thumbnail cache should accept payloads before applying eviction policy");
        runner.Expect(
            evictionCache.Telemetry().evictions > 0 &&
                evictionCache.Telemetry().residentEntries == 0,
            "thumbnail cache should evict least-recent entries when memory policy is exceeded");

        EditorThumbnailUploadRetirementQueue uploadRetirement;
        uploadRetirement.EnqueueForTesting(4096, 12);
        runner.Expect(
            uploadRetirement.Telemetry().pendingCount == 1 &&
                uploadRetirement.Telemetry().pendingBytes == 4096,
            "thumbnail upload retirement queue should retain pending uploads before fence completion");
        runner.Expect(
            uploadRetirement.RetireCompleted(11) == 0 &&
                uploadRetirement.Telemetry().pendingCount == 1,
            "thumbnail upload retirement queue should not retire before completed fence reaches target");
        runner.Expect(
            uploadRetirement.RetireCompleted(12) == 1 &&
                uploadRetirement.Telemetry().pendingCount == 0 &&
                uploadRetirement.Telemetry().retiredBytes == 4096,
            "thumbnail upload retirement queue should retire uploads after completed fence reaches target");

        EditorAssetPreviewRenderTargetPool nullPreviewTargetPool;
        EditorAssetPreviewRenderTargetAllocation nullPreviewTargetAllocation{};
        std::string nullPreviewTargetError;
        runner.Expect(
            !nullPreviewTargetPool.Allocate(
                128,
                128,
                nullPreviewTargetAllocation,
                nullPreviewTargetError),
            "preview render target pool should reject allocation before D3D12 initialization");

        EditorAssetGpuThumbnailAllocationRequest meshPreviewRequest{};
        meshPreviewRequest.key = "mesh-preview-regression";
        meshPreviewRequest.kind = EditorAssetKind::Mesh;
        meshPreviewRequest.previewKind = EditorAssetPreviewKind::Mesh;
        meshPreviewRequest.vertexCount = 128;
        meshPreviewRequest.faceCount = 64;
        meshPreviewRequest.materialSlotCount = 2;
        meshPreviewRequest.boundsRadius = 1.75f;
        meshPreviewRequest.previewCameraDistance = 4.9f;
        meshPreviewRequest.previewLightDirection[0] = 0.38f;
        meshPreviewRequest.previewLightDirection[1] = -0.82f;
        meshPreviewRequest.previewLightDirection[2] = 0.42f;
        meshPreviewRequest.hasPreviewGeometry = true;
        meshPreviewRequest.hasMaterialBinding = true;
        meshPreviewRequest.swatchRgba = 0xff4477aau;
        EditorAssetThumbnailPixelData meshPreviewPixels;
        std::string meshPreviewError;
        std::string previewSceneDetail;
        runner.Expect(
            RenderEditorAssetPreviewScenePass(
                meshPreviewRequest,
                meshPreviewPixels,
                previewSceneDetail,
                meshPreviewError),
            "engine-scene mesh/material preview pass should generate pixels");
        runner.Expect(
            meshPreviewPixels.width == 128 &&
                meshPreviewPixels.height == 128 &&
                meshPreviewPixels.rowPitch == 512 &&
                meshPreviewPixels.rgba8.size() == 65536,
            "engine-scene mesh/material preview pass should produce a stable RGBA8 payload");
        runner.Expect(
            previewSceneDetail.find("material slots 2") != std::string::npos &&
                previewSceneDetail.find("camera") != std::string::npos,
            "engine-scene mesh/material preview pass should report material and camera binding");

        EditorAssetThumbnailPixelData fallbackIconPixels;
        runner.Expect(
            BuildEditorAssetFallbackIconPixels(
                EditorAssetKind::Audio,
                EditorAssetPreviewKind::Audio,
                0xff335577u,
                fallbackIconPixels),
            "fallback icon atlas should generate production fallback icon pixels");
        runner.Expect(
            fallbackIconPixels.width == 96 &&
                fallbackIconPixels.height == 96 &&
                fallbackIconPixels.rowPitch == 384 &&
                fallbackIconPixels.rgba8.size() == 36864,
            "fallback icon atlas should produce a stable RGBA8 tile payload");
        RemoveTreeIfPresent(thumbnailDecodeRoot);

        EditorAssetThumbnailService thumbnails;
        FakeThumbnailGpuBackend fakeGpuBackend;
        thumbnails.SetGpuThumbnailBackend(&fakeGpuBackend);
        thumbnailStage = "thumbnail initial sync";
        thumbnails.Sync(registry);
        meshRecord = registry.Find(EditorAssetKind::Mesh, "mesh_rock_a");
        textureRecord = registry.Find(EditorAssetKind::Texture, "texture_legacy");
        runner.Expect(meshRecord != nullptr, "mesh record should be findable after registry mutations");
        runner.Expect(textureRecord != nullptr, "texture record should be findable after registry mutations");
        runner.Expect(
            thumbnails.Count() == registry.Count(),
            "thumbnail cache should track registered assets");
        runner.Expect(
            !BuildEditorAssetThumbnailKey(*meshRecord).empty(),
            "thumbnail key should be stable");
        runner.Expect(
            thumbnails.Resolve(*meshRecord).status == EditorAssetThumbnailStatus::Pending,
            "mesh thumbnail should be pending before preview job processing");
        runner.Expect(
            thumbnails.PreviewJobs().Count(EditorAssetPreviewJobStatus::Queued) >= 2,
            "thumbnail sync should queue preview jobs");
        {
            EditorAssetThumbnailService asyncThumbnails;
            asyncThumbnails.Sync(registry);
            const uint32_t asyncStarted =
                asyncThumbnails.ProcessPreviewJobs(std::chrono::milliseconds(1), 2);
            runner.Expect(
                asyncStarted > 0,
                "async thumbnail preview should launch work inside the UI time budget");

            bool asyncCompleted = false;
            for (int attempt = 0; attempt < 100; ++attempt) {
                asyncThumbnails.ProcessPreviewJobs(std::chrono::milliseconds(1), 2);
                if (asyncThumbnails.Count(EditorAssetThumbnailStatus::Ready) > 0 ||
                    asyncThumbnails.Count(EditorAssetThumbnailStatus::Failed) > 0) {
                    asyncCompleted = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            runner.Expect(
                asyncCompleted,
                "async thumbnail preview should complete without blocking the caller");
        }
        runner.Expect(
            thumbnails.ProcessPreviewJobs(2) == 2,
            "thumbnail preview jobs should respect the requested processing budget");
        thumbnails.ProcessPreviewJobs(16);
        runner.Expect(
            thumbnails.Resolve(*meshRecord).status == EditorAssetThumbnailStatus::Ready,
            "mesh thumbnail should use the rich preview provider");
        runner.Expect(
            thumbnails.Resolve(*meshRecord).previewKind == EditorAssetPreviewKind::Mesh,
            "mesh thumbnail should expose mesh preview metadata");
        runner.Expect(
            thumbnails.Resolve(*textureRecord).status == EditorAssetThumbnailStatus::Ready,
            "texture thumbnail should be preview ready for supported texture extensions");
        runner.Expect(
            thumbnails.GpuThumbnails().Count(EditorAssetGpuThumbnailStatus::Queued) >= 2,
            "ready preview thumbnails should queue GPU thumbnail rendering");
        runner.Expect(
            thumbnails.ProcessGpuThumbnails(1) == 1,
            "GPU thumbnail rendering should respect the requested processing budget");
        thumbnails.ProcessGpuThumbnails(16);
        runner.Expect(
            thumbnails.Resolve(*meshRecord).gpuStatus == EditorAssetGpuThumbnailStatus::Ready &&
                thumbnails.Resolve(*meshRecord).gpuHandleToken != 0 &&
                thumbnails.Resolve(*meshRecord).gpuDisplayTextureId != 0 &&
                thumbnails.Resolve(*meshRecord).gpuDescriptorIndex != UINT32_MAX &&
                thumbnails.Resolve(*meshRecord).gpuShaderResourceView,
            "mesh thumbnail should become GPU SRV resident");
        runner.Expect(
            thumbnails.Resolve(*textureRecord).gpuStatus == EditorAssetGpuThumbnailStatus::Ready &&
                thumbnails.Resolve(*textureRecord).gpuHandleToken != 0 &&
                thumbnails.Resolve(*textureRecord).gpuDisplayTextureId != 0,
            "texture thumbnail should become GPU SRV resident");
        runner.Expect(
            fakeGpuBackend.AllocationCount() >= 2,
            "GPU thumbnail backend should allocate SRV descriptors for ready thumbnails");

        thumbnailStage = "thumbnail gpu srv invalidation";
        const uint64_t meshTextureBefore = thumbnails.Resolve(*meshRecord).gpuDisplayTextureId;
        const uint32_t releasesBefore = fakeGpuBackend.ReleaseCount();
        EditorAssetRecord updatedMeshForGpu = *meshRecord;
        updatedMeshForGpu.sourceTimestamp += 101;
        runner.Expect(
            registry.Register(updatedMeshForGpu),
            "updated mesh registration should succeed for GPU thumbnail invalidation");
        thumbnails.Sync(registry);
        thumbnails.ProcessPreviewJobs(8);
        thumbnails.ProcessGpuThumbnails(8);
        meshRecord = registry.Find(EditorAssetKind::Mesh, "mesh_rock_a");
        runner.Expect(meshRecord != nullptr, "updated mesh record should be findable");
        runner.Expect(
            fakeGpuBackend.ReleaseCount() > releasesBefore,
            "stale GPU thumbnail SRV should be released when source timestamp changes");
        runner.Expect(
            thumbnails.Resolve(*meshRecord).gpuDisplayTextureId != 0 &&
                thumbnails.Resolve(*meshRecord).gpuDisplayTextureId != meshTextureBefore,
            "updated mesh thumbnail should receive a fresh GPU display texture id");

        thumbnailStage = "thumbnail invalid texture registration";
        EditorAssetRecord invalidTexture = MakeAsset(
            EditorAssetKind::Texture,
            "texture_invalid_preview",
            "Resources/textures/invalid.preview",
            true,
            "guid-texture-invalid-preview");
        invalidTexture.sourceTimestamp = 10;
        runner.Expect(registry.Register(invalidTexture), "invalid texture registration should succeed");
        thumbnails.Sync(registry);
        runner.Expect(
            thumbnails.Resolve(*registry.Find(EditorAssetKind::Texture, "texture_invalid_preview")).status ==
                EditorAssetThumbnailStatus::Pending,
            "invalid texture thumbnail should be pending until preview job processing");
        thumbnails.ProcessPreviewJobs(8);
        const EditorAssetRecord* invalidTextureRecord =
            registry.Find(EditorAssetKind::Texture, "texture_invalid_preview");
        runner.Expect(invalidTextureRecord != nullptr, "invalid texture record should be findable");
        runner.Expect(
            thumbnails.Resolve(*invalidTextureRecord).status == EditorAssetThumbnailStatus::Failed,
            "unsupported texture extension should report thumbnail failure");
        runner.Expect(
            thumbnails.Resolve(*invalidTextureRecord).gpuStatus == EditorAssetGpuThumbnailStatus::Cancelled,
            "failed preview thumbnails should not request GPU rendering");
        runner.Expect(
            thumbnails.RetryPreview(thumbnails.Resolve(*invalidTextureRecord).key),
            "failed thumbnail preview should be retryable");
        thumbnails.ProcessPreviewJobs(1);
        const uint32_t generationBefore =
            thumbnails.Resolve(*invalidTextureRecord).generation;

        thumbnailStage = "thumbnail timestamp invalidation";
        EditorAssetRecord updatedInvalidTexture = *invalidTextureRecord;
        updatedInvalidTexture.sourceTimestamp = 11;
        runner.Expect(
            registry.Register(updatedInvalidTexture),
            "updated invalid texture registration should succeed");
        thumbnails.Sync(registry);
        thumbnails.ProcessPreviewJobs(8);
        const uint32_t generationAfter =
            thumbnails.Resolve(*registry.Find(EditorAssetKind::Texture, "texture_invalid_preview")).generation;
        runner.Expect(
            generationAfter > generationBefore,
            "thumbnail generation should advance when source timestamp changes");

        thumbnailStage = "thumbnail diagnostics";
        EditorAssetThumbnailDiagnosticsAdapter thumbnailDiagnostics(&registry, &thumbnails);
        EditorValidationService validation;
        validation.AddAdapter(&thumbnailDiagnostics);
        const EditorValidationReport thumbnailReport = validation.Validate();
        runner.Expect(
            thumbnailReport.warningCount == 1,
            "thumbnail diagnostics should report failed preview generation");
    } catch (const std::exception& error) {
        throw std::runtime_error(thumbnailStage + ": " + error.what());
    }
    } catch (const std::exception& error) {
        throw std::runtime_error(assetStage + ": " + error.what());
    }
}

void TestAssetMigrationPipeline(RegressionRunner& runner) {
    const std::filesystem::path root =
        std::filesystem::path{"Resources"} / "__editor_asset_migration_regression";
    RemoveTreeIfPresent(root);

    try {
        const std::filesystem::path meshPath = root / "source" / "migration_mesh.mesh";
        const std::filesystem::path coursePath = root / "course" / "migration_course.course";
        WriteTextFile(meshPath, "migration mesh");
        WriteTextFile(coursePath, "mesh=migration_mesh\n");

        EditorAssetRegistry registry;
        EditorAssetRecord legacyMesh = MakeAsset(
            EditorAssetKind::Mesh,
            "migration_mesh",
            meshPath.generic_string(),
            false);
        legacyMesh.logicalPath = legacyMesh.sourcePath;
        legacyMesh.metadataPath = legacyMesh.sourcePath + ".meta";
        legacyMesh.sourceTimestamp = 10;

        EditorAssetRecord course = MakeAsset(
            EditorAssetKind::Course,
            "migration_course",
            coursePath.generic_string(),
            true,
            "guid-migration-course");
        course.logicalPath = course.sourcePath;
        course.metadataPath = course.sourcePath + ".meta";
        course.dependencies.push_back("Mesh:migration_mesh");
        WriteTextFile(
            course.metadataPath,
            "guid=guid-migration-course\n"
            "logicalPath=" + course.logicalPath + "\n"
            "dependencies=Mesh:migration_mesh\n");

        runner.Expect(registry.Register(legacyMesh), "legacy migration mesh should register");
        runner.Expect(registry.Register(course), "migration course should register");
        const EditorAssetRecord* meshRecord = registry.Find(EditorAssetKind::Mesh, "migration_mesh");
        runner.Expect(meshRecord != nullptr, "migration mesh should be findable");
        const EditorAssetHandle legacyHandle =
            MakeEditorAssetHandle(*meshRecord, registry.Revision());

        EditorAssetReferenceDiagnosticsAdapter referenceDiagnostics(&registry);
        EditorValidationReport legacyReport{};
        referenceDiagnostics.Validate(legacyReport);
        runner.Expect(
            legacyReport.warningCount >= 1,
            "legacy path-only asset should report a migration warning");

        const EditorAssetMutationSafetyReport blockedRename =
            EvaluateEditorAssetMutationSafety(
                registry,
                *meshRecord,
                EditorAssetMutationKind::Rename);
        runner.Expect(blockedRename.Blocked(), "legacy asset rename should block before .meta migration");

        EditorAssetRecord migratedMesh = *meshRecord;
        EnsureEditorAssetIdentity(migratedMesh);
        WriteTextFile(
            migratedMesh.metadataPath,
            "guid=" + migratedMesh.guid + "\n"
            "logicalPath=" + migratedMesh.logicalPath + "\n");
        migratedMesh.hasMetadata = true;
        migratedMesh.provisionalGuid = false;
        runner.Expect(registry.Register(migratedMesh), "migrated mesh should re-register with durable metadata");
        runner.Expect(
            !IsEditorAssetHandleCurrent(registry, legacyHandle),
            "legacy handle should become stale after registry migration");
        const EditorAssetHandleResolveResult staleResolve =
            ResolveEditorAssetHandle(registry, legacyHandle);
        runner.Expect(
            staleResolve.found && staleResolve.record != nullptr,
            "stale handle should still resolve by stable asset identity");
        const EditorAssetHandle refreshedHandle =
            RefreshEditorAssetHandle(registry, legacyHandle);
        runner.Expect(
            refreshedHandle.Valid() && IsEditorAssetHandleCurrent(registry, refreshedHandle),
            "refreshed asset handle should be current after migration");

        EditorValidationReport migratedReport{};
        referenceDiagnostics.Validate(migratedReport);
        runner.Expect(
            migratedReport.warningCount == 0,
            "durable metadata migration should clear path fallback warnings");

        EditorAssetThumbnailService thumbnails;
        thumbnails.Sync(registry);
        thumbnails.ProcessPreviewJobs(8);
        const EditorAssetThumbnailEntry beforeThumbnail =
            thumbnails.Resolve(*registry.Find(EditorAssetKind::Mesh, "migration_mesh"));
        runner.Expect(
            beforeThumbnail.status == EditorAssetThumbnailStatus::Ready,
            "migration mesh should produce rich preview metadata");
        runner.Expect(
            beforeThumbnail.lineCount > 0,
            "migration mesh preview should summarize source lines");
        thumbnails.Clear();

        EditorAssetMutationExecutor executor(registry);
        EditorTransactionStack transactions;
        std::string lastAssetTransactionError;
        const auto applyAssetTransaction =
            [&](const EditorTransactionRecord& record, EditorTransactionApplyMode mode) {
                const EditorAssetMutationResult result = executor.ApplyTransaction(record, mode);
                if (!result.succeeded) {
                    lastAssetTransactionError = result.message.empty()
                        ? record.label + " " + std::string(ToString(mode)) + " returned an empty error."
                        : result.message;
                }
                return result.succeeded;
            };

        const EditorAssetMutationResult renameResult =
            executor.Execute(
                EditorAssetMutationRequest{
                    EditorAssetMutationKind::Rename,
                    EditorAssetKind::Mesh,
                    "migration_mesh",
                    "migration_mesh_renamed",
                    {},
                    &transactions});
        runner.Expect(renameResult.succeeded, "migrated asset rename should succeed");
        runner.Expect(
            registry.Find(EditorAssetKind::Mesh, "migration_mesh") == nullptr,
            "renamed migration asset should remove old id");
        const EditorAssetRecord* renamedMesh =
            registry.Find(EditorAssetKind::Mesh, "migration_mesh_renamed");
        runner.Expect(renamedMesh != nullptr, "renamed migration asset should be findable");
        const EditorAssetRecord* renamedCourse =
            registry.Find(EditorAssetKind::Course, "migration_course");
        runner.Expect(
            renamedCourse != nullptr &&
                !renamedCourse->dependencies.empty() &&
                renamedCourse->dependencies.front() == "Mesh:migration_mesh_renamed",
            "migration dependent should rewrite after rename");
        runner.Expect(
            !ResolveEditorAssetHandle(registry, refreshedHandle).found,
            "old refreshed handle should not resolve after identity-changing rename");

        const std::filesystem::path moveDestination = root / "moved";
        const EditorAssetMutationResult moveResult =
            executor.Execute(
                EditorAssetMutationRequest{
                    EditorAssetMutationKind::Move,
                    EditorAssetKind::Mesh,
                    "migration_mesh_renamed",
                    {},
                    moveDestination.generic_string(),
                    &transactions});
        runner.Expect(moveResult.succeeded, "migrated asset move should succeed");
        const EditorAssetRecord* movedMesh =
            registry.Find(EditorAssetKind::Mesh, "migration_mesh_renamed");
        runner.Expect(
            movedMesh != nullptr && movedMesh->sourcePath.find("/moved/") != std::string::npos,
            "migrated asset move should update source path");
        thumbnails.Sync(registry);
        thumbnails.ProcessPreviewJobs(8);
        const EditorAssetThumbnailEntry movedThumbnail = thumbnails.Resolve(*movedMesh);
        runner.Expect(
            movedThumbnail.key == movedMesh->thumbnailKey,
            "thumbnail cache should follow moved asset stable key");
        thumbnails.Clear();

        const EditorAssetMutationResult blockedDelete =
            executor.Execute(
                EditorAssetMutationRequest{
                    EditorAssetMutationKind::Delete,
                    EditorAssetKind::Mesh,
                    "migration_mesh_renamed",
                    {},
                    {},
                    &transactions});
        runner.Expect(!blockedDelete.succeeded, "delete should block while migrated mesh has dependents");

        const EditorAssetMutationResult deleteCourse =
            executor.Execute(
                EditorAssetMutationRequest{
                    EditorAssetMutationKind::Delete,
                    EditorAssetKind::Course,
                    "migration_course",
                    {},
                    {},
                    &transactions});
        runner.Expect(deleteCourse.succeeded, "migration dependent delete should succeed");
        runner.Expect(
            registry.Find(EditorAssetKind::Course, "migration_course") == nullptr,
            "migration dependent delete should remove registry record");

        const EditorAssetMutationResult deleteMesh =
            executor.Execute(
                EditorAssetMutationRequest{
                    EditorAssetMutationKind::Delete,
                    EditorAssetKind::Mesh,
                    "migration_mesh_renamed",
                    {},
                    {},
                    &transactions});
        runner.Expect(deleteMesh.succeeded, "migration mesh delete should succeed after dependents are gone");
        runner.Expect(
            registry.Find(EditorAssetKind::Mesh, "migration_mesh_renamed") == nullptr,
            "migration mesh delete should remove registry record");

        runner.Expect(transactions.Undo(applyAssetTransaction), "migration mesh delete undo should restore mesh");
        runner.Expect(transactions.Undo(applyAssetTransaction), "migration dependent delete undo should restore course");
        const EditorTransactionRecord* nextMoveUndo = transactions.NextUndoTransaction();
        runner.Expect(
            nextMoveUndo != nullptr,
            "migration move undo should have a transaction available");
        runner.Expect(
            nextMoveUndo->label == "Move Asset",
            "migration move undo expected Move Asset, got " + nextMoveUndo->label);
        runner.Expect(
            transactions.Undo(applyAssetTransaction),
            "migration move undo should restore source folder: " + lastAssetTransactionError);
        runner.Expect(transactions.Undo(applyAssetTransaction), "migration rename undo should restore original id");
        runner.Expect(
            registry.Find(EditorAssetKind::Mesh, "migration_mesh") != nullptr,
            "migration rename undo should restore original mesh id");
        const EditorAssetRecord* restoredCourse =
            registry.Find(EditorAssetKind::Course, "migration_course");
        runner.Expect(
            restoredCourse != nullptr &&
                !restoredCourse->dependencies.empty() &&
                restoredCourse->dependencies.front() == "Mesh:migration_mesh",
            "migration undo should restore original dependency token");

        runner.Expect(transactions.Redo(applyAssetTransaction), "migration rename redo should apply");
        runner.Expect(transactions.Redo(applyAssetTransaction), "migration move redo should apply");
        runner.Expect(
            registry.Find(EditorAssetKind::Mesh, "migration_mesh_renamed") != nullptr,
            "migration redo should restore moved renamed mesh");
    } catch (...) {
        RemoveTreeIfPresent(root);
        throw;
    }

    RemoveTreeIfPresent(root);
}

void TestAssetImportReimportPipeline(RegressionRunner& runner) {
    const std::filesystem::path root =
        std::filesystem::path{"Resources"} / "__editor_asset_import_regression";
    const std::filesystem::path externalRoot =
        std::filesystem::path{"generated"} / "editor" / "tests" / "external_asset_import";
    RemoveTreeIfPresent(root);
    RemoveTreeIfPresent(externalRoot);

    std::string importStage = "setup";
    try {
        importStage = "write fixtures";
        const std::filesystem::path meshPath = root / "mesh" / "import_mesh.mesh";
        const std::filesystem::path coursePath = root / "course" / "import_course.course";
        const std::filesystem::path legacyPath = root / "legacy" / "legacy_texture.png";
        const std::filesystem::path externalTexturePath = externalRoot / "external_texture.bmp";
        const std::filesystem::path externalBatchTexturePath = externalRoot / "batch_texture.bmp";
        const std::filesystem::path unsupportedExternalPath = externalRoot / "unsupported.txt";
        WriteTextFile(meshPath, "import mesh v1");
        WriteTextFile(coursePath, "mesh=import_mesh\n");
        WriteTextFile(legacyPath, "legacy texture");
        WriteBinaryFile(externalTexturePath, MakeBmpPreviewHeader(4, 2));
        WriteBinaryFile(externalBatchTexturePath, MakeBmpPreviewHeader(6, 3));
        WriteTextFile(unsupportedExternalPath, "unsupported");

        EditorAssetRegistry registry;
        EditorAssetThumbnailService thumbnails;
        EditorAssetImportService importService(registry, &thumbnails);

        importStage = "mesh import";
        const EditorAssetImportResult meshImport = importService.Import(meshPath);
        runner.Expect(meshImport.succeeded, "mesh import should succeed");
        runner.Expect(meshImport.record.kind == EditorAssetKind::Mesh, "mesh import should classify mesh kind");
        runner.Expect(meshImport.record.hasMetadata, "mesh import should create durable metadata");
        runner.Expect(!meshImport.record.provisionalGuid, "mesh import should clear provisional GUID state");
        runner.Expect(std::filesystem::exists(meshImport.record.metadataPath), "mesh import should write .meta file");
        const std::string meshGuid = meshImport.record.guid;

        importStage = "course import";
        const EditorAssetImportResult courseImport = importService.Import(coursePath);
        runner.Expect(courseImport.succeeded, "course import should succeed");
        const EditorAssetRecord* courseRecord =
            registry.Find(EditorAssetKind::Course, "__editor_asset_import_regression/course/import_course");
        runner.Expect(courseRecord != nullptr, "course import should register logical id");
        runner.Expect(
            courseRecord != nullptr &&
                std::find(
                    courseRecord->dependencies.begin(),
                    courseRecord->dependencies.end(),
                    "Mesh:import_mesh") != courseRecord->dependencies.end(),
            "course import should scan dependency text");
        runner.Expect(
            thumbnails.Count() == registry.Count(),
            "import service should sync thumbnail cache after import");
        runner.Expect(
            thumbnails.PreviewJobs().Count(EditorAssetPreviewJobStatus::Queued) > 0,
            "import service should enqueue preview jobs after import");
        thumbnails.ProcessPreviewJobs(8);

        importStage = "external import";
        EditorAssetExternalImportPolicy externalPolicy{};
        externalPolicy.destinationFolder = "__editor_asset_import_regression/external";
        externalPolicy.collisionPolicy = EditorAssetExternalImportCollisionPolicy::Rename;
        const EditorAssetImportResult externalImport =
            importService.ImportExternal(externalTexturePath, externalPolicy);
        runner.Expect(externalImport.succeeded, "external texture import should succeed");
        runner.Expect(
            externalImport.record.sourcePath ==
                "Resources/__editor_asset_import_regression/external/external_texture.bmp",
            "external import should copy into the requested Resources folder");
        runner.Expect(
            std::filesystem::exists(externalImport.record.sourcePath),
            "external import should create destination file");
        runner.Expect(
            externalImport.record.hasMetadata && !externalImport.record.provisionalGuid,
            "external import should create durable metadata");
        thumbnails.ProcessPreviewJobs(8);
        thumbnails.ProcessGpuThumbnails(8);
        const EditorAssetThumbnailEntry externalThumbnail = thumbnails.Resolve(externalImport.record);
        runner.Expect(
            externalThumbnail.status == EditorAssetThumbnailStatus::Ready &&
                externalThumbnail.previewKind == EditorAssetPreviewKind::Texture,
            "external import should refresh rich texture preview metadata");
        runner.Expect(
            externalThumbnail.width == 4 && externalThumbnail.height == 2,
            "external texture preview should expose source dimensions");
        runner.Expect(
            externalThumbnail.gpuStatus == EditorAssetGpuThumbnailStatus::Ready &&
                externalThumbnail.gpuHandleToken != 0,
            "external import should allocate a GPU thumbnail after preview generation");
        importStage = "external import collision";
        const EditorAssetImportResult externalCollisionImport =
            importService.ImportExternal(externalTexturePath, externalPolicy);
        runner.Expect(externalCollisionImport.succeeded, "external import collision rename should succeed");
        runner.Expect(
            externalCollisionImport.record.sourcePath.find("external_texture_1.bmp") != std::string::npos,
            "external import collision should allocate a unique filename");
        importStage = "unsupported external import";
        const EditorAssetImportResult unsupportedImport =
            importService.ImportExternal(unsupportedExternalPath, externalPolicy);
        runner.Expect(!unsupportedImport.succeeded, "unsupported external import should fail");
        runner.Expect(
            !std::filesystem::exists(root / "external" / "unsupported.txt"),
            "unsupported external import should not copy a partial destination");

        importStage = "batch external import";
        EditorAssetExternalImportPolicy batchExternalPolicy{};
        batchExternalPolicy.destinationFolder = "__editor_asset_import_regression/batch";
        batchExternalPolicy.collisionPolicy = EditorAssetExternalImportCollisionPolicy::Rename;
        const EditorAssetImportResult batchExternalImport =
            importService.ImportExternalBatch(
                std::vector<std::filesystem::path>{
                    externalBatchTexturePath,
                    externalBatchTexturePath,
                    unsupportedExternalPath},
                batchExternalPolicy);
        runner.Expect(batchExternalImport.succeeded, "batch external import should succeed with valid files");
        runner.Expect(batchExternalImport.warning, "batch external import should warn for skipped unsupported files");
        runner.Expect(batchExternalImport.importedCount == 2, "batch external import should import both valid files");
        runner.Expect(batchExternalImport.skippedCount == 1, "batch external import should count unsupported files");
        runner.Expect(
            batchExternalImport.record.sourcePath.find("batch_texture_1.bmp") != std::string::npos,
            "batch external import should preserve collision rename result");
        runner.Expect(
            !std::filesystem::exists(root / "batch" / "unsupported.txt"),
            "batch external import should not copy unsupported files");
        thumbnails.ProcessPreviewJobs(8);
        thumbnails.ProcessGpuThumbnails(8);
        const EditorAssetThumbnailEntry batchExternalThumbnail =
            thumbnails.Resolve(batchExternalImport.record);
        runner.Expect(
            batchExternalThumbnail.status == EditorAssetThumbnailStatus::Ready &&
                batchExternalThumbnail.width == 6 &&
                batchExternalThumbnail.height == 3,
            "batch external import should refresh rich preview metadata once finalized");
        runner.Expect(
            batchExternalThumbnail.gpuStatus == EditorAssetGpuThumbnailStatus::Ready &&
                batchExternalThumbnail.gpuHandleToken != 0,
            "batch external import should allocate a GPU thumbnail once finalized");

        importStage = "course reimport";
        courseRecord =
            registry.Find(EditorAssetKind::Course, "__editor_asset_import_regression/course/import_course");
        runner.Expect(courseRecord != nullptr, "course record should be findable after external imports");
        const EditorAssetHandle courseHandle =
            MakeEditorAssetHandle(*courseRecord, registry.Revision());
        WriteTextFile(coursePath, "mesh=import_mesh\nreimported=true\n");
        const EditorAssetImportResult courseReimport =
            importService.Reimport(EditorAssetKind::Course, "__editor_asset_import_regression/course/import_course");
        runner.Expect(courseReimport.succeeded, "course reimport should succeed");
        const EditorAssetRecord* reimportedCourse =
            registry.Find(EditorAssetKind::Course, "__editor_asset_import_regression/course/import_course");
        runner.Expect(reimportedCourse != nullptr, "reimported course should remain findable");
        runner.Expect(
            reimportedCourse != nullptr && reimportedCourse->guid == courseImport.record.guid,
            "course reimport should preserve GUID");
        runner.Expect(
            !IsEditorAssetHandleCurrent(registry, courseHandle),
            "course handle should become stale after reimport revision");
        runner.Expect(
            RefreshEditorAssetHandle(registry, courseHandle).Valid(),
            "course handle should refresh after reimport");

        importStage = "legacy texture registration";
        EditorAssetRecord legacyTexture = MakeAsset(
            EditorAssetKind::Texture,
            "legacy_texture",
            legacyPath.generic_string(),
            false);
        legacyTexture.logicalPath = legacyTexture.sourcePath;
        runner.Expect(registry.Register(legacyTexture), "legacy texture registration should succeed");
        EditorAssetReferenceDiagnosticsAdapter diagnostics(&registry);
        EditorValidationReport beforeMigration{};
        diagnostics.Validate(beforeMigration);
        runner.Expect(
            beforeMigration.warningCount >= 1,
            "legacy texture should emit metadata migration warning before batch migration");

        importStage = "batch metadata migration";
        const EditorAssetImportResult batchMigration = importService.BatchMigrateMetadata();
        runner.Expect(batchMigration.succeeded, "batch metadata migration should succeed");
        runner.Expect(batchMigration.migratedCount >= 1, "batch migration should migrate at least one asset");
        const EditorAssetRecord* migratedLegacy =
            registry.Find(EditorAssetKind::Texture, "legacy_texture");
        runner.Expect(migratedLegacy != nullptr, "migrated legacy texture should remain registered");
        runner.Expect(
            migratedLegacy != nullptr && migratedLegacy->hasMetadata && !migratedLegacy->provisionalGuid,
            "batch migration should make legacy texture metadata durable");
        runner.Expect(
            migratedLegacy != nullptr && std::filesystem::exists(migratedLegacy->metadataPath),
            "batch migration should write legacy .meta file");
        EditorValidationReport afterMigration{};
        diagnostics.Validate(afterMigration);
        runner.Expect(
            afterMigration.warningCount == 0,
            "batch metadata migration should clear migration warnings");

        importStage = "missing reimport";
        std::error_code removeError;
        std::filesystem::remove(meshPath, removeError);
        const EditorAssetImportResult missingReimport =
            importService.Reimport(EditorAssetKind::Mesh, "import_mesh");
        runner.Expect(!missingReimport.succeeded, "missing source reimport should fail");
        const EditorAssetRecord* meshRecord =
            registry.Find(EditorAssetKind::Mesh, "import_mesh");
        runner.Expect(
            meshRecord != nullptr && meshRecord->guid == meshGuid,
            "failed reimport should preserve existing registry identity");
    } catch (const std::exception& error) {
        RemoveTreeIfPresent(root);
        RemoveTreeIfPresent(externalRoot);
        throw std::runtime_error(importStage + ": " + std::string(error.what()));
    } catch (...) {
        RemoveTreeIfPresent(root);
        RemoveTreeIfPresent(externalRoot);
        throw;
    }

    RemoveTreeIfPresent(root);
    RemoveTreeIfPresent(externalRoot);
}

void TestLayoutPersistence(RegressionRunner& runner) {
    const std::filesystem::path testPath =
        std::filesystem::path{"generated"} / "editor" / "tests" / "layout_regression.ini";

    EditorPanelRegistry panels;
    runner.Expect(
        panels.Register(
            EditorPanelDescriptor{
                "editor.viewport",
                "Viewport",
                "Editor",
                EditorPanelHostArea::Viewport,
                true,
                []() {}}),
        "viewport panel should register");
    runner.Expect(
        panels.Register(
            EditorPanelDescriptor{
                "course.timeline",
                "Course Timeline",
                "Course",
                EditorPanelHostArea::BottomDock,
                true,
                []() {}}),
        "course timeline panel should register");
    runner.Expect(
        panels.Register(
            EditorPanelDescriptor{
                "vfx.inspector",
                "VFX Inspector",
                "VFX",
                EditorPanelHostArea::RightInspector,
                true,
                []() {}}),
        "vfx inspector panel should register");

    EditorLayoutPersistenceService persistence;
    persistence.SetPath(testPath);
    persistence.CaptureRegistryDefaults(panels);
    persistence.ApplyWorkspacePreset("VFX Debug");
    persistence.SetActivePanel(EditorPanelHostArea::BottomDock, "course.timeline");
    persistence.SetActivePanel(EditorPanelHostArea::Viewport, "editor.viewport");
    persistence.SetPanelVisible("vfx.inspector", false);
    runner.Expect(persistence.Save(), "layout persistence should save test layout");

    EditorLayoutPersistenceService loaded;
    loaded.SetPath(testPath);
    runner.Expect(loaded.Load(), "layout persistence should load test layout");
    runner.Expect(
        loaded.ActivePanel(EditorPanelHostArea::BottomDock) == "course.timeline",
        "active bottom dock panel should round-trip");
    runner.Expect(
        loaded.ActivePanel(EditorPanelHostArea::Viewport) == "editor.viewport",
        "active viewport panel should round-trip");
    runner.Expect(
        loaded.WorkspacePreset() == "VFX Debug",
        "workspace preset should round-trip");
    runner.Expect(!loaded.IsPanelVisible("vfx.inspector"), "panel visibility should round-trip");
    loaded.SetActivePanel(EditorPanelHostArea::RightInspector, "missing.panel");
    runner.Expect(
        !loaded.ValidateActivePanels(panels),
        "missing active panel should be cleared by validation");
    runner.Expect(
        loaded.ActivePanel(EditorPanelHostArea::RightInspector).empty(),
        "missing active panel should reset to safe default");

    {
        std::ofstream corrupt(testPath, std::ios::trunc);
        corrupt << "version=1\n";
        corrupt << "unknownBrokenLine\n";
        corrupt << "active.BottomDock=course.timeline\n";
    }
    EditorLayoutPersistenceService recovered;
    recovered.SetPath(testPath);
    runner.Expect(recovered.Load(), "layout persistence should load corrupt layout with defaults");
    runner.Expect(!recovered.LastLoadValid(), "corrupt layout should be reported as not fully valid");
    runner.Expect(
        recovered.ActivePanel(EditorPanelHostArea::BottomDock) == "course.timeline",
        "valid entries in corrupt layout should still recover");

    const std::filesystem::path activeTabPath =
        std::filesystem::path{"generated"} / "editor" / "tests" / "layout_active_tabs.ini";
    std::error_code activeRemoveError;
    std::filesystem::remove(activeTabPath, activeRemoveError);
    EditorLayoutPersistenceService activeTabs;
    activeTabs.SetPath(activeTabPath);
    activeTabs.SetActivePanel(EditorPanelHostArea::LeftSidebar, "editor.workspace");
    runner.Expect(
        !activeTabs.Dirty(),
        "programmatic active panel synchronization should not dirty layout persistence");
    activeTabs.SetActivePanelFromUser(EditorPanelHostArea::LeftSidebar, "editor.selection");
    runner.Expect(
        activeTabs.Dirty(),
        "user active tab selection should dirty layout persistence");
    activeTabs.SaveIfDirty();
    runner.Expect(
        activeTabs.Dirty() && !std::filesystem::exists(activeTabPath),
        "debounced layout persistence should not save immediately after tab selection");
    runner.Expect(activeTabs.Save(), "explicit layout save should flush debounced active tab selection");
    runner.Expect(!activeTabs.Dirty(), "explicit layout save should clear dirty state");
    std::filesystem::remove(activeTabPath, activeRemoveError);

    std::error_code removeError;
    std::filesystem::remove(testPath, removeError);
}

void TestEditorToolRegistrationDescriptors(RegressionRunner& runner) {
    EditorToolRegistry tools;
    EditorCommandRegistry commands;
    EditorPanelRegistry panels;

    tools.BeginFrame();
    runner.Expect(
        tools.RegisterCommand(
            EditorCommandRegistrationDescriptor{
                {},
                "test.tool.primary",
                EditorCommand{
                    "test.command",
                    "Test Command",
                    "Test",
                    "Ctrl+T",
                    []() { return true; },
                    []() { return std::string(); },
                    []() { return EditorCommandResult{true, "ok"}; }},
                false},
            commands),
        "command descriptor should register");
    runner.Expect(commands.Find("test.command") != nullptr, "registered command should be findable");

    runner.Expect(
        !tools.RegisterCommand(
            EditorCommandRegistrationDescriptor{
                {},
                "test.tool.duplicate",
                EditorCommand{
                    "test.command",
                    "Duplicate Test Command",
                    "Test",
                    "",
                    nullptr,
                    nullptr,
                    nullptr},
                false},
            commands),
        "duplicate command descriptor should be rejected when replacement is disabled");
    runner.Expect(tools.ErrorCount() >= 1, "duplicate command should emit a registration error");

    const uint32_t warningCountBeforeShortcut = tools.WarningCount();
    runner.Expect(
        tools.RegisterCommand(
            EditorCommandRegistrationDescriptor{
                {},
                "test.tool.shortcutConflict",
                EditorCommand{
                    "test.command.conflict",
                    "Shortcut Conflict",
                    "Test",
                    "Ctrl+T",
                    nullptr,
                    nullptr,
                    nullptr},
                false},
            commands),
        "shortcut-conflicting command should still register");
    runner.Expect(
        tools.WarningCount() > warningCountBeforeShortcut,
        "shortcut conflict should emit a warning");

    runner.Expect(
        tools.RegisterPanel(
            EditorPanelRegistrationDescriptor{
                {},
                "test.panel.tool",
                EditorPanelDescriptor{
                    "test.panel",
                    "Test Panel",
                    "Test",
                    EditorPanelHostArea::Diagnostics,
                    true,
                    []() {}},
                false},
            panels),
        "panel descriptor should register");
    runner.Expect(panels.Count() == 1, "panel registry should contain descriptor panel");

    runner.Expect(
        tools.RegisterToolbarItem(
            EditorToolbarItemDescriptor{
                {},
                "toolbar.test.command",
                "test.command",
                "Test",
                10,
                false,
                true,
                false}),
        "toolbar descriptor should register");
    runner.Expect(
        tools.ValidateToolbarCommands(commands),
        "toolbar descriptor should resolve registered command");

    runner.Expect(
        tools.RegisterToolbarItem(
            EditorToolbarItemDescriptor{
                {},
                "toolbar.test.missing",
                "test.missing",
                "Missing",
                20,
                false,
                true,
                false}),
        "missing-command toolbar descriptor should register before validation");
    runner.Expect(
        !tools.ValidateToolbarCommands(commands),
        "toolbar validation should fail for unresolved command ids");

    runner.Expect(
        tools.RegisterMenuSection(
            EditorMenuSectionDescriptor{
                {},
                "menu.test",
                "Test",
                10,
                true,
                false,
                {}}),
        "menu section descriptor should register");
    runner.Expect(
        !tools.RegisterMenuSection(
            EditorMenuSectionDescriptor{
                {},
                "menu.test",
                "Duplicate Test",
                20,
                true,
                false,
                {}}),
        "duplicate menu section should be rejected when replacement is disabled");
    runner.Expect(
        tools.RegisterMenuItem(
            EditorMenuItemDescriptor{
                {},
                "menu.item.test.command",
                "menu.test",
                "test.command",
                "Test Command",
                10,
                false,
                true,
                false,
                {}}),
        "menu item descriptor should register");
    runner.Expect(
        tools.ValidateMenuCommands(commands),
        "menu descriptor should resolve registered command and section");

    const uint32_t menuErrorCountBeforeMissing = tools.ErrorCount();
    runner.Expect(
        tools.RegisterMenuItem(
            EditorMenuItemDescriptor{
                {},
                "menu.item.test.missing",
                "menu.test",
                "test.menu.missing",
                "Missing",
                20,
                false,
                true,
                false,
                {}}),
        "missing-command menu descriptor should register before validation");
    runner.Expect(
        !tools.ValidateMenuCommands(commands),
        "menu validation should fail for unresolved command ids");
    runner.Expect(
        tools.ErrorCount() > menuErrorCountBeforeMissing,
        "missing menu command should emit a registration error");

    EditorToolRegistry featureTools;
    EditorCommandRegistry featureCommands;
    EditorPanelRegistry featurePanels;
    featureTools.BeginFrame();
    runner.Expect(
        featureTools.RegisterCommand(
            EditorCommandRegistrationDescriptor{
                {},
                "feature.disabled.command",
                EditorCommand{
                    "feature.disabled",
                    "Disabled Feature Command",
                    "Feature",
                    "",
                    nullptr,
                    nullptr,
                    nullptr},
                false,
                EditorToolFeatureGate{EditorToolFeatureState::Disabled, "test.disabled", true}},
            featureCommands),
        "disabled feature command registration should be a safe no-op");
    runner.Expect(
        featureCommands.Find("feature.disabled") == nullptr,
        "disabled feature command should not enter command registry");
    runner.Expect(
        featureTools.RegisterPanel(
            EditorPanelRegistrationDescriptor{
                {},
                "feature.hidden.panel",
                EditorPanelDescriptor{
                    "feature.hidden.panel",
                    "Hidden Panel",
                    "Feature",
                    EditorPanelHostArea::Diagnostics,
                    true,
                    []() {}},
                false,
                EditorToolFeatureGate{EditorToolFeatureState::Hidden, "test.hidden", true}},
            featurePanels),
        "hidden feature panel should register as hidden");
    runner.Expect(
        featurePanels.Count(EditorPanelHostArea::Diagnostics) == 0,
        "hidden feature panel should not be visible in panel hosts");
    runner.Expect(
        featureTools.RegisterToolbarItem(
            EditorToolbarItemDescriptor{
                {},
                "toolbar.feature.hidden",
                "test.command",
                "Hidden",
                10,
                false,
                true,
                false,
                EditorToolFeatureGate{EditorToolFeatureState::Hidden, "test.hidden", true}}),
        "hidden feature toolbar item should register as hidden");
    runner.Expect(
        featureTools.Toolbar().VisibleItems().empty(),
        "hidden feature toolbar item should not be visible");
    runner.Expect(
        featureTools.RegisterMenuSection(
            EditorMenuSectionDescriptor{
                {},
                "menu.feature.disabled",
                "Disabled",
                10,
                true,
                false,
                EditorToolFeatureGate{EditorToolFeatureState::Disabled, "test.disabled", true}}),
        "disabled feature menu section should be a safe no-op");
    runner.Expect(
        featureTools.Menu().VisibleSections().empty(),
        "disabled feature menu section should not be visible");

    EditorToolRegistry defaultToolbarTools;
    defaultToolbarTools.BeginFrame();
    RegisterDefaultEditorToolbar(defaultToolbarTools);
    runner.Expect(
        defaultToolbarTools.Toolbar().Count() >= 10,
        "default editor toolbar should be descriptor-driven");
    RegisterDefaultEditorMenu(defaultToolbarTools, commands);
    runner.Expect(
        defaultToolbarTools.Menu().SectionCount() > 0 &&
            defaultToolbarTools.Menu().ItemCount() >= commands.Count(),
        "default editor menu should be descriptor-driven");

    EditorToolRegistry providerTools;
    providerTools.BeginFrame();
    runner.Expect(
        providerTools.RegisterAssetProvider(
            EditorAssetProviderDescriptor{
                {},
                "asset.provider.test",
                "Test Asset Provider",
                "Asset",
                false,
                {}}),
        "asset provider descriptor should register");
    runner.Expect(
        providerTools.AssetProviders().size() == 1,
        "asset provider registry should retain descriptor");

    EditorCompositePropertyAccessor compositeAccessor;
    EditorObjectHandle propertyTarget{};
    propertyTarget.domain = EditorDomainId::CourseTerrainPlacement;
    propertyTarget.stableId = "provider-property-target";
    propertyTarget.generation = 1;
    EditorPropertyDescriptor propertyDescriptor{};
    propertyDescriptor.domain = propertyTarget.domain;
    propertyDescriptor.name = "provider.value";
    propertyDescriptor.kind = EditorPropertyKind::Float;
    RegressionPropertyAccessor providerAccessor(propertyTarget, propertyDescriptor.name, 42.0f);
    runner.Expect(
        providerTools.RegisterPropertyAccessor(
            EditorPropertyAccessorRegistrationDescriptor{
                {},
                "property.provider.test",
                &providerAccessor,
                0,
                false,
                {}},
            compositeAccessor),
        "property accessor provider should register");
    runner.Expect(
        compositeAccessor.CanAccess(propertyTarget, propertyDescriptor),
        "registered property accessor should be composed");

    class ProviderValidationAdapter final : public EditorValidationAdapter {
    public:
        void Validate(EditorValidationReport& report) const override {
            EditorValidationIssue issue{};
            issue.severity = EditorValidationSeverity::Warning;
            issue.title = "Provider";
            issue.message = "provider validation reached";
            report.AddIssue(std::move(issue));
        }
    };
    ProviderValidationAdapter providerValidation;
    EditorValidationService providerValidationService;
    runner.Expect(
        providerTools.RegisterValidationAdapter(
            EditorValidationAdapterRegistrationDescriptor{
                {},
                "validation.provider.test",
                &providerValidation,
                0,
                false,
                {}},
            providerValidationService),
        "validation adapter provider should register");
    const EditorValidationReport providerReport = providerValidationService.Validate();
    runner.Expect(
        providerReport.warningCount == 1,
        "registered validation provider should contribute report issues");

    EditorRuntimeInspector providerRuntimeInspector;
    runner.Expect(
        providerTools.RegisterRuntimeWatchProvider(
            EditorRuntimeWatchProviderDescriptor{
                {},
                "runtime.provider.test",
                "Runtime Provider Test",
                0,
                [](const EditorRuntimeWatchBuildInput& input) {
                    if (input.inspector != nullptr) {
                        input.inspector->AddRecord(
                            EditorRuntimeWatchRecord{
                                "Provider",
                                "Runtime Provider Test",
                                "Ready",
                                "provider runtime watch reached",
                                EditorRuntimeWatchSeverity::Info,
                                0});
                    }
                },
                false,
                {}}),
        "runtime watch provider should register");
    providerTools.BuildRuntimeWatch(
        EditorRuntimeWatchBuildInput{
            &providerRuntimeInspector});
    runner.Expect(
        providerRuntimeInspector.Count() == 1,
        "registered runtime watch provider should contribute rows");

    EditorToolRegistry disabledProviderTools;
    disabledProviderTools.BeginFrame();
    EditorCompositePropertyAccessor disabledComposite;
    runner.Expect(
        disabledProviderTools.RegisterPropertyAccessor(
            EditorPropertyAccessorRegistrationDescriptor{
                {},
                "property.provider.disabled",
                &providerAccessor,
                0,
                false,
                EditorToolFeatureGate{EditorToolFeatureState::Disabled, "provider.disabled", true}},
            disabledComposite),
        "disabled property provider should be a safe no-op");
    runner.Expect(
        !disabledComposite.CanAccess(propertyTarget, propertyDescriptor),
        "disabled property provider should not be composed");

    EditorToolRegistry moduleTools;
    moduleTools.BeginFrame();
    EditorToolModuleRegistry modules;
    std::vector<int> moduleOrder;
    bool disabledModuleRan = false;
    runner.Expect(
        modules.Register(
            EditorToolModuleRegistration{
                EditorToolModuleDescriptor{
                    {},
                    "module.late",
                    "Late Module",
                    20,
                    false,
                    {}},
                [&moduleOrder](EditorToolModuleRegistrationContext&) {
                    moduleOrder.push_back(20);
                }},
            &moduleTools),
        "tool module descriptor should register");
    runner.Expect(
        modules.Register(
            EditorToolModuleRegistration{
                EditorToolModuleDescriptor{
                    {},
                    "module.assets",
                    "Asset Module",
                    10,
                    false,
                    {}},
                [&moduleOrder](EditorToolModuleRegistrationContext& moduleContext) {
                    moduleOrder.push_back(10);
                    if (moduleContext.tools != nullptr) {
                        moduleContext.tools->RegisterAssetProvider(
                            EditorAssetProviderDescriptor{
                                {},
                                "asset.provider.module",
                                "Module Asset Provider",
                                "Asset",
                                false,
                                {}});
                    }
                }},
            &moduleTools),
        "tool module should register startup callbacks");
    runner.Expect(
        !modules.Register(
            EditorToolModuleRegistration{
                EditorToolModuleDescriptor{
                    {},
                    "module.assets",
                    "Duplicate Asset Module",
                    15,
                    false,
                    {}},
                [](EditorToolModuleRegistrationContext&) {}},
            &moduleTools),
        "duplicate tool module should be rejected when replacement is disabled");
    runner.Expect(moduleTools.ErrorCount() > 0, "duplicate module should emit a tool diagnostic");
    runner.Expect(
        modules.Register(
            EditorToolModuleRegistration{
                EditorToolModuleDescriptor{
                    {},
                    "module.disabled",
                    "Disabled Module",
                    0,
                    false,
                    EditorToolFeatureGate{EditorToolFeatureState::Disabled, "module.disabled", true}},
                [&disabledModuleRan](EditorToolModuleRegistrationContext&) {
                    disabledModuleRan = true;
                }},
            &moduleTools),
        "disabled tool module registration should be a safe no-op");
    modules.RunFrameRegistrations(
        EditorToolModuleRegistrationContext{
            &moduleTools});
    runner.Expect(
        moduleOrder.size() == 2 && moduleOrder[0] == 10 && moduleOrder[1] == 20,
        "tool modules should execute in load order");
    runner.Expect(!disabledModuleRan, "disabled tool module callback should not run");
    runner.Expect(
        moduleTools.AssetProviders().size() == 1 &&
            moduleTools.AssetProviders().front().id == "asset.provider.module",
        "tool module should publish provider descriptors through the registration context");

    EditorToolRegistry startupModuleTools;
    startupModuleTools.BeginFrame();
    EditorPropertyRegistry startupProperties;
    EditorDetailsSectionProviderRegistry startupDetailsSections;
    RunAppEditorStartupToolPipeline(
        AppEditorStartupToolModuleInput{
            &startupModuleTools,
            &startupProperties,
            &startupDetailsSections});
    runner.Expect(
        startupProperties.Count() > 0,
        "app startup tool modules should register built-in property descriptors");
    runner.Expect(
        startupDetailsSections.Count() > 0,
        "app startup tool modules should register built-in details section providers");

    EditorToolRegistry appProviderModuleTools;
    appProviderModuleTools.BeginFrame();
    EditorCompositePropertyAccessor appPropertyAccessors;
    EditorCompositePropertyAccessor appPreviewPropertyAccessors;
    EditorValidationService appValidationService;
    RunAppEditorFrameProviderToolPipeline(
        AppEditorFrameProviderToolModuleInput{
            &appProviderModuleTools,
            &appPropertyAccessors,
            &appPreviewPropertyAccessors,
            &appValidationService,
            &providerAccessor,
            &providerAccessor,
            &providerAccessor,
            &providerAccessor,
            &providerValidation,
            &providerValidation,
            &providerValidation,
            &providerValidation});
    runner.Expect(
        appProviderModuleTools.AssetProviders().size() == 1,
        "app provider tool modules should publish asset provider descriptors");
    runner.Expect(
        appProviderModuleTools.PropertyAccessors().size() == 4,
        "app provider tool modules should publish authoring and preview property accessors");
    runner.Expect(
        appProviderModuleTools.ValidationAdapters().size() == 4,
        "app provider tool modules should publish validation adapters");
    runner.Expect(
        appProviderModuleTools.RuntimeWatchProviders().size() == 1,
        "app provider tool modules should publish runtime watch providers");
    runner.Expect(
        appProviderModuleTools.ErrorCount() == 0,
        "app provider tool modules should register without diagnostics errors");
}

void TestPanelLayoutGeometry(RegressionRunner& runner) {
    EditorPanelLayoutService layout;
    EditorPanelLayoutConfig config{};
    config.developerToolsVisible = true;
    config.workX = 0.0f;
    config.workY = 0.0f;
    config.workWidth = 1600.0f;
    config.workHeight = 900.0f;
    config.topReservedHeight = 96.0f;
    config.bottomReservedHeight = 24.0f;
    layout.Configure(config);

    runner.Expect(layout.ViewportRect().Valid(), "viewport rect should be valid");
    runner.Expect(layout.BottomDockRect().Valid(), "bottom dock rect should be valid");
    runner.Expect(layout.InspectorRect().Valid(), "inspector rect should be valid");
    runner.Expect(
        layout.ViewportRect().x >= layout.LeftSidebarRect().x + layout.LeftSidebarRect().width,
        "viewport should start after left sidebar");
    runner.Expect(
        layout.ViewportRect().x + layout.ViewportRect().width <= layout.InspectorRect().x,
        "viewport should end before inspector");
    runner.Expect(
        layout.BottomDockRect().y >= layout.ViewportRect().y + layout.ViewportRect().height,
        "bottom dock should be below viewport");
}

void TestFeatureGuardTripwire(RegressionRunner& runner) {
    const ExistingFeatureProtectionReport emptyReport =
        BuildExistingFeatureProtectionReport(ExistingFeatureProtectionInput{});
    runner.Expect(!emptyReport.Healthy(), "empty feature guard input should report blocked checks");
    runner.Expect(emptyReport.blockedCount > 0, "feature guard should count blocked checks");
    runner.Expect(!emptyReport.checks.empty(), "feature guard should emit detailed checks");
}

} // namespace

int RunEditorCoreRegressionTests() {
    std::ofstream log("editor_core_regression.log", std::ios::trunc);
    if (!log) {
        return 2;
    }

    RegressionRunner runner(log);
    const std::vector<RegressionCase> tests{
        {"transaction stack undo/redo", [&]() { TestTransactionStack(runner); }},
        {"selection and property registry", [&]() { TestSelectionAndPropertyRegistry(runner); }},
        {"property edit service", [&]() { TestPropertyEditService(runner); }},
        {"production property adapters", [&]() { TestProductionPropertyAdapters(runner); }},
        {"details section providers", [&]() { TestDetailsSectionProviders(runner); }},
        {"runtime watch builder", [&]() { TestRuntimeWatchBuilder(runner); }},
        {"play session lifecycle service", [&]() { TestPlaySessionLifecycleService(runner); }},
        {"play session runtime control service", [&]() { TestPlaySessionRuntimeControlService(runner); }},
        {"runtime authoring apply service", [&]() { TestRuntimeAuthoringApplyService(runner); }},
        {"asset registry and mutation safety", [&]() { TestAssetRegistryAndMutationSafety(runner); }},
        {"asset migration pipeline", [&]() { TestAssetMigrationPipeline(runner); }},
        {"asset import reimport pipeline", [&]() { TestAssetImportReimportPipeline(runner); }},
        {"layout persistence", [&]() { TestLayoutPersistence(runner); }},
        {"editor tool registration descriptors", [&]() { TestEditorToolRegistrationDescriptors(runner); }},
        {"panel layout geometry", [&]() { TestPanelLayoutGeometry(runner); }},
        {"feature guard tripwire", [&]() { TestFeatureGuardTripwire(runner); }},
    };

    log << "Editor Core Regression Tests\n";
    log << "caseCount=" << tests.size() << '\n';
    for (const RegressionCase& test : tests) {
        runner.Run(test);
    }
    log << "summary cases=" << runner.CaseCount()
        << " failed=" << runner.FailedCount()
        << " result=" << (runner.FailedCount() == 0 ? "ok" : "failed")
        << '\n';

    return runner.FailedCount() == 0 ? 0 : 1;
}

} // namespace editor
