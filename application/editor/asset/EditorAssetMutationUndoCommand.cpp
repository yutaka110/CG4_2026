#include "EditorAssetMutationUndoCommand.h"

#include "IEditorAssetExecutionService.h"
#include "../core/EditorExecutionContext.h"
#include "../io/EditorTrashService.h"

#include <filesystem>
#include <utility>

namespace editor {
namespace {

std::size_t StringBytes(const std::string& value) noexcept {
    return value.capacity() + 1;
}

std::size_t RecordBytes(const EditorAssetRecord& record) noexcept {
    std::size_t bytes = sizeof(record) + StringBytes(record.id) +
        StringBytes(record.displayName) + StringBytes(record.logicalPath) +
        StringBytes(record.sourcePath) + StringBytes(record.metadataPath) +
        StringBytes(record.guid);
    for (const std::string& value : record.tags) bytes += StringBytes(value);
    for (const std::string& value : record.dependencies) bytes += StringBytes(value);
    for (const std::string& value : record.guidDependencies) bytes += StringBytes(value);
    for (const std::string& value : record.pathOnlyReferences) bytes += StringBytes(value);
    return bytes;
}

} // namespace

EditorAssetMutationUndoCommand::EditorAssetMutationUndoCommand(
    EditorAssetMutationChange change)
    : change_(std::move(change)) {}

EditorAssetMutationUndoCommand::~EditorAssetMutationUndoCommand() {
    if (!cleanupExternalPayload_ || !change_.diskBacked || change_.fileTransactionId.empty()) {
        return;
    }
    const std::filesystem::path projectRoot = change_.fileTransactionProjectRoot.empty()
        ? std::filesystem::current_path()
        : std::filesystem::path(change_.fileTransactionProjectRoot);
    EditorTrashService(EditorProjectPathPolicy(projectRoot))
        .Cleanup(change_.fileTransactionId, nullptr);
}

EditorUndoResult EditorAssetMutationUndoCommand::Apply(
    EditorTransactionApplyMode mode,
    EditorExecutionContext& context) const {
    IEditorExecutionService* untyped = context.Find(IEditorAssetExecutionService::kServiceId);
    auto* service = dynamic_cast<IEditorAssetExecutionService*>(untyped);
    if (service == nullptr) {
        return EditorUndoResult::Failure(
            EditorErrorCode::MissingService,
            "Asset execution service is not registered.");
    }
    return service->ApplyAssetMutation(change_, mode);
}

std::size_t EditorAssetMutationUndoCommand::EstimatedBytes() const noexcept {
    std::size_t bytes = sizeof(*this) + RecordBytes(change_.beforeRecord) +
        RecordBytes(change_.afterRecord) + change_.sourceBytes.capacity() +
        change_.metadataBytes.capacity() + StringBytes(change_.fileTransactionId) +
        StringBytes(change_.fileTransactionProjectRoot) + StringBytes(change_.sourceTrashPath) +
        StringBytes(change_.metadataTrashPath);
    for (const EditorAssetDependencyRewrite& rewrite : change_.dependencyRewrites) {
        bytes += sizeof(rewrite) + RecordBytes(rewrite.beforeRecord) + RecordBytes(rewrite.afterRecord);
    }
    return bytes;
}

} // namespace editor
