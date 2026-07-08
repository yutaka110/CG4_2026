#include "EditorDetailsEditController.h"

#include <string>

namespace editor {
namespace {

EditorPropertyEditSessionResult MakeDetailsEditResult(
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

std::string BuildDetailsEditLabel(const EditorPropertyDescriptor& descriptor) {
    return descriptor.displayName.empty()
        ? std::string("Edit ") + descriptor.name
        : std::string("Edit ") + descriptor.displayName;
}

EditorPropertyAccessor* PreviewAccessor(const EditorDetailsEditControllerContext& context) {
    return context.previewAccessor != nullptr ? context.previewAccessor : context.accessor;
}

EditorPropertyEditSessionResult RequireDetailsSession(
    const EditorDetailsEditControllerContext& context) {
    if (context.session == nullptr) {
        return MakeDetailsEditResult(false, false, "Details edit session is unavailable.");
    }
    if (context.accessor == nullptr) {
        return MakeDetailsEditResult(false, false, "Property accessor is unavailable.");
    }
    return MakeDetailsEditResult(true, false, {});
}

} // namespace

EditorPropertyEditSessionResult BeginEditorDetailsPropertyEdit(
    const EditorDetailsEditControllerContext& context,
    const EditorObjectHandle& target,
    const EditorPropertyDescriptor& descriptor) {
    const EditorPropertyEditSessionResult ready = RequireDetailsSession(context);
    if (!ready.applied) {
        return ready;
    }

    if (context.session->IsActive()) {
        const EditorPropertyEditSessionResult cancelResult =
            CancelEditorDetailsPropertyEdit(context);
        if (!cancelResult.applied) {
            return cancelResult;
        }
    }

    return context.session->Begin(
        EditorPropertyEditSessionBeginRequest{
            context.accessor,
            {EditorPropertyEditSessionProperty{target, descriptor}},
            BuildDetailsEditLabel(descriptor),
            target,
            context.canMutateAuthoring,
            context.notifyOnFailure,
            context.source});
}

EditorPropertyEditSessionResult PreviewEditorDetailsPropertyEdit(
    const EditorDetailsEditControllerContext& context,
    const EditorObjectHandle& target,
    const EditorPropertyDescriptor& descriptor,
    const EditorPropertyValue& requestedValue) {
    const EditorPropertyEditSessionResult ready = RequireDetailsSession(context);
    if (!ready.applied) {
        return ready;
    }

    if (!context.session->IsActive()) {
        const EditorPropertyEditSessionResult beginResult =
            BeginEditorDetailsPropertyEdit(context, target, descriptor);
        if (!beginResult.applied) {
            return beginResult;
        }
    }

    return context.session->Preview(
        EditorPropertyEditSessionPreviewRequest{
            PreviewAccessor(context),
            {EditorPropertyEditSessionValue{target, descriptor.name, requestedValue}},
            context.canMutateAuthoring,
            context.notifyOnFailure,
            context.source});
}

EditorPropertyEditSessionResult CommitEditorDetailsPropertyEdit(
    const EditorDetailsEditControllerContext& context) {
    const EditorPropertyEditSessionResult ready = RequireDetailsSession(context);
    if (!ready.applied) {
        return ready;
    }
    if (!context.session->IsActive()) {
        return MakeDetailsEditResult(true, false, "Details edit session is not active.");
    }

    return context.session->Commit(
        EditorPropertyEditSessionCommitRequest{
            context.accessor,
            PreviewAccessor(context),
            context.transactions,
            context.dirtyState,
            context.notifications,
            context.canMutateAuthoring,
            context.notifyOnFailure,
            context.source});
}

EditorPropertyEditSessionResult CancelEditorDetailsPropertyEdit(
    const EditorDetailsEditControllerContext& context) {
    if (context.session == nullptr) {
        return MakeDetailsEditResult(false, false, "Details edit session is unavailable.");
    }
    if (!context.session->IsActive()) {
        return MakeDetailsEditResult(true, false, "Details edit session is not active.");
    }
    if (PreviewAccessor(context) == nullptr) {
        return MakeDetailsEditResult(false, false, "Property accessor is unavailable.");
    }
    return context.session->Cancel(
        EditorPropertyEditSessionCancelRequest{
            PreviewAccessor(context),
            context.notifyOnFailure,
            context.source});
}

EditorPropertyEditSessionResult ApplyEditorDetailsImmediatePropertyEdit(
    const EditorDetailsEditControllerContext& context,
    const EditorObjectHandle& target,
    const EditorPropertyDescriptor& descriptor,
    const EditorPropertyValue& requestedValue) {
    const EditorPropertyEditSessionResult beginResult =
        BeginEditorDetailsPropertyEdit(context, target, descriptor);
    if (!beginResult.applied) {
        return beginResult;
    }

    const EditorPropertyEditSessionResult previewResult =
        PreviewEditorDetailsPropertyEdit(context, target, descriptor, requestedValue);
    if (!previewResult.applied) {
        CancelEditorDetailsPropertyEdit(context);
        return previewResult;
    }

    return CommitEditorDetailsPropertyEdit(context);
}

} // namespace editor
