#pragma once

#include "../../AnimationStateMachine.h"
#include "../graph/EditorGraph.h"
#include "../core/EditorExecutionService.h"
#include "../documents/EditorDocumentId.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

class EditorAnimationStateMachineDocumentProvider;
class EditorDocumentManager;
class EditorTransactionStack;

inline constexpr uint32_t kEditorAnimationStateMachineSchemaVersion = 2;
inline constexpr uint32_t kEditorAnimationStateMachineMaxStates = 1024;
inline constexpr uint32_t kEditorAnimationStateMachineMaxTransitions = 8192;
inline constexpr uint32_t kEditorAnimationStateMachineMaxParameters = 256;

struct EditorAnimationStateMachineAsset {
    uint32_t schemaVersion = kEditorAnimationStateMachineSchemaVersion;
    std::string assetGuid;
    std::string name;
    std::vector<AnimationStateMachineParameter> parameters;
    EditorGraph graph;
    uint64_t revision = 0;
};

struct EditorAnimationStateMachineDiagnostic {
    EditorGraphIssueSeverity severity = EditorGraphIssueSeverity::Error;
    std::string code;
    std::string nodeId;
    std::string message;
};

struct EditorAnimationStateMachineArtifact {
    bool succeeded = false;
    uint64_t sourceFingerprint = 0;
    std::string generatedProgram;
    AnimationStateMachineProgram program;
    std::vector<std::string> animationSourceAssetGuids;
    std::vector<EditorAnimationStateMachineDiagnostic> diagnostics;
};

EditorGraphSchema BuildEditorAnimationStateMachineSchema();
EditorAnimationStateMachineAsset MakeDefaultEditorAnimationStateMachine(
    std::string assetGuid, std::string name);
EditorAnimationStateMachineArtifact CompileEditorAnimationStateMachine(
    const EditorAnimationStateMachineAsset& asset, const EditorGraphSchema& schema);

class EditorAnimationStateMachineService final : public IEditorExecutionService {
public:
    static constexpr std::string_view kServiceId = "editor.animationStateMachine.execution";
    std::string_view ServiceId() const noexcept override { return kServiceId; }

    void Bind(EditorAnimationStateMachineDocumentProvider* provider,
        EditorTransactionStack* transactions, EditorDocumentManager* documents);
    void SetActiveDocument(EditorDocumentId document);
    const EditorDocumentId& ActiveDocument() const { return activeDocument_; }
    EditorAnimationStateMachineAsset* ActiveAsset();
    const EditorAnimationStateMachineAsset* ActiveAsset() const;
    const EditorAnimationStateMachineArtifact& LastCompileArtifact() const { return lastCompileArtifact_; }
    const EditorAnimationStateMachineArtifact& LastSuccessfulArtifact() const { return lastSuccessfulArtifact_; }
    const EditorGraphSchema& Schema() const { return schema_; }
    const AnimationStateMachineSample& PreviewSample() const { return preview_.Sample(); }

    bool AddNode(std::string_view nodeTypeId, float positionX, float positionY,
        std::string* createdNodeId, std::string& errorMessage);
    bool RemoveNode(std::string_view nodeId, std::string& errorMessage);
    bool Connect(std::string_view fromNodeId, std::string_view fromPinId,
        std::string_view toNodeId, std::string_view toPinId, std::string& errorMessage);
    bool Disconnect(std::string_view linkId, std::string& errorMessage);
    bool SetNodeProperty(std::string_view nodeId, std::string key,
        std::string value, std::string& errorMessage);
    bool MoveNode(std::string_view nodeId, float positionX, float positionY,
        std::string& errorMessage);
    bool AddParameter(std::string name, AnimationParameterType type,
        float defaultValue, std::string& errorMessage);
    bool RemoveParameter(std::string_view name, std::string& errorMessage);
    bool Recompile(std::string& errorMessage);
    bool ResetPreview(std::string& errorMessage);
    bool StepPreview(float deltaTime, std::string& errorMessage);
    bool SetPreviewFloat(std::string_view name, float value);
    bool SetPreviewInt(std::string_view name, int32_t value);
    bool SetPreviewBool(std::string_view name, bool value);
    bool FirePreviewTrigger(std::string_view name);

    bool PublishFromCommand(const EditorDocumentId& document,
        const EditorAnimationStateMachineAsset& asset, std::string& errorMessage);
    void SetMutationCallback(
        std::function<void(const EditorDocumentId&, std::string_view)> callback) {
        mutationCallback_ = std::move(callback);
    }

private:
    bool CommitMutation(std::string_view label, EditorAnimationStateMachineAsset before,
        EditorAnimationStateMachineAsset after, std::string& errorMessage);
    void UpdateCompileArtifact(const EditorAnimationStateMachineAsset& asset);

    EditorGraphSchema schema_ = BuildEditorAnimationStateMachineSchema();
    EditorAnimationStateMachineDocumentProvider* provider_ = nullptr;
    EditorTransactionStack* transactions_ = nullptr;
    EditorDocumentManager* documents_ = nullptr;
    EditorDocumentId activeDocument_{};
    EditorAnimationStateMachineArtifact lastCompileArtifact_{};
    EditorAnimationStateMachineArtifact lastSuccessfulArtifact_{};
    AnimationStateMachineInstance preview_{};
    std::function<void(const EditorDocumentId&, std::string_view)> mutationCallback_;
};

} // namespace editor
