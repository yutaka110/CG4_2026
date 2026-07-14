#pragma once

#include "EditorAssetMutationSafety.h"
#include "EditorTransactionStack.h"
#include "asset/EditorAssetMutationChange.h"
#include "asset/IEditorAssetExecutionService.h"

#include <filesystem>
#include <string>
#include <vector>

namespace editor {

struct EditorAssetMutationRequest {
    EditorAssetMutationKind kind = EditorAssetMutationKind::Rename;
    EditorAssetKind targetKind = EditorAssetKind::Unknown;
    std::string targetId;
    std::string newId;
    std::string newSourcePath;
    EditorTransactionStack* transactions = nullptr;
};

struct EditorAssetMutationResult {
    bool succeeded = false;
    bool warning = false;
    std::string message;
    EditorAssetRecord updatedRecord;
    EditorAssetRecord deletedRecord;
    std::size_t rewrittenReferenceCount = 0;
    std::vector<std::string> rewrittenDependents;
    EditorAssetMutationChange transactionChange;
};

class EditorAssetMutationExecutor final : public IEditorAssetExecutionService {
public:
    explicit EditorAssetMutationExecutor(
        EditorAssetRegistry& registry,
        std::filesystem::path projectRoot = std::filesystem::current_path());

    EditorAssetMutationResult Execute(const EditorAssetMutationRequest& request);
    EditorUndoResult ApplyAssetMutation(
        const EditorAssetMutationChange& change,
        EditorTransactionApplyMode mode) override;

private:
    EditorAssetMutationResult DuplicateAsset(
        const EditorAssetRecord& target,
        const EditorAssetMutationRequest& request,
        const EditorAssetMutationSafetyReport& safety);
    EditorAssetMutationResult RenameAsset(
        const EditorAssetRecord& target,
        const EditorAssetMutationRequest& request,
        const EditorAssetMutationSafetyReport& safety);
    EditorAssetMutationResult MoveAsset(
        const EditorAssetRecord& target,
        const EditorAssetMutationRequest& request,
        const EditorAssetMutationSafetyReport& safety);
    EditorAssetMutationResult DeleteAsset(
        const EditorAssetRecord& target,
        const EditorAssetMutationSafetyReport& safety,
        const EditorAssetMutationRequest& request);
    EditorAssetMutationResult RepairReferences(
        const EditorAssetRecord& target,
        const EditorAssetMutationSafetyReport& safety,
        const EditorAssetMutationRequest& request);

    EditorAssetRegistry& registry_;
    std::filesystem::path projectRoot_;
};

} // namespace editor
