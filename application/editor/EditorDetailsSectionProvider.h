#pragma once

#include <memory>
#include <functional>
#include <vector>

#include "EditorAssetSelection.h"
#include "EditorDetailsEditController.h"
#include "EditorPropertyClipboardService.h"
#include "EditorPropertyRegistry.h"
#include "EditorSelection.h"
#include "EditorValidation.h"

namespace editor {

class EditorWorldMutationService;
class SceneWorldObjectProvider;
struct EditorWorldMutationResult;

struct EditorDetailsSectionContext {
    const std::vector<EditorObjectHandle>* selectedObjects = nullptr;
    const EditorPropertyRegistry* propertyRegistry = nullptr;
    EditorPropertyAccessor* propertyAccessor = nullptr;
    EditorPropertyEditSession* propertyEditSession = nullptr;
    EditorTransactionStack* transactions = nullptr;
    EditorDirtyStateService* dirtyState = nullptr;
    EditorNotificationCenter* notifications = nullptr;
    EditorPropertyClipboardService* propertyClipboard = nullptr;
    const EditorAssetRegistry* assetRegistry = nullptr;
    const EditorAssetSelection* assetSelection = nullptr;
    const EditorValidationReport* validationReport = nullptr;
    bool canMutateAuthoring = true;
    const char* source = "editor.details.section";
    EditorWorldMutationService* worldMutations = nullptr;
    SceneWorldObjectProvider* sceneWorldProvider = nullptr;
    std::function<void(const EditorWorldMutationResult&)> onWorldMutated;
};

class EditorDetailsSectionProvider {
public:
    virtual ~EditorDetailsSectionProvider() = default;

    virtual EditorDomainId Domain() const = 0;
    virtual const char* SectionId() const = 0;
    virtual const char* DisplayName() const = 0;
    virtual void Draw(const EditorDetailsSectionContext& context) = 0;
};

class EditorDetailsSectionProviderRegistry {
public:
    void Clear();
    void Add(std::unique_ptr<EditorDetailsSectionProvider> provider);
    std::vector<EditorDetailsSectionProvider*> FindByDomain(EditorDomainId domain) const;
    std::size_t Count() const { return providers_.size(); }

private:
    std::vector<std::unique_ptr<EditorDetailsSectionProvider>> providers_;
};

} // namespace editor
