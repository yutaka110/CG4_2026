#pragma once

#include "EditorAssetMutationSafety.h"
#include "EditorTransactionStack.h"

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

class EditorAssetMutationExecutor {
public:
    explicit EditorAssetMutationExecutor(EditorAssetRegistry& registry);

    EditorAssetMutationResult Execute(const EditorAssetMutationRequest& request);
    EditorAssetMutationResult ApplyTransaction(
        const EditorTransactionRecord& transaction,
        EditorTransactionApplyMode mode);

private:
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

    EditorAssetRegistry& registry_;
};

} // namespace editor
