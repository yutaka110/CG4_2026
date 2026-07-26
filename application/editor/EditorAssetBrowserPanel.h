#pragma once

#include "EditorAssetRegistry.h"
#include "EditorAssetSelection.h"
#include "EditorAssetThumbnailService.h"
#include "EditorContentBrowserState.h"

#include <filesystem>
#include <vector>
#include <Windows.h>

namespace editor {

class EditorNotificationCenter;
class EditorTransactionStack;

struct EditorAssetBrowserPanelContext {
    EditorAssetRegistry* registry = nullptr;
    EditorAssetSelection* assetSelection = nullptr;
    EditorAssetThumbnailService* thumbnails = nullptr;
    EditorTransactionStack* transactions = nullptr;
    EditorNotificationCenter* notifications = nullptr;
    EditorContentBrowserState* browserState = nullptr;
    const IEditorAssetWorkspaceStatusProvider* workspaceStatus = nullptr;
    HWND nativeDialogOwner = nullptr;
    std::vector<std::filesystem::path>* pendingExternalImportPaths = nullptr;
};

float ResolveEditorAssetViewHeight(float availableHeight);
void DrawEditorAssetBrowserPanel(const EditorAssetBrowserPanelContext& context);

} // namespace editor
