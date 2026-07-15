#pragma once

#include "../../GameplayVisualScript.h"
#include "../graph/EditorGraph.h"
#include "../core/EditorExecutionService.h"
#include "../documents/EditorDocumentId.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

class EditorDocumentManager;
class EditorGameplayVisualScriptDocumentProvider;
class EditorTransactionStack;

inline constexpr uint32_t kEditorGameplayVisualScriptSchemaVersion = 2;
inline constexpr uint32_t kEditorGameplayVisualScriptMaxVariables = 512;
inline constexpr uint32_t kEditorGameplayVisualScriptMaxInstructions = 16384;

struct EditorGameplayVisualScriptAsset {
    uint32_t schemaVersion = kEditorGameplayVisualScriptSchemaVersion;
    std::string assetGuid;
    std::string name;
    std::vector<GameplayVariableDefinition> variables;
    uint32_t instructionBudget = 4096;
    EditorGraph graph;
    uint64_t revision = 0;
};

struct EditorGameplayVisualScriptDiagnostic {
    EditorGraphIssueSeverity severity = EditorGraphIssueSeverity::Error;
    std::string code;
    std::string nodeId;
    std::string message;
};

struct EditorGameplayVisualScriptArtifact {
    bool succeeded = false;
    uint64_t sourceFingerprint = 0;
    std::string generatedProgram;
    GameplayVisualScriptProgram program;
    std::vector<EditorGameplayVisualScriptDiagnostic> diagnostics;
};

EditorGraphSchema BuildEditorGameplayVisualScriptSchema();
EditorGameplayVisualScriptAsset MakeDefaultEditorGameplayVisualScript(
    std::string assetGuid, std::string name);
EditorGameplayVisualScriptArtifact CompileEditorGameplayVisualScript(
    const EditorGameplayVisualScriptAsset& asset, const EditorGraphSchema& schema);

class EditorGameplayVisualScriptService final : public IEditorExecutionService {
public:
    static constexpr std::string_view kServiceId = "editor.gameplayVisualScript.execution";
    std::string_view ServiceId() const noexcept override { return kServiceId; }

    void Bind(EditorGameplayVisualScriptDocumentProvider* provider,
        EditorTransactionStack* transactions, EditorDocumentManager* documents);
    void SetActiveDocument(EditorDocumentId document);
    const EditorDocumentId& ActiveDocument() const { return activeDocument_; }
    EditorGameplayVisualScriptAsset* ActiveAsset();
    const EditorGameplayVisualScriptAsset* ActiveAsset() const;
    const EditorGameplayVisualScriptArtifact& LastCompileArtifact() const { return lastCompileArtifact_; }
    const EditorGameplayVisualScriptArtifact& LastSuccessfulArtifact() const { return lastSuccessfulArtifact_; }
    const EditorGraphSchema& Schema() const { return schema_; }
    const GameplayExecutionResult& LastPreviewResult() const { return lastPreviewResult_; }
    const std::vector<std::string>& PreviewTrace() const { return preview_.Trace(); }
    const std::vector<std::string>& PreviewOutput() const { return previewOutput_; }

    bool AddNode(std::string_view nodeTypeId, float x, float y,
        std::string* createdNodeId, std::string& errorMessage);
    bool RemoveNode(std::string_view nodeId, std::string& errorMessage);
    bool Connect(std::string_view fromNodeId, std::string_view fromPinId,
        std::string_view toNodeId, std::string_view toPinId, std::string& errorMessage);
    bool Disconnect(std::string_view linkId, std::string& errorMessage);
    bool SetNodeProperty(std::string_view nodeId, std::string key,
        std::string value, std::string& errorMessage);
    bool MoveNode(std::string_view nodeId, float x, float y, std::string& errorMessage);
    bool AddVariable(std::string name, GameplayValue value, std::string& errorMessage);
    bool RemoveVariable(std::string_view name, std::string& errorMessage);
    bool SetInstructionBudget(uint32_t budget, std::string& errorMessage);
    bool Recompile(std::string& errorMessage);
    bool ResetPreview(std::string& errorMessage);
    bool ExecutePreview(std::string_view eventName, float deltaTime, std::string& errorMessage);
    bool SetPreviewVariable(std::string_view name, const GameplayValue& value);

    bool PublishFromCommand(const EditorDocumentId& document,
        const EditorGameplayVisualScriptAsset& asset, std::string& errorMessage);
    void SetMutationCallback(
        std::function<void(const EditorDocumentId&, std::string_view)> callback) {
        mutationCallback_ = std::move(callback);
    }

private:
    bool CommitMutation(std::string_view label, EditorGameplayVisualScriptAsset before,
        EditorGameplayVisualScriptAsset after, std::string& errorMessage);
    void UpdateCompileArtifact(const EditorGameplayVisualScriptAsset& asset);

    EditorGraphSchema schema_ = BuildEditorGameplayVisualScriptSchema();
    EditorGameplayVisualScriptDocumentProvider* provider_ = nullptr;
    EditorTransactionStack* transactions_ = nullptr;
    EditorDocumentManager* documents_ = nullptr;
    EditorDocumentId activeDocument_{};
    EditorGameplayVisualScriptArtifact lastCompileArtifact_{};
    EditorGameplayVisualScriptArtifact lastSuccessfulArtifact_{};
    GameplayVisualScriptInstance preview_{};
    GameplayExecutionResult lastPreviewResult_{};
    std::vector<std::string> previewOutput_;
    std::function<void(const EditorDocumentId&, std::string_view)> mutationCallback_;
};

} // namespace editor
