#pragma once

#include "../graph/EditorGraph.h"
#include "../core/EditorExecutionService.h"
#include "../documents/EditorDocumentId.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

class EditorDocumentManager;
class EditorMaterialGraphDocumentProvider;
class EditorTransactionStack;

inline constexpr uint32_t kEditorMaterialGraphSchemaVersion = 2;

enum class EditorMaterialDomain {
    Surface,
    PostProcess,
};

enum class EditorMaterialBlendMode {
    Opaque,
    Masked,
    Translucent,
};

enum class EditorMaterialShadingModel {
    Lit,
    Unlit,
};

struct EditorMaterialGraphAsset {
    uint32_t schemaVersion = kEditorMaterialGraphSchemaVersion;
    std::string assetGuid;
    std::string name;
    EditorMaterialDomain domain = EditorMaterialDomain::Surface;
    EditorMaterialBlendMode blendMode = EditorMaterialBlendMode::Opaque;
    EditorMaterialShadingModel shadingModel = EditorMaterialShadingModel::Lit;
    EditorGraph graph;
    uint64_t revision = 0;
};

struct EditorMaterialCompileDiagnostic {
    EditorGraphIssueSeverity severity = EditorGraphIssueSeverity::Error;
    std::string code;
    std::string nodeId;
    std::string message;
};

struct EditorMaterialCompileArtifact {
    bool succeeded = false;
    uint64_t sourceFingerprint = 0;
    std::string hlslSource;
    std::vector<std::string> textureAssetGuids;
    std::vector<EditorMaterialCompileDiagnostic> diagnostics;
};

EditorGraphSchema BuildEditorMaterialGraphSchema();
EditorMaterialGraphAsset MakeDefaultEditorMaterialGraph(
    std::string assetGuid,
    std::string name);
EditorMaterialCompileArtifact CompileEditorMaterialGraph(
    const EditorMaterialGraphAsset& asset,
    const EditorGraphSchema& schema);

class EditorMaterialGraphService final : public IEditorExecutionService {
public:
    static constexpr std::string_view kServiceId = "editor.materialGraph.execution";
    std::string_view ServiceId() const noexcept override { return kServiceId; }

    void Bind(
        EditorMaterialGraphDocumentProvider* provider,
        EditorTransactionStack* transactions,
        EditorDocumentManager* documents);
    void SetActiveDocument(EditorDocumentId document);
    const EditorDocumentId& ActiveDocument() const { return activeDocument_; }
    EditorMaterialGraphAsset* ActiveAsset();
    const EditorMaterialGraphAsset* ActiveAsset() const;
    const EditorMaterialCompileArtifact& LastCompileArtifact() const { return lastCompileArtifact_; }
    const EditorMaterialCompileArtifact& LastSuccessfulArtifact() const { return lastSuccessfulArtifact_; }
    const EditorGraphSchema& Schema() const { return schema_; }

    bool AddNode(
        std::string_view nodeTypeId,
        float positionX,
        float positionY,
        std::string* createdNodeId,
        std::string& errorMessage);
    bool RemoveNode(std::string_view nodeId, std::string& errorMessage);
    bool Connect(
        std::string_view fromNodeId,
        std::string_view fromPinId,
        std::string_view toNodeId,
        std::string_view toPinId,
        std::string& errorMessage);
    bool Disconnect(std::string_view linkId, std::string& errorMessage);
    bool SetNodeProperty(
        std::string_view nodeId,
        std::string key,
        std::string value,
        std::string& errorMessage);
    bool MoveNode(
        std::string_view nodeId,
        float positionX,
        float positionY,
        std::string& errorMessage);
    bool Recompile(std::string& errorMessage);

    bool PublishFromCommand(
        const EditorDocumentId& document,
        const EditorMaterialGraphAsset& asset,
        std::string& errorMessage);

    void SetMutationCallback(
        std::function<void(const EditorDocumentId&, std::string_view)> callback) {
        mutationCallback_ = std::move(callback);
    }

private:
    bool CommitMutation(
        std::string_view label,
        EditorMaterialGraphAsset before,
        EditorMaterialGraphAsset after,
        std::string& errorMessage);
    bool ValidateConnection(
        const EditorMaterialGraphAsset& asset,
        std::string_view fromNodeId,
        std::string_view fromPinId,
        std::string_view toNodeId,
        std::string_view toPinId,
        std::string& errorMessage) const;
    void UpdateCompileArtifact(const EditorMaterialGraphAsset& asset);

    EditorGraphSchema schema_ = BuildEditorMaterialGraphSchema();
    EditorMaterialGraphDocumentProvider* provider_ = nullptr;
    EditorTransactionStack* transactions_ = nullptr;
    EditorDocumentManager* documents_ = nullptr;
    EditorDocumentId activeDocument_{};
    EditorMaterialCompileArtifact lastCompileArtifact_{};
    EditorMaterialCompileArtifact lastSuccessfulArtifact_{};
    std::function<void(const EditorDocumentId&, std::string_view)> mutationCallback_;
};

const char* ToString(EditorMaterialDomain value);
const char* ToString(EditorMaterialBlendMode value);
const char* ToString(EditorMaterialShadingModel value);
bool EditorMaterialDomainFromString(std::string_view value, EditorMaterialDomain& output);
bool EditorMaterialBlendModeFromString(std::string_view value, EditorMaterialBlendMode& output);
bool EditorMaterialShadingModelFromString(std::string_view value, EditorMaterialShadingModel& output);

} // namespace editor
