#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "core/EditorExecutionContext.h"
#include "core/EditorTransactionMemoryBudget.h"
#include "core/EditorUndoCommand.h"
#include "EditorSelection.h"

namespace editor {

enum class EditorTransactionPayloadKind {
    Snapshot,
    PropertyDelta,
    MultiPropertyDelta,
    Command,
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

struct EditorTransactionPayload {
    EditorTransactionPayloadKind kind = EditorTransactionPayloadKind::Snapshot;
    std::string propertyPath;
    std::string valueType;
    std::string beforeSummary;
    std::string afterSummary;
    std::vector<EditorPropertyChange> propertyChanges;
};

struct EditorTransactionRecord {
    uint64_t id = 0;
    std::string label;
    EditorObjectHandle target;
    EditorTransactionPayload payload;
    EditorUndoCommandPtr command;
    std::size_t estimatedBytes = 0;
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
    ~EditorTransactionStack();

    using ApplyCallback = std::function<bool(const EditorTransactionRecord&, EditorTransactionApplyMode)>;

    void Clear();
    void SetMaxHistory(std::size_t maxHistory);
    std::size_t MaxHistory() const { return maxHistory_; }
    bool SetMemoryBudgetBytes(std::size_t bytes, EditorError* error = nullptr);
    std::size_t MemoryBudgetBytes() const noexcept { return memoryBudget_.LimitBytes(); }
    std::size_t HistoryBytes() const noexcept { return historyBytes_; }
    bool Busy() const noexcept { return busy_; }

    bool PushCommand(
        std::string label,
        EditorObjectHandle target,
        EditorUndoCommandPtr command,
        EditorError* error = nullptr);
    bool CanPushCommand(
        const std::string& label,
        const EditorObjectHandle& target,
        const EditorUndoCommandPtr& command,
        EditorError* error = nullptr) const;

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
    void PushMultiPropertyDelta(
        std::string label,
        EditorObjectHandle target,
        std::vector<EditorPropertyChange> changes);

    void StagePropertyDelta(EditorPropertyChange change);
    void StagePropertyDeltas(std::vector<EditorPropertyChange> changes);
    bool HasStagedPropertyDelta() const { return !stagedPropertyChanges_.empty(); }
    std::size_t StagedPropertyDeltaCount() const { return stagedPropertyChanges_.size(); }
    const EditorPropertyChange* StagedPropertyDelta() const;
    const std::vector<EditorPropertyChange>& StagedPropertyDeltas() const { return stagedPropertyChanges_; }
    EditorPropertyChange ConsumeStagedPropertyDelta();
    std::vector<EditorPropertyChange> ConsumeStagedPropertyDeltas();

    bool CanUndo() const { return !undoStack_.empty(); }
    bool CanRedo() const { return !redoStack_.empty(); }
    const EditorTransactionRecord* NextUndoTransaction() const;
    const EditorTransactionRecord* NextRedoTransaction() const;
    bool Undo(const ApplyCallback& apply);
    bool Redo(const ApplyCallback& apply);
    bool Undo(EditorExecutionContext& context, EditorError* error = nullptr);
    bool Redo(EditorExecutionContext& context, EditorError* error = nullptr);

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
    bool PushRecord(
        EditorTransactionRecord record,
        bool enforceSingleRecordBudget,
        EditorError* error = nullptr);
    bool ApplyCommand(
        bool undo,
        EditorExecutionContext& context,
        EditorError* error);
    void ClearRedoHistory();
    std::size_t EstimateRecordBytes(const EditorTransactionRecord& record) const;
    void Touch();
    void TrimUndoHistory();

    std::vector<EditorTransactionRecord> undoStack_;
    std::vector<EditorTransactionRecord> redoStack_;
    std::size_t maxHistory_ = 128;
    EditorTransactionMemoryBudget memoryBudget_{};
    std::size_t historyBytes_ = 0;
    uint64_t nextId_ = 1;
    uint32_t revision_ = 0;
    bool busy_ = false;
    EditorTransactionLegacyMirror legacyMirror_{};
    std::vector<EditorPropertyChange> stagedPropertyChanges_;
};

const char* ToString(EditorTransactionPayloadKind kind);
const char* ToString(EditorTransactionApplyMode mode);

} // namespace editor
