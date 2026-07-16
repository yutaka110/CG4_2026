#pragma once

#include "EditorProductionNavigationPipeline.h"
#include "../EditorViewportOverlay.h"
#include "../core/EditorExecutionService.h"
#include "../documents/EditorNavigationDocumentProvider.h"

#include <functional>

namespace editor {

class EditorDocumentManager;
class EditorTransactionStack;

struct EditorNavigationAuthoringPolicy {
    uint32_t maximumOverlayCommands = 4096;
};

struct EditorNavigationAuthoringStats {
    uint32_t mutations = 0;
    uint32_t compileFailures = 0;
    uint32_t runtimePublishes = 0;
    uint32_t overlayCommands = 0;
    uint32_t overlayBudgetRejected = 0;
};

// E-18 durable Navigation Data authoring owner. Every edit is compiled before
// publication and enters the generic command transaction stack as a full,
// bounded asset snapshot. The E-13 query pipeline only receives compiled data.
class EditorProductionNavigationAuthoringPipeline final :
    public IEditorExecutionService,
    public IEditorViewportOverlayProvider {
public:
    static constexpr std::string_view kServiceId = "editor.navigation.authoring.execution";

    std::string_view ServiceId() const noexcept override { return kServiceId; }
    std::string_view Id() const override { return kServiceId; }
    EditorViewportOverlayLayerId Layer() const override {
        return EditorViewportOverlayLayerId::CourseNavigation;
    }
    void Build(const EditorViewportOverlayFrameContext& context,
        EditorViewportOverlayCommandSink& sink) const override;

    bool Initialize(EditorNavigationAuthoringPolicy policy = {},
        std::string* errorMessage = nullptr);
    void Shutdown();
    void Bind(EditorNavigationDocumentProvider* provider,
        EditorTransactionStack* transactions, EditorDocumentManager* documents,
        EditorProductionNavigationPipeline* runtime);
    void SetActiveDocument(EditorDocumentId document);

    const EditorDocumentId& ActiveDocument() const noexcept { return activeDocument_; }
    EditorNavigationAuthoringAsset* ActiveAsset();
    const EditorNavigationAuthoringAsset* ActiveAsset() const;
    const EditorNavigationAuthoringCompileResult& CompileResult() const noexcept {
        return compileResult_;
    }

    bool AddArea(EditorNavigationAreaDefinition area, std::string& errorMessage);
    bool UpdateArea(EditorNavigationAreaDefinition area, std::string& errorMessage);
    bool RemoveArea(std::string_view id, std::string& errorMessage);
    bool AddAgentProfile(EditorNavigationAgentProfile profile, std::string& errorMessage);
    bool UpdateAgentProfile(EditorNavigationAgentProfile profile, std::string& errorMessage);
    bool RemoveAgentProfile(std::string_view id, std::string& errorMessage);
    bool AddOffMeshLink(EditorNavigationOffMeshLink link, std::string& errorMessage);
    bool UpdateOffMeshLink(EditorNavigationOffMeshLink link, std::string& errorMessage);
    bool RemoveOffMeshLink(std::string_view id, std::string& errorMessage);
    bool PublishFromCommand(const EditorDocumentId& document,
        const EditorNavigationAuthoringAsset& asset, std::string& errorMessage);

    const EditorNavigationAuthoringPolicy& Policy() const noexcept { return policy_; }
    const EditorNavigationAuthoringStats& Stats() const noexcept { return stats_; }
    void SetMutationCallback(
        std::function<void(const EditorDocumentId&, std::string_view)> callback) {
        mutationCallback_ = std::move(callback);
    }

private:
    bool Commit(std::string_view label, EditorNavigationAuthoringAsset before,
        EditorNavigationAuthoringAsset after, std::string& errorMessage);
    bool RefreshAndPublishRuntime(std::string* errorMessage = nullptr);

    EditorNavigationAuthoringPolicy policy_{};
    bool initialized_ = false;
    EditorNavigationDocumentProvider* provider_ = nullptr;
    EditorTransactionStack* transactions_ = nullptr;
    EditorDocumentManager* documents_ = nullptr;
    EditorProductionNavigationPipeline* runtime_ = nullptr;
    EditorDocumentId activeDocument_{};
    EditorNavigationAuthoringCompileResult compileResult_{};
    mutable EditorNavigationAuthoringStats stats_{};
    std::function<void(const EditorDocumentId&, std::string_view)> mutationCallback_;
};

} // namespace editor
