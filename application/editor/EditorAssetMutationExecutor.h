#pragma once

#include "EditorAssetMutationSafety.h"

#include <string>

namespace editor {

struct EditorAssetMutationRequest {
    EditorAssetMutationKind kind = EditorAssetMutationKind::Rename;
    EditorAssetKind targetKind = EditorAssetKind::Unknown;
    std::string targetId;
    std::string newId;
    std::string newSourcePath;
};

struct EditorAssetMutationResult {
    bool succeeded = false;
    bool warning = false;
    std::string message;
    EditorAssetRecord updatedRecord;
};

class EditorAssetMutationExecutor {
public:
    explicit EditorAssetMutationExecutor(EditorAssetRegistry& registry);

    EditorAssetMutationResult Execute(const EditorAssetMutationRequest& request);

private:
    EditorAssetMutationResult RenameAsset(
        const EditorAssetRecord& target,
        const EditorAssetMutationRequest& request);
    EditorAssetMutationResult MoveAsset(
        const EditorAssetRecord& target,
        const EditorAssetMutationRequest& request);

    EditorAssetRegistry& registry_;
};

} // namespace editor
