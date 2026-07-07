#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "EditorSelection.h"

namespace editor {

enum class EditorTransactionPayloadKind {
    Snapshot,
    PropertyDelta,
};

enum class EditorTransactionApplyMode {
    Undo,
    Redo,
};

struct EditorTransactionPayload {
    EditorTransactionPayloadKind kind = EditorTransactionPayloadKind::Snapshot;
    std::string propertyPath;
    std::string valueType;
    std::string beforeSummary;
    std::string afterSummary;
};

struct EditorPropertyChange {
    EditorObjectHandle target;
    std::string propertyPath;
    std::string displayName;
    std::string valueType;
    std::string beforeValue;
    std::string afterValue;
    uint32_t sourceRevision = 0;
};

struct EditorTransactionRecord {
    uint64_t id = 0;
    std::string label;
    EditorObjectHandle target;
    EditorTransactionPayload payload;
};

struct EditorTransactionLegacyMirror {
    bool active = false;
    std::string label;
    uint32_t undoDepth = 0;
    uint32_t redoDepth = 0;
    uint32_t revision = 0;
};

class EditorTransactionStack {
public:
    using ApplyCallback = std::function<bool(const EditorTransactionRecord&, EditorTransactionApplyMode)>;

    void Clear();
    void SetMaxHistory(std::size_t maxHistory);
    std::size_t MaxHistory() const { return maxHistory_; }

    void PushSnapshot(
        std::string label,
        EditorObjectHandle target,
        std::string beforeSummary,
        std::string afterSummary);
    void PushPropertyDelta(
        std::string label,
        EditorObjectHandle target,
        std::string propertyPath,
        std::string valueType,
        std::string beforeValue,
        std::string afterValue);

    void StagePropertyDelta(EditorPropertyChange change);
    bool HasStagedPropertyDelta() const { return stagedPropertyChange_.has_value(); }
    const EditorPropertyChange* StagedPropertyDelta() const;
    EditorPropertyChange ConsumeStagedPropertyDelta();

    bool CanUndo() const { return !undoStack_.empty(); }
    bool CanRedo() const { return !redoStack_.empty(); }
    bool Undo(const ApplyCallback& apply);
    bool Redo(const ApplyCallback& apply);

    std::size_t UndoDepth() const { return undoStack_.size(); }
    std::size_t RedoDepth() const { return redoStack_.size(); }
    uint32_t Revision() const { return revision_; }
    const EditorTransactionRecord* LastTransaction() const;

    void SetLegacyMirror(
        std::string label,
        uint32_t undoDepth,
        uint32_t redoDepth,
        uint32_t revision);
    const EditorTransactionLegacyMirror& LegacyMirror() const { return legacyMirror_; }

private:
    void Touch();
    void TrimUndoHistory();

    std::vector<EditorTransactionRecord> undoStack_;
    std::vector<EditorTransactionRecord> redoStack_;
    std::size_t maxHistory_ = 128;
    uint64_t nextId_ = 1;
    uint32_t revision_ = 0;
    EditorTransactionLegacyMirror legacyMirror_{};
    std::optional<EditorPropertyChange> stagedPropertyChange_;
};

const char* ToString(EditorTransactionPayloadKind kind);
const char* ToString(EditorTransactionApplyMode mode);

} // namespace editor
