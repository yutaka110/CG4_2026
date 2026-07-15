#include "EditorTransactionStack.h"

#include <algorithm>
#include <exception>
#include <limits>
#include <utility>

namespace editor {
namespace {

std::size_t StringBytes(const std::string& value) {
    return value.capacity() + 1;
}

std::size_t HandleBytes(const EditorObjectHandle& handle) {
    return sizeof(EditorObjectHandle) +
        StringBytes(handle.stableId) +
        StringBytes(handle.displayName);
}

std::size_t PropertyChangeBytes(const EditorPropertyChange& change) {
    return sizeof(EditorPropertyChange) +
        HandleBytes(change.target) +
        StringBytes(change.propertyPath) +
        StringBytes(change.displayName) +
        StringBytes(change.valueType) +
        StringBytes(change.beforeValue) +
        StringBytes(change.afterValue);
}

class TransactionBusyGuard final {
public:
    explicit TransactionBusyGuard(bool& busy)
        : busy_(busy) {
        busy_ = true;
    }

    ~TransactionBusyGuard() {
        busy_ = false;
    }

    TransactionBusyGuard(const TransactionBusyGuard&) = delete;
    TransactionBusyGuard& operator=(const TransactionBusyGuard&) = delete;

private:
    bool& busy_;
};

} // namespace

EditorTransactionStack::~EditorTransactionStack() {
}

void EditorTransactionStack::Clear() {
    if (busy_) {
        return;
    }
    const bool hadState = !undoStack_.empty() || !redoStack_.empty() || !stagedPropertyChanges_.empty();
    undoStack_.clear();
    redoStack_.clear();
    stagedPropertyChanges_.clear();
    historyBytes_ = 0;
    if (hadState) {
        Touch();
    }
}

void EditorTransactionStack::SetMaxHistory(std::size_t maxHistory) {
    if (busy_) {
        return;
    }
    maxHistory_ = (std::max<std::size_t>)(1, maxHistory);
    TrimUndoHistory();
}

bool EditorTransactionStack::SetMemoryBudgetBytes(
    std::size_t bytes,
    EditorError* error) {
    if (busy_) {
        SetEditorError(
            error,
            EditorErrorCode::Busy,
            "Transaction history budget cannot change during undo or redo.");
        return false;
    }
    if (!memoryBudget_.SetLimitBytes(bytes, error)) {
        return false;
    }
    TrimUndoHistory();
    return true;
}

bool EditorTransactionStack::PushCommand(
    std::string label,
    EditorObjectHandle target,
    EditorUndoCommandPtr command,
    EditorError* error) {
    if (!CanPushCommand(label, target, command, error)) return false;

    EditorTransactionRecord record{};
    record.id = nextId_;
    record.label = label.empty() ? std::string(command->TypeId()) : std::move(label);
    record.target = std::move(target);
    record.payload.kind = EditorTransactionPayloadKind::Command;
    record.payload.beforeSummary = "undo:" + std::string(command->TypeId());
    record.payload.afterSummary = "redo:" + std::string(command->TypeId());
    record.command = std::move(command);
    record.estimatedBytes = EstimateRecordBytes(record);
    if (!PushRecord(std::move(record), true, error)) {
        return false;
    }
    ++nextId_;
    return true;
}

bool EditorTransactionStack::CanPushCommand(
    const std::string& label,
    const EditorObjectHandle& target,
    const EditorUndoCommandPtr& command,
    EditorError* error) const {
    ClearEditorError(error);
    if (busy_) {
        SetEditorError(error, EditorErrorCode::Busy, "A transaction cannot be registered during undo or redo.");
        return false;
    }
    if (command == nullptr) {
        SetEditorError(error, EditorErrorCode::InvalidArgument, "Transaction command is null.");
        return false;
    }
    if (command->DomainId().empty() || command->TypeId().empty()) {
        SetEditorError(error, EditorErrorCode::InvalidArgument, "Transaction command domain id and type id are required.");
        return false;
    }
    EditorTransactionRecord record{};
    record.label = label.empty() ? std::string(command->TypeId()) : label;
    record.target = target;
    record.payload.kind = EditorTransactionPayloadKind::Command;
    record.payload.beforeSummary = "undo:" + std::string(command->TypeId());
    record.payload.afterSummary = "redo:" + std::string(command->TypeId());
    record.command = command;
    record.estimatedBytes = EstimateRecordBytes(record);
    if (!memoryBudget_.AcceptsSingleRecord(record.estimatedBytes)) {
        SetEditorError(error, EditorErrorCode::MemoryBudgetExceeded, "Transaction command exceeds the history memory budget.");
        return false;
    }
    return true;
}

void EditorTransactionStack::PushSnapshot(
    std::string label,
    EditorObjectHandle target,
    std::string beforeSummary,
    std::string afterSummary) {
    EditorTransactionRecord record{};
    record.id = nextId_++;
    record.label = std::move(label);
    record.target = std::move(target);
    record.payload.kind = EditorTransactionPayloadKind::Snapshot;
    record.payload.beforeSummary = std::move(beforeSummary);
    record.payload.afterSummary = std::move(afterSummary);

    PushRecord(std::move(record), false);
}

void EditorTransactionStack::PushPropertyDelta(
    std::string label,
    EditorObjectHandle target,
    std::string propertyPath,
    std::string valueType,
    std::string beforeValue,
    std::string afterValue) {
    EditorTransactionRecord record{};
    record.id = nextId_++;
    record.label = std::move(label);
    record.target = std::move(target);
    record.payload.kind = EditorTransactionPayloadKind::PropertyDelta;
    record.payload.propertyPath = std::move(propertyPath);
    record.payload.valueType = std::move(valueType);
    record.payload.beforeSummary = std::move(beforeValue);
    record.payload.afterSummary = std::move(afterValue);

    PushRecord(std::move(record), false);
}

void EditorTransactionStack::PushMultiPropertyDelta(
    std::string label,
    EditorObjectHandle target,
    std::vector<EditorPropertyChange> changes) {
    if (changes.empty()) {
        return;
    }
    if (changes.size() == 1) {
        EditorPropertyChange change = std::move(changes.front());
        PushPropertyDelta(
            std::move(label),
            std::move(change.target),
            std::move(change.propertyPath),
            std::move(change.valueType),
            std::move(change.beforeValue),
            std::move(change.afterValue));
        return;
    }

    EditorTransactionRecord record{};
    record.id = nextId_++;
    record.label = std::move(label);
    record.target = std::move(target);
    record.payload.kind = EditorTransactionPayloadKind::MultiPropertyDelta;
    record.payload.propertyChanges = std::move(changes);

    PushRecord(std::move(record), false);
}

void EditorTransactionStack::StagePropertyDelta(EditorPropertyChange change) {
    if (busy_) {
        return;
    }
    stagedPropertyChanges_.clear();
    stagedPropertyChanges_.push_back(std::move(change));
}

void EditorTransactionStack::StagePropertyDeltas(std::vector<EditorPropertyChange> changes) {
    if (busy_) {
        return;
    }
    stagedPropertyChanges_ = std::move(changes);
}

const EditorPropertyChange* EditorTransactionStack::StagedPropertyDelta() const {
    return stagedPropertyChanges_.empty() ? nullptr : &stagedPropertyChanges_.front();
}

EditorPropertyChange EditorTransactionStack::ConsumeStagedPropertyDelta() {
    EditorPropertyChange change{};
    if (!stagedPropertyChanges_.empty()) {
        change = std::move(stagedPropertyChanges_.front());
        stagedPropertyChanges_.clear();
    }
    return change;
}

std::vector<EditorPropertyChange> EditorTransactionStack::ConsumeStagedPropertyDeltas() {
    std::vector<EditorPropertyChange> changes = std::move(stagedPropertyChanges_);
    stagedPropertyChanges_.clear();
    return changes;
}

bool EditorTransactionStack::Undo(const ApplyCallback& apply) {
    if (busy_ || undoStack_.empty()) {
        return false;
    }

    EditorTransactionRecord record = undoStack_.back();
    {
        TransactionBusyGuard guard(busy_);
        try {
            if (apply && !apply(record, EditorTransactionApplyMode::Undo)) {
                return false;
            }
        } catch (...) {
            return false;
        }
    }

    undoStack_.pop_back();
    redoStack_.push_back(std::move(record));
    Touch();
    return true;
}

const EditorTransactionRecord* EditorTransactionStack::NextUndoTransaction() const {
    return undoStack_.empty() ? nullptr : &undoStack_.back();
}

const EditorTransactionRecord* EditorTransactionStack::NextRedoTransaction() const {
    return redoStack_.empty() ? nullptr : &redoStack_.back();
}

bool EditorTransactionStack::Redo(const ApplyCallback& apply) {
    if (busy_ || redoStack_.empty()) {
        return false;
    }

    EditorTransactionRecord record = redoStack_.back();
    {
        TransactionBusyGuard guard(busy_);
        try {
            if (apply && !apply(record, EditorTransactionApplyMode::Redo)) {
                return false;
            }
        } catch (...) {
            return false;
        }
    }

    redoStack_.pop_back();
    undoStack_.push_back(std::move(record));
    Touch();
    return true;
}

bool EditorTransactionStack::Undo(
    EditorExecutionContext& context,
    EditorError* error) {
    return ApplyCommand(true, context, error);
}

bool EditorTransactionStack::Redo(
    EditorExecutionContext& context,
    EditorError* error) {
    return ApplyCommand(false, context, error);
}

const EditorTransactionRecord* EditorTransactionStack::LastTransaction() const {
    if (!undoStack_.empty()) {
        return &undoStack_.back();
    }
    if (!redoStack_.empty()) {
        return &redoStack_.back();
    }
    return nullptr;
}

void EditorTransactionStack::SetLegacyMirror(
    std::string label,
    uint32_t undoDepth,
    uint32_t redoDepth,
    uint32_t revision) {
    if (legacyMirror_.active &&
        legacyMirror_.label == label &&
        legacyMirror_.undoDepth == undoDepth &&
        legacyMirror_.redoDepth == redoDepth &&
        legacyMirror_.revision == revision) {
        return;
    }

    legacyMirror_.active = true;
    legacyMirror_.label = std::move(label);
    legacyMirror_.undoDepth = undoDepth;
    legacyMirror_.redoDepth = redoDepth;
    legacyMirror_.revision = revision;
}

void EditorTransactionStack::Touch() {
    ++revision_;
}

bool EditorTransactionStack::PushRecord(
    EditorTransactionRecord record,
    bool enforceSingleRecordBudget,
    EditorError* error) {
    ClearEditorError(error);
    if (busy_) {
        SetEditorError(
            error,
            EditorErrorCode::Busy,
            "A transaction cannot be registered during undo or redo.");
        return false;
    }
    if (record.estimatedBytes == 0) {
        record.estimatedBytes = EstimateRecordBytes(record);
    }
    if (enforceSingleRecordBudget &&
        !memoryBudget_.AcceptsSingleRecord(record.estimatedBytes)) {
        SetEditorError(
            error,
            EditorErrorCode::MemoryBudgetExceeded,
            "Transaction command exceeds the history memory budget.");
        return false;
    }

    ClearRedoHistory();
    historyBytes_ += record.estimatedBytes;
    undoStack_.push_back(std::move(record));
    TrimUndoHistory();
    Touch();
    return true;
}

bool EditorTransactionStack::ApplyCommand(
    bool undo,
    EditorExecutionContext& context,
    EditorError* error) {
    ClearEditorError(error);
    if (busy_) {
        SetEditorError(error, EditorErrorCode::Busy, "Undo/redo is already in progress.");
        return false;
    }

    std::vector<EditorTransactionRecord>& source = undo ? undoStack_ : redoStack_;
    std::vector<EditorTransactionRecord>& destination = undo ? redoStack_ : undoStack_;
    if (source.empty()) {
        SetEditorError(
            error,
            EditorErrorCode::NotAvailable,
            undo ? "No transaction is available to undo." : "No transaction is available to redo.");
        return false;
    }
    const EditorTransactionRecord& record = source.back();
    if (record.command == nullptr) {
        SetEditorError(
            error,
            EditorErrorCode::InvalidArgument,
            "The next transaction is a legacy payload and requires the legacy apply callback.");
        return false;
    }

    EditorUndoResult result{};
    {
        TransactionBusyGuard guard(busy_);
        try {
            result = record.command->Apply(
                undo ? EditorTransactionApplyMode::Undo : EditorTransactionApplyMode::Redo,
                context);
        } catch (const std::exception& exception) {
            result = EditorUndoResult::Failure(EditorErrorCode::ApplyFailed, exception.what());
        } catch (...) {
            result = EditorUndoResult::Failure(
                EditorErrorCode::ApplyFailed,
                "Transaction command threw an unknown exception.");
        }
    }
    if (!result.succeeded) {
        SetEditorError(
            error,
            result.code == EditorErrorCode::None ? EditorErrorCode::ApplyFailed : result.code,
            result.message.empty() ? std::string("Transaction command apply failed.") : result.message);
        return false;
    }

    destination.push_back(std::move(source.back()));
    source.pop_back();
    Touch();
    return true;
}

void EditorTransactionStack::ClearRedoHistory() {
    for (const EditorTransactionRecord& record : redoStack_) {
        historyBytes_ = record.estimatedBytes <= historyBytes_
            ? historyBytes_ - record.estimatedBytes
            : 0;
    }
    redoStack_.clear();
}

std::size_t EditorTransactionStack::EstimateRecordBytes(
    const EditorTransactionRecord& record) const {
    std::size_t bytes = sizeof(EditorTransactionRecord) +
        StringBytes(record.label) +
        HandleBytes(record.target) +
        StringBytes(record.payload.propertyPath) +
        StringBytes(record.payload.valueType) +
        StringBytes(record.payload.beforeSummary) +
        StringBytes(record.payload.afterSummary);
    for (const EditorPropertyChange& change : record.payload.propertyChanges) {
        bytes += PropertyChangeBytes(change);
    }
    if (record.command != nullptr) {
        const std::size_t commandBytes = record.command->EstimatedBytes();
        if (commandBytes > (std::numeric_limits<std::size_t>::max)() - bytes) {
            return (std::numeric_limits<std::size_t>::max)();
        }
        bytes += commandBytes;
    }
    return bytes;
}

void EditorTransactionStack::TrimUndoHistory() {
    while (!undoStack_.empty() &&
        (undoStack_.size() > maxHistory_ ||
            memoryBudget_.ExceededBy(historyBytes_))) {
        const std::size_t removedBytes = undoStack_.front().estimatedBytes;
        undoStack_.erase(undoStack_.begin());
        historyBytes_ = removedBytes <= historyBytes_
            ? historyBytes_ - removedBytes
            : 0;
    }
    while (memoryBudget_.ExceededBy(historyBytes_) && !redoStack_.empty()) {
        const std::size_t removedBytes = redoStack_.front().estimatedBytes;
        redoStack_.erase(redoStack_.begin());
        historyBytes_ = removedBytes <= historyBytes_
            ? historyBytes_ - removedBytes
            : 0;
    }
}

const char* ToString(EditorTransactionPayloadKind kind) {
    switch (kind) {
    case EditorTransactionPayloadKind::Snapshot:
        return "Snapshot";
    case EditorTransactionPayloadKind::PropertyDelta:
        return "PropertyDelta";
    case EditorTransactionPayloadKind::MultiPropertyDelta:
        return "MultiPropertyDelta";
    case EditorTransactionPayloadKind::Command:
        return "Command";
    }
    return "Unknown";
}

const char* ToString(EditorTransactionApplyMode mode) {
    switch (mode) {
    case EditorTransactionApplyMode::Undo:
        return "Undo";
    case EditorTransactionApplyMode::Redo:
        return "Redo";
    }
    return "Unknown";
}

} // namespace editor
