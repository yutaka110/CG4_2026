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
#include "EditorLayoutPersistenceService.h"
#include "EditorPanelLayoutService.h"
#include "EditorPanelRegistry.h"
#include "EditorPropertyEditSession.h"
#include "EditorPropertyEditService.h"
#include "EditorPropertyRegistry.h"
#include "EditorSelection.h"
#include "EditorTransactionStack.h"
#include "EditorValidationService.h"
#include "ExistingFeatureProtection.h"

#include <chrono>
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
        {"asset registry and mutation safety", [&]() { TestAssetRegistryAndMutationSafety(runner); }},
        {"asset migration pipeline", [&]() { TestAssetMigrationPipeline(runner); }},
        {"asset import reimport pipeline", [&]() { TestAssetImportReimportPipeline(runner); }},
        {"layout persistence", [&]() { TestLayoutPersistence(runner); }},
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
