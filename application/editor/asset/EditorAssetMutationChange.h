#pragma once

#include "../EditorAssetMutationSafety.h"

#include <cstdint>
#include <string>
#include <vector>

namespace editor {

struct EditorAssetDependencyRewrite {
    EditorAssetRecord beforeRecord;
    EditorAssetRecord afterRecord;
};

struct EditorAssetMutationChange {
    EditorAssetMutationKind kind = EditorAssetMutationKind::Rename;
    EditorAssetRecord beforeRecord;
    EditorAssetRecord afterRecord;
    std::vector<EditorAssetDependencyRewrite> dependencyRewrites;
    bool sourceSnapshotValid = false;
    bool metadataSnapshotValid = false;
    std::vector<uint8_t> sourceBytes;
    std::vector<uint8_t> metadataBytes;
    bool diskBacked = false;
    bool sourceFileExisted = false;
    bool metadataFileExisted = false;
    std::string fileTransactionId;
    std::string fileTransactionProjectRoot;
    std::string sourceTrashPath;
    std::string metadataTrashPath;
};

} // namespace editor
