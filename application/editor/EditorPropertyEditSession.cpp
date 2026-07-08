#include "EditorPropertyEditSession.h"

#include <algorithm>
#include <utility>

namespace editor {
namespace {

EditorPropertyEditSessionResult MakeSessionResult(
    bool applied,
    bool changed,
    std::string message,
    std::size_t changedCount = 0) {
    EditorPropertyEditSessionResult result{};
    result.applied = applied;
    result.changed = changed;
    result.message = std::move(message);
    result.changedCount = changedCount;
    return result;
}

bool SamePropertyValue(
    const EditorPropertyDescriptor& descriptor,
    const EditorPropertyValue& lhs,
    const EditorPropertyValue& rhs) {
    return FormatEditorPropertyValue(descriptor, lhs) ==
        FormatEditorPropertyValue(descriptor, rhs);
}

const EditorPropertyValue& CurrentValue(const EditorPropertyEditSession::Entry& entry) {
    return entry.hasPreviewValue ? entry.previewValue : entry.beforeValue;
}

} // namespace

EditorPropertyEditSession::Entry* EditorPropertyEditSession::FindEntry(
    const EditorObjectHandle& target,
    const std::string& propertyPath) {
    auto it = std::find_if(
        entries_.begin(),
        entries_.end(),
        [&](const Entry& entry) {
            return entry.target.SameObject(target) && entry.descriptor.name == propertyPath;
        });
    return it != entries_.end() ? &*it : nullptr;
}

const EditorPropertyEditSession::Entry* EditorPropertyEditSession::FindEntry(
    const EditorObjectHandle& target,
    const std::string& propertyPath) const {
    auto it = std::find_if(
        entries_.begin(),
        entries_.end(),
        [&](const Entry& entry) {
            return entry.target.SameObject(target) && entry.descriptor.name == propertyPath;
        });
    return it != entries_.end() ? &*it : nullptr;
}

void EditorPropertyEditSession::Clear() {
    active_ = false;
    label_.clear();
    transactionTarget_ = {};
    entries_.clear();
}

bool EditorPropertyEditSession::Changed() const {
    if (!active_) {
        return false;
    }
    return std::any_of(
        entries_.begin(),
        entries_.end(),
        [](const Entry& entry) {
            return !SamePropertyValue(entry.descriptor, entry.beforeValue, CurrentValue(entry));
        });
}

EditorPropertyEditSessionResult EditorPropertyEditSession::Begin(
    const EditorPropertyEditSessionBeginRequest& request) {
    Clear();
    if (!request.canMutateAuthoring) {
        return MakeSessionResult(false, false, "Authoring is locked during Play/Sim.");
    }
    if (request.accessor == nullptr) {
        return MakeSessionResult(false, false, "Property accessor is unavailable.");
    }
    if (request.properties.empty()) {
        return MakeSessionResult(false, false, "No properties were registered for the edit session.");
    }

    std::vector<Entry> entries;
    entries.reserve(request.properties.size());
    for (const EditorPropertyEditSessionProperty& property : request.properties) {
        if (property.descriptor.name.empty()) {
            return MakeSessionResult(false, false, "Property descriptor is unavailable.");
        }
        if (property.descriptor.readOnly) {
            return MakeSessionResult(false, false, "Property is read-only.");
        }
        const bool duplicate = std::any_of(
            entries.begin(),
            entries.end(),
            [&](const Entry& entry) {
                return entry.target.SameObject(property.target) &&
                    entry.descriptor.name == property.descriptor.name;
            });
        if (duplicate) {
            return MakeSessionResult(false, false, "Edit session contains duplicate property targets.");
        }

        EditorPropertyValue beforeValue{};
        if (!request.accessor->Get(property.target, property.descriptor, beforeValue)) {
            return MakeSessionResult(false, false, "Property is not accessible for the edit session.");
        }

        Entry entry{};
        entry.target = property.target;
        entry.descriptor = property.descriptor;
        entry.beforeValue = beforeValue;
        entry.previewValue = beforeValue;
        entries.push_back(std::move(entry));
    }

    entries_ = std::move(entries);
    label_ = request.label;
    transactionTarget_ =
        request.transactionTarget.domain != EditorDomainId::Unknown
            ? request.transactionTarget
            : entries_.front().target;
    active_ = true;
    return MakeSessionResult(true, false, "Property edit session began.");
}

EditorPropertyEditSessionResult EditorPropertyEditSession::Preview(
    const EditorPropertyEditSessionPreviewRequest& request) {
    if (!active_) {
        return MakeSessionResult(false, false, "Property edit session is not active.");
    }
    if (!request.canMutateAuthoring) {
        return MakeSessionResult(false, false, "Authoring is locked during Play/Sim.");
    }
    if (request.accessor == nullptr) {
        return MakeSessionResult(false, false, "Property accessor is unavailable.");
    }
    if (request.values.empty()) {
        return MakeSessionResult(true, Changed(), "Property edit session preview was unchanged.");
    }

    struct AppliedPreview {
        Entry* entry = nullptr;
        EditorPropertyValue previousValue;
        bool hadPreviousValue = false;
    };
    std::vector<AppliedPreview> applied;
    applied.reserve(request.values.size());

    const auto rollbackApplied =
        [&]() {
            for (auto it = applied.rbegin(); it != applied.rend(); ++it) {
                if (it->entry == nullptr || !it->hadPreviousValue) {
                    continue;
                }
                std::string rollbackError;
                request.accessor->Set(
                    it->entry->target,
                    it->entry->descriptor,
                    it->previousValue,
                    &rollbackError);
                it->entry->previewValue = it->previousValue;
                it->entry->hasPreviewValue = true;
            }
        };

    for (const EditorPropertyEditSessionValue& value : request.values) {
        Entry* entry = FindEntry(value.target, value.propertyPath);
        if (entry == nullptr) {
            rollbackApplied();
            return MakeSessionResult(false, false, "Preview targets a property outside the edit session.");
        }

        AppliedPreview appliedPreview{};
        appliedPreview.entry = entry;
        appliedPreview.hadPreviousValue =
            request.accessor->Get(entry->target, entry->descriptor, appliedPreview.previousValue);

        std::string setError;
        if (!request.accessor->Set(entry->target, entry->descriptor, value.value, &setError)) {
            rollbackApplied();
            return MakeSessionResult(
                false,
                false,
                setError.empty() ? std::string("Property edit session preview was rejected.") : setError);
        }

        EditorPropertyValue readback{};
        if (!request.accessor->Get(entry->target, entry->descriptor, readback)) {
            rollbackApplied();
            return MakeSessionResult(false, false, "Preview was applied but could not be read back.");
        }

        entry->previewValue = readback;
        entry->hasPreviewValue = true;
        applied.push_back(std::move(appliedPreview));
    }

    return MakeSessionResult(true, Changed(), "Property edit session preview applied.", applied.size());
}

EditorPropertyEditSessionResult EditorPropertyEditSession::Commit(
    const EditorPropertyEditSessionCommitRequest& request) {
    if (!active_) {
        return MakeSessionResult(false, false, "Property edit session is not active.");
    }
    if (!request.canMutateAuthoring) {
        return MakeSessionResult(false, false, "Authoring is locked during Play/Sim.");
    }
    if (request.accessor == nullptr) {
        return MakeSessionResult(false, false, "Property accessor is unavailable.");
    }

    std::vector<EditorPropertyBatchEdit> edits;
    edits.reserve(entries_.size());
    for (const Entry& entry : entries_) {
        const EditorPropertyValue& finalValue = CurrentValue(entry);
        if (SamePropertyValue(entry.descriptor, entry.beforeValue, finalValue)) {
            continue;
        }
        edits.push_back(EditorPropertyBatchEdit{entry.target, &entry.descriptor, finalValue});
    }

    if (edits.empty()) {
        Clear();
        return MakeSessionResult(true, false, "Property edit session was unchanged.");
    }

    EditorPropertyAccessor* resetAccessor =
        request.previewAccessor != nullptr ? request.previewAccessor : request.accessor;
    for (const Entry& entry : entries_) {
        std::string resetError;
        if (!resetAccessor->Set(entry.target, entry.descriptor, entry.beforeValue, &resetError)) {
            return MakeSessionResult(
                false,
                false,
                resetError.empty()
                    ? std::string("Property edit session could not restore its begin state.")
                    : resetError);
        }
    }

    EditorPropertyEditService service;
    const EditorPropertyBatchEditResult result =
        service.ApplyBatch(
            EditorPropertyBatchEditRequest{
                request.accessor,
                request.transactions,
                request.dirtyState,
                request.notifications,
                std::move(edits),
                label_,
                transactionTarget_,
                request.canMutateAuthoring,
                request.notifyOnFailure,
                request.source});
    if (!result.applied) {
        for (const Entry& entry : entries_) {
            std::string restoreError;
            resetAccessor->Set(entry.target, entry.descriptor, CurrentValue(entry), &restoreError);
        }
        return MakeSessionResult(false, false, result.message);
    }

    const std::size_t changedCount = result.changedCount;
    Clear();
    return MakeSessionResult(true, result.changed, result.message, changedCount);
}

EditorPropertyEditSessionResult EditorPropertyEditSession::Cancel(
    const EditorPropertyEditSessionCancelRequest& request) {
    if (!active_) {
        return MakeSessionResult(true, false, "Property edit session is not active.");
    }
    if (request.accessor == nullptr) {
        return MakeSessionResult(false, false, "Property accessor is unavailable.");
    }

    bool restored = true;
    for (const Entry& entry : entries_) {
        std::string setError;
        restored = request.accessor->Set(entry.target, entry.descriptor, entry.beforeValue, &setError) && restored;
    }
    Clear();
    return MakeSessionResult(
        restored,
        false,
        restored
            ? std::string("Property edit session canceled.")
            : std::string("Property edit session cancel could not restore every property."));
}

} // namespace editor
