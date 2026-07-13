#include "EditorTransactionStack.h"

#include <algorithm>
#include <utility>

namespace editor {

void EditorTransactionStack::Clear() {
    const bool hadState = !undoStack_.empty() || !redoStack_.empty() || !stagedPropertyChanges_.empty();
    undoStack_.clear();
    redoStack_.clear();
    stagedPropertyChanges_.clear();
    if (hadState) {
        Touch();
    }
}

void EditorTransactionStack::SetMaxHistory(std::size_t maxHistory) {
    maxHistory_ = (std::max<std::size_t>)(1, maxHistory);
    TrimUndoHistory();
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

    undoStack_.push_back(std::move(record));
    TrimUndoHistory();
    redoStack_.clear();
    Touch();
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

    undoStack_.push_back(std::move(record));
    TrimUndoHistory();
    redoStack_.clear();
    Touch();
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

    undoStack_.push_back(std::move(record));
    TrimUndoHistory();
    redoStack_.clear();
    Touch();
}

void EditorTransactionStack::PushAssetMutation(
    std::string label,
    EditorObjectHandle target,
    EditorAssetMutationChange change) {
    EditorTransactionRecord record{};
    record.id = nextId_++;
    record.label = std::move(label);
    record.target = std::move(target);
    record.payload.kind = EditorTransactionPayloadKind::AssetMutation;
    record.payload.assetMutation = std::move(change);
    record.payload.beforeSummary = record.payload.assetMutation.beforeRecord.id;
    record.payload.afterSummary =
        record.payload.assetMutation.kind == EditorAssetMutationKind::Delete
            ? std::string("<deleted>")
            : record.payload.assetMutation.afterRecord.id;

    undoStack_.push_back(std::move(record));
    TrimUndoHistory();
    redoStack_.clear();
    Touch();
}

void EditorTransactionStack::PushRuntimeAuthoringApply(
    std::string label,
    EditorObjectHandle target,
    EditorRuntimeAuthoringApplyChange change) {
    EditorTransactionRecord record{};
    record.id = nextId_++;
    record.label = std::move(label);
    record.target = std::move(target);
    record.payload.kind = EditorTransactionPayloadKind::RuntimeAuthoringApply;
    record.payload.beforeSummary =
        "Course rev " + std::to_string(change.beforeTerrain.courseObjectEditRevision);
    record.payload.afterSummary =
        "Course rev " + std::to_string(change.afterTerrain.courseObjectEditRevision);
    record.payload.runtimeAuthoringApply = std::move(change);

    undoStack_.push_back(std::move(record));
    TrimUndoHistory();
    redoStack_.clear();
    Touch();
}

void EditorTransactionStack::StagePropertyDelta(EditorPropertyChange change) {
    stagedPropertyChanges_.clear();
    stagedPropertyChanges_.push_back(std::move(change));
}

void EditorTransactionStack::StagePropertyDeltas(std::vector<EditorPropertyChange> changes) {
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
    if (undoStack_.empty()) {
        return false;
    }

    EditorTransactionRecord record = undoStack_.back();
    if (apply && !apply(record, EditorTransactionApplyMode::Undo)) {
        return false;
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
    if (redoStack_.empty()) {
        return false;
    }

    EditorTransactionRecord record = redoStack_.back();
    if (apply && !apply(record, EditorTransactionApplyMode::Redo)) {
        return false;
    }

    redoStack_.pop_back();
    undoStack_.push_back(std::move(record));
    Touch();
    return true;
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

void EditorTransactionStack::TrimUndoHistory() {
    if (undoStack_.size() <= maxHistory_) {
        return;
    }
    const std::size_t removeCount = undoStack_.size() - maxHistory_;
    undoStack_.erase(undoStack_.begin(), undoStack_.begin() + static_cast<std::ptrdiff_t>(removeCount));
}

const char* ToString(EditorTransactionPayloadKind kind) {
    switch (kind) {
    case EditorTransactionPayloadKind::Snapshot:
        return "Snapshot";
    case EditorTransactionPayloadKind::PropertyDelta:
        return "PropertyDelta";
    case EditorTransactionPayloadKind::MultiPropertyDelta:
        return "MultiPropertyDelta";
    case EditorTransactionPayloadKind::AssetMutation:
        return "AssetMutation";
    case EditorTransactionPayloadKind::RuntimeAuthoringApply:
        return "RuntimeAuthoringApply";
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
