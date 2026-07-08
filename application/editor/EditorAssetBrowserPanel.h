#pragma once

#include "EditorAssetRegistry.h"
#include "EditorAssetSelection.h"
#include "EditorAssetThumbnailService.h"

namespace editor {

class EditorNotificationCenter;
class EditorTransactionStack;

struct EditorAssetBrowserPanelContext {
    EditorAssetRegistry* registry = nullptr;
    EditorAssetSelection* assetSelection = nullptr;
    EditorAssetThumbnailService* thumbnails = nullptr;
    EditorTransactionStack* transactions = nullptr;
    EditorNotificationCenter* notifications = nullptr;
};

void DrawEditorAssetBrowserPanel(const EditorAssetBrowserPanelContext& context);

} // namespace editor
