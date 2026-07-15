#pragma once

#include "EditorGeometryMesh.h"
#include "../core/EditorExecutionService.h"
#include "../core/EditorUndoCommand.h"
#include "../documents/EditorDocumentId.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace editor {

struct EditorGeometryPropertyState {
    std::optional<std::string> geometry;
    std::optional<std::string> collision;
};

class IEditorGeometryExecutionService : public IEditorExecutionService {
public:
    static constexpr std::string_view kServiceId = "editor.geometry.edit";

    std::string_view ServiceId() const noexcept final { return kServiceId; }
    virtual EditorUndoResult ApplyGeometryState(
        std::string_view documentKey,
        std::string_view entityGuid,
        const EditorGeometryPropertyState& state) = 0;
};

class EditorScene;

class EditorGeometryExecutionService final : public IEditorGeometryExecutionService {
public:
    using ChangedCallback = std::function<void(std::string_view)>;

    void Bind(
        EditorDocumentId document,
        EditorScene* scene,
        ChangedCallback onChanged = {});
    void Clear();

    EditorUndoResult ApplyGeometryState(
        std::string_view documentKey,
        std::string_view entityGuid,
        const EditorGeometryPropertyState& state) override;

private:
    EditorDocumentId document_{};
    EditorScene* scene_ = nullptr;
    ChangedCallback onChanged_{};
};

class EditorGeometryEditUndoCommand final : public IEditorUndoCommand {
public:
    EditorGeometryEditUndoCommand(
        std::string documentKey,
        std::string entityGuid,
        EditorGeometryPropertyState before,
        EditorGeometryPropertyState after);

    EditorUndoResult Apply(
        EditorTransactionApplyMode mode,
        EditorExecutionContext& context) const override;
    std::size_t EstimatedBytes() const noexcept override;
    std::string_view DomainId() const noexcept override { return "geometry.edit"; }
    std::string_view TypeId() const noexcept override { return "geometry.property-state"; }

private:
    std::string documentKey_;
    std::string entityGuid_;
    EditorGeometryPropertyState before_;
    EditorGeometryPropertyState after_;
};

} // namespace editor
