#pragma once

#include "EditorBlenderSceneImportService.h"
#include "../core/EditorExecutionService.h"
#include "../core/EditorUndoCommand.h"
#include "../documents/EditorDocumentId.h"

#include <functional>
#include <string>
#include <string_view>

namespace editor {

class EditorAssetRegistry;
class EditorDirtyStateService;
class EditorDocumentManager;
class EditorSceneDocumentProvider;
class EditorTransactionStack;

inline constexpr std::string_view kEditorBlenderSceneImportExecutionServiceId =
    "editor.blender-scene-import.execution";

class IEditorBlenderSceneImportExecutionService
    : public IEditorExecutionService {
public:
    std::string_view ServiceId() const noexcept final {
        return kEditorBlenderSceneImportExecutionServiceId;
    }

    virtual EditorUndoResult ApplyBlenderSceneSnapshot(
        const EditorDocumentId& document,
        const EditorScene& snapshot,
        std::string_view reason) = 0;
};

class EditorBlenderSceneImportUndoCommand final
    : public IEditorUndoCommand {
public:
    EditorBlenderSceneImportUndoCommand(
        EditorDocumentId document,
        EditorScene before,
        EditorScene after);

    EditorUndoResult Apply(
        EditorTransactionApplyMode mode,
        EditorExecutionContext& context) const override;
    std::size_t EstimatedBytes() const noexcept override;
    std::string_view DomainId() const noexcept override {
        return "blender-scene-import";
    }
    std::string_view TypeId() const noexcept override {
        return "blender-scene-import.snapshot";
    }

    const EditorDocumentId& Document() const noexcept { return document_; }

private:
    EditorDocumentId document_;
    EditorScene before_;
    EditorScene after_;
};

class EditorBlenderSceneImportExecutionService final
    : public IEditorBlenderSceneImportExecutionService {
public:
    using SceneChangedCallback =
        std::function<void(const EditorDocumentId&, std::string_view)>;

    EditorBlenderSceneImportExecutionService(
        EditorSceneDocumentProvider& scenes,
        EditorDocumentManager* documents,
        EditorDirtyStateService* dirtyState,
        SceneChangedCallback sceneChanged = {});

    EditorUndoResult ApplyBlenderSceneSnapshot(
        const EditorDocumentId& document,
        const EditorScene& snapshot,
        std::string_view reason) override;

private:
    EditorSceneDocumentProvider& scenes_;
    EditorDocumentManager* documents_ = nullptr;
    EditorDirtyStateService* dirtyState_ = nullptr;
    SceneChangedCallback sceneChanged_;
};

struct EditorBlenderSceneImportTransactionResult {
    bool succeeded = false;
    bool warning = false;
    std::string message;
    EditorBlenderSceneImportResult importResult;
};

class EditorBlenderSceneImportWorkflow {
public:
    using SceneChangedCallback =
        std::function<void(const EditorDocumentId&, std::string_view)>;

    EditorBlenderSceneImportWorkflow(
        EditorSceneDocumentProvider& scenes,
        EditorTransactionStack& transactions,
        const EditorAssetRegistry* assets,
        EditorDocumentManager* documents,
        EditorDirtyStateService* dirtyState,
        SceneChangedCallback sceneChanged = {});

    EditorBlenderSceneImportTransactionResult Execute(
        EditorBlenderSceneImportMode mode,
        const EditorDocumentId& document,
        const std::filesystem::path& sourcePath,
        const EditorBlenderSceneImportOptions& options = {});

    bool HasImportedScene(const EditorDocumentId& document) const;

private:
    EditorSceneDocumentProvider& scenes_;
    EditorTransactionStack& transactions_;
    const EditorAssetRegistry* assets_ = nullptr;
    EditorDocumentManager* documents_ = nullptr;
    EditorDirtyStateService* dirtyState_ = nullptr;
    SceneChangedCallback sceneChanged_;
};

} // namespace editor
