#include "EditorPropertyEditService.h"

#include <algorithm>
#include <sstream>
#include <utility>

namespace editor {
namespace {

EditorPropertyEditResult MakeResult(
    bool applied,
    bool changed,
    std::string message,
    std::string beforeValue = {},
    std::string afterValue = {}) {
    EditorPropertyEditResult result{};
    result.applied = applied;
    result.changed = changed;
    result.message = std::move(message);
    result.beforeValue = std::move(beforeValue);
    result.afterValue = std::move(afterValue);
    return result;
}

EditorPropertyEditResult Fail(
    const EditorPropertyEditRequest& request,
    std::string message) {
    if (request.notifyOnFailure && request.notifications != nullptr) {
        request.notifications->Push(
            EditorNotificationSeverity::Error,
            request.source != nullptr ? request.source : "editor.propertyEdit",
            message);
    }
    return MakeResult(false, false, std::move(message));
}

EditorPropertyBatchEditResult MakeBatchResult(
    bool applied,
    bool changed,
    std::string message,
    std::vector<EditorPropertyChange> changes = {}) {
    EditorPropertyBatchEditResult result{};
    result.applied = applied;
    result.changed = changed;
    result.message = std::move(message);
    result.changedCount = changes.size();
    result.changes = std::move(changes);
    return result;
}

EditorPropertyBatchEditResult FailBatch(
    const EditorPropertyBatchEditRequest& request,
    std::string message) {
    if (request.notifyOnFailure && request.notifications != nullptr) {
        request.notifications->Push(
            EditorNotificationSeverity::Error,
            request.source != nullptr ? request.source : "editor.propertyEdit",
            message);
    }
    return MakeBatchResult(false, false, std::move(message));
}

EditorPropertyApplyDeltaResult MakeDeltaResult(
    bool applied,
    bool changed,
    std::string message,
    std::string appliedValue = {}) {
    EditorPropertyApplyDeltaResult result{};
    result.applied = applied;
    result.changed = changed;
    result.message = std::move(message);
    result.appliedValue = std::move(appliedValue);
    return result;
}

EditorPropertyApplyDeltaResult FailDelta(
    const EditorPropertyApplyDeltaRequest& request,
    std::string message) {
    if (request.notifyOnFailure && request.notifications != nullptr) {
        request.notifications->Push(
            EditorNotificationSeverity::Error,
            request.source != nullptr ? request.source : "editor.propertyEdit",
            message);
    }
    return MakeDeltaResult(false, false, std::move(message));
}

void StagePropertyDelta(
    EditorTransactionStack* transactions,
    const EditorObjectHandle& target,
    const EditorPropertyDescriptor& descriptor,
    const std::string& beforeSummary,
    const std::string& afterSummary) {
    if (transactions == nullptr || beforeSummary == afterSummary) {
        return;
    }

    EditorPropertyChange change{};
    change.target = target;
    change.propertyPath = descriptor.name;
    change.displayName = descriptor.displayName;
    change.valueType = descriptor.valueType;
    change.beforeValue = beforeSummary;
    change.afterValue = afterSummary;
    change.sourceRevision = target.generation;
    transactions->StagePropertyDelta(std::move(change));
}

void MarkPropertyDirty(
    const EditorPropertyEditRequest& request,
    const std::string& beforeSummary,
    const std::string& afterSummary) {
    if (request.dirtyState == nullptr || request.descriptor == nullptr) {
        return;
    }

    const std::string dirtyId = BuildEditorPropertyEditDirtyId(request.target);
    const bool wasDirty = request.dirtyState->IsDirty(dirtyId);

    std::ostringstream reason;
    reason << "Property edit " << request.descriptor->displayName
           << " (" << request.descriptor->name << "): "
           << beforeSummary << " -> " << afterSummary;

    request.dirtyState->MarkDirty(
        IsEditorPropertyEditCourseAuthoringDomain(request.target.domain)
            ? EditorDirtyDomain::CourseAuthoring
            : EditorDirtyDomain::Property,
        dirtyId,
        BuildEditorPropertyEditDirtyLabel(request.target),
        reason.str(),
        request.transactions != nullptr ? request.transactions->Revision() : request.target.generation);

    if (!wasDirty && request.notifications != nullptr) {
        request.notifications->Push(
            EditorNotificationSeverity::Info,
            request.source != nullptr ? request.source : "editor.propertyEdit",
            BuildEditorPropertyEditDirtyLabel(request.target) + " is now dirty from property editing.");
    }
}

void MarkBatchDirty(
    const EditorPropertyBatchEditRequest& request,
    const std::vector<EditorPropertyChange>& changes) {
    if (request.dirtyState == nullptr || changes.empty()) {
        return;
    }

    const EditorObjectHandle& target =
        request.transactionTarget.domain != EditorDomainId::Unknown
            ? request.transactionTarget
            : changes.front().target;
    const std::string dirtyId = BuildEditorPropertyEditDirtyId(target);
    const bool wasDirty = request.dirtyState->IsDirty(dirtyId);

    std::ostringstream reason;
    reason << "Batch property edit";
    if (!request.label.empty()) {
        reason << " " << request.label;
    }
    reason << " (" << changes.size() << " changes)";
    for (const EditorPropertyChange& change : changes) {
        reason << "; " << change.propertyPath
               << ": " << change.beforeValue
               << " -> " << change.afterValue;
    }

    request.dirtyState->MarkDirty(
        IsEditorPropertyEditCourseAuthoringDomain(target.domain)
            ? EditorDirtyDomain::CourseAuthoring
            : EditorDirtyDomain::Property,
        dirtyId,
        BuildEditorPropertyEditDirtyLabel(target),
        reason.str(),
        request.transactions != nullptr ? request.transactions->Revision() : target.generation);

    if (!wasDirty && request.notifications != nullptr) {
        request.notifications->Push(
            EditorNotificationSeverity::Info,
            request.source != nullptr ? request.source : "editor.propertyEdit",
            BuildEditorPropertyEditDirtyLabel(target) + " is now dirty from batch property editing.");
    }
}

void MarkDeltaDirty(
    const EditorPropertyApplyDeltaRequest& request,
    const EditorPropertyDescriptor& descriptor,
    const EditorObjectHandle& target,
    const std::string& appliedSummary) {
    if (request.dirtyState == nullptr) {
        return;
    }

    std::ostringstream reason;
    reason << "Property "
           << (request.mode == EditorTransactionApplyMode::Undo ? "undo" : "redo")
           << " " << descriptor.displayName
           << " (" << descriptor.name << "): "
           << appliedSummary;

    request.dirtyState->MarkDirty(
        IsEditorPropertyEditCourseAuthoringDomain(target.domain)
            ? EditorDirtyDomain::CourseAuthoring
            : EditorDirtyDomain::Property,
        BuildEditorPropertyEditDirtyId(target),
        BuildEditorPropertyEditDirtyLabel(target),
        reason.str(),
        request.transaction != nullptr ? static_cast<uint32_t>(request.transaction->id) : target.generation);
}

} // namespace

bool IsEditorPropertyEditCourseAuthoringDomain(EditorDomainId domain) {
    return domain == EditorDomainId::CourseTerrainPlacement ||
        domain == EditorDomainId::CourseRockCluster;
}

std::string BuildEditorPropertyEditDirtyId(const EditorObjectHandle& target) {
    if (IsEditorPropertyEditCourseAuthoringDomain(target.domain)) {
        return "course.authoring";
    }
    return target.stableId.empty() ? std::string("editor.property") : target.stableId;
}

std::string BuildEditorPropertyEditDirtyLabel(const EditorObjectHandle& target) {
    if (IsEditorPropertyEditCourseAuthoringDomain(target.domain)) {
        return "Course Authoring";
    }
    if (!target.displayName.empty()) {
        return target.displayName;
    }
    return target.stableId.empty() ? std::string("Property Edit") : target.stableId;
}

EditorPropertyEditResult EditorPropertyEditService::Apply(
    const EditorPropertyEditRequest& request) const {
    if (!request.canMutateAuthoring) {
        return Fail(request, "Authoring is locked during Play/Sim.");
    }
    if (request.accessor == nullptr) {
        return Fail(request, "Property accessor is unavailable.");
    }
    if (request.descriptor == nullptr) {
        return Fail(request, "Property descriptor is unavailable.");
    }
    if (request.descriptor->readOnly) {
        return Fail(request, "Property is read-only.");
    }

    EditorPropertyValue beforeValue{};
    if (!request.accessor->Get(request.target, *request.descriptor, beforeValue)) {
        return Fail(request, "Property is not accessible for the selected object.");
    }

    std::string setError;
    if (!request.accessor->Set(
            request.target,
            *request.descriptor,
            request.requestedValue,
            &setError)) {
        return Fail(
            request,
            setError.empty() ? std::string("Property edit was rejected.") : setError);
    }

    EditorPropertyValue afterValue{};
    if (!request.accessor->Get(request.target, *request.descriptor, afterValue)) {
        return Fail(request, "Property was changed but could not be read back.");
    }

    const std::string beforeSummary =
        FormatEditorPropertyValue(*request.descriptor, beforeValue);
    const std::string afterSummary =
        FormatEditorPropertyValue(*request.descriptor, afterValue);
    if (beforeSummary == afterSummary) {
        return MakeResult(
            true,
            false,
            "Property value was unchanged.",
            beforeSummary,
            afterSummary);
    }

    StagePropertyDelta(
        request.transactions,
        request.target,
        *request.descriptor,
        beforeSummary,
        afterSummary);
    MarkPropertyDirty(request, beforeSummary, afterSummary);

    return MakeResult(
        true,
        true,
        "Property edit applied.",
        beforeSummary,
        afterSummary);
}

EditorPropertyBatchEditResult EditorPropertyEditService::ApplyBatch(
    const EditorPropertyBatchEditRequest& request) const {
    if (!request.canMutateAuthoring) {
        return FailBatch(request, "Authoring is locked during Play/Sim.");
    }
    if (request.accessor == nullptr) {
        return FailBatch(request, "Property accessor is unavailable.");
    }
    if (request.edits.empty()) {
        return FailBatch(request, "No property edits were requested.");
    }

    struct PreparedEdit {
        EditorPropertyBatchEdit edit;
        EditorPropertyValue beforeValue;
        std::string beforeSummary;
    };
    std::vector<PreparedEdit> prepared;
    prepared.reserve(request.edits.size());

    for (const EditorPropertyBatchEdit& edit : request.edits) {
        if (edit.descriptor == nullptr) {
            return FailBatch(request, "Property descriptor is unavailable.");
        }
        if (edit.descriptor->readOnly) {
            return FailBatch(request, "Property is read-only.");
        }
        for (const PreparedEdit& existing : prepared) {
            if (existing.edit.target.SameObject(edit.target) &&
                existing.edit.descriptor != nullptr &&
                existing.edit.descriptor->name == edit.descriptor->name) {
                return FailBatch(request, "Batch property edit contains duplicate property targets.");
            }
        }

        EditorPropertyValue beforeValue{};
        if (!request.accessor->Get(edit.target, *edit.descriptor, beforeValue)) {
            return FailBatch(request, "Property is not accessible for a batch edit target.");
        }
        PreparedEdit preparedEdit{};
        preparedEdit.edit = edit;
        preparedEdit.beforeValue = beforeValue;
        preparedEdit.beforeSummary = FormatEditorPropertyValue(*edit.descriptor, beforeValue);
        prepared.push_back(std::move(preparedEdit));
    }

    std::vector<PreparedEdit> applied;
    applied.reserve(prepared.size());
    std::vector<EditorPropertyChange> changes;
    changes.reserve(prepared.size());

    const auto rollbackApplied =
        [&]() {
            for (auto it = applied.rbegin(); it != applied.rend(); ++it) {
                std::string rollbackError;
                request.accessor->Set(
                    it->edit.target,
                    *it->edit.descriptor,
                    it->beforeValue,
                    &rollbackError);
            }
        };

    for (const PreparedEdit& edit : prepared) {
        std::string setError;
        if (!request.accessor->Set(
                edit.edit.target,
                *edit.edit.descriptor,
                edit.edit.requestedValue,
                &setError)) {
            rollbackApplied();
            return FailBatch(
                request,
                setError.empty() ? std::string("Batch property edit was rejected.") : setError);
        }
        applied.push_back(edit);

        EditorPropertyValue afterValue{};
        if (!request.accessor->Get(edit.edit.target, *edit.edit.descriptor, afterValue)) {
            rollbackApplied();
            return FailBatch(request, "Batch property was changed but could not be read back.");
        }

        const std::string afterSummary =
            FormatEditorPropertyValue(*edit.edit.descriptor, afterValue);
        if (edit.beforeSummary == afterSummary) {
            continue;
        }

        EditorPropertyChange change{};
        change.target = edit.edit.target;
        change.propertyPath = edit.edit.descriptor->name;
        change.displayName = request.label.empty() ? edit.edit.descriptor->displayName : request.label;
        change.valueType = edit.edit.descriptor->valueType;
        change.beforeValue = edit.beforeSummary;
        change.afterValue = afterSummary;
        change.sourceRevision = edit.edit.target.generation;
        changes.push_back(std::move(change));
    }

    if (changes.empty()) {
        return MakeBatchResult(true, false, "Batch property values were unchanged.");
    }

    if (request.transactions != nullptr) {
        request.transactions->StagePropertyDeltas(changes);
    }
    MarkBatchDirty(request, changes);

    return MakeBatchResult(
        true,
        true,
        "Batch property edit applied.",
        std::move(changes));
}

EditorPropertyApplyDeltaResult EditorPropertyEditService::ApplyDelta(
    const EditorPropertyApplyDeltaRequest& request) const {
    if (!request.canMutateAuthoring) {
        return FailDelta(request, "Authoring is locked during Play/Sim.");
    }
    if (request.accessor == nullptr) {
        return FailDelta(request, "Property accessor is unavailable.");
    }
    if (request.propertyRegistry == nullptr) {
        return FailDelta(request, "Property registry is unavailable.");
    }
    if (request.transaction == nullptr) {
        return FailDelta(request, "Property transaction is unavailable.");
    }
    if (request.transaction->payload.kind != EditorTransactionPayloadKind::PropertyDelta &&
        request.transaction->payload.kind != EditorTransactionPayloadKind::MultiPropertyDelta) {
        return FailDelta(request, "Transaction is not a property delta.");
    }

    const EditorTransactionRecord& transaction = *request.transaction;

    std::vector<EditorPropertyChange> changes;
    if (transaction.payload.kind == EditorTransactionPayloadKind::PropertyDelta) {
        EditorPropertyChange change{};
        change.target = transaction.target;
        change.propertyPath = transaction.payload.propertyPath;
        change.valueType = transaction.payload.valueType;
        change.beforeValue = transaction.payload.beforeSummary;
        change.afterValue = transaction.payload.afterSummary;
        changes.push_back(std::move(change));
    } else {
        changes = transaction.payload.propertyChanges;
    }
    if (changes.empty()) {
        return FailDelta(request, "Property delta does not contain any changes.");
    }
    if (request.mode == EditorTransactionApplyMode::Undo) {
        std::reverse(changes.begin(), changes.end());
    }

    struct PreparedDelta {
        EditorPropertyChange change;
        const EditorPropertyDescriptor* descriptor = nullptr;
        EditorPropertyValue applyValue;
        EditorPropertyValue currentValue;
        bool hadCurrentValue = false;
    };
    std::vector<PreparedDelta> prepared;
    prepared.reserve(changes.size());

    for (const EditorPropertyChange& change : changes) {
        const EditorPropertyDescriptor* descriptor =
            request.propertyRegistry->Find(change.target.domain, change.propertyPath);
        if (descriptor == nullptr) {
            return FailDelta(request, "Property descriptor for transaction delta was not found.");
        }
        if (descriptor->readOnly) {
            return FailDelta(request, "Property delta targets a read-only property.");
        }

        const std::string& summary =
            request.mode == EditorTransactionApplyMode::Undo
                ? change.beforeValue
                : change.afterValue;
        EditorPropertyValue value{};
        std::string parseError;
        if (!ParseEditorPropertyValue(*descriptor, summary, value, &parseError)) {
            return FailDelta(
                request,
                parseError.empty() ? std::string("Failed to parse property delta value.") : parseError);
        }

        PreparedDelta delta{};
        delta.change = change;
        delta.descriptor = descriptor;
        delta.applyValue = value;
        delta.hadCurrentValue =
            request.accessor->Get(change.target, *descriptor, delta.currentValue);
        prepared.push_back(std::move(delta));
    }

    std::vector<PreparedDelta> applied;
    applied.reserve(prepared.size());
    bool changed = false;
    std::string appliedSummary;

    const auto rollbackApplied =
        [&]() {
            for (auto it = applied.rbegin(); it != applied.rend(); ++it) {
                if (!it->hadCurrentValue) {
                    continue;
                }
                std::string rollbackError;
                request.accessor->Set(
                    it->change.target,
                    *it->descriptor,
                    it->currentValue,
                    &rollbackError);
            }
        };

    for (const PreparedDelta& delta : prepared) {
        std::string setError;
        if (!request.accessor->Set(delta.change.target, *delta.descriptor, delta.applyValue, &setError)) {
            rollbackApplied();
            return FailDelta(
                request,
                setError.empty() ? std::string("Property delta apply was rejected.") : setError);
        }
        applied.push_back(delta);

        EditorPropertyValue afterValue{};
        if (!request.accessor->Get(delta.change.target, *delta.descriptor, afterValue)) {
            rollbackApplied();
            return FailDelta(request, "Property delta was applied but could not be read back.");
        }

        const std::string afterSummary = FormatEditorPropertyValue(*delta.descriptor, afterValue);
        changed = changed ||
            !delta.hadCurrentValue ||
            FormatEditorPropertyValue(*delta.descriptor, delta.currentValue) != afterSummary;
        if (!appliedSummary.empty()) {
            appliedSummary += "; ";
        }
        appliedSummary += delta.change.propertyPath + "=" + afterSummary;
        MarkDeltaDirty(request, *delta.descriptor, delta.change.target, afterSummary);
    }

    return MakeDeltaResult(
        true,
        changed,
        request.mode == EditorTransactionApplyMode::Undo
            ? std::string("Property delta undo applied.")
            : std::string("Property delta redo applied."),
        appliedSummary);
}

} // namespace editor
