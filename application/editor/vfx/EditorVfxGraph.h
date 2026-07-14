#pragma once

#include "../graph/EditorGraph.h"
#include "../core/EditorExecutionService.h"
#include "../documents/EditorDocumentId.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

class EffectRuntime;

namespace editor {

class EditorDocumentManager;
class EditorAssetRegistry;
class EditorTransactionStack;
class EditorVfxGraphDocumentProvider;

inline constexpr uint32_t kEditorVfxGraphSchemaVersion = 2;
inline constexpr uint32_t kEditorVfxGraphMaxEmitters = 64;

enum class EditorVfxSimulationTarget {
    CPU,
    GPU,
};

enum class EditorVfxRendererKind {
    Sprite,
    Ribbon,
    Beam,
};

struct EditorVfxGraphAsset {
    uint32_t schemaVersion = kEditorVfxGraphSchemaVersion;
    std::string assetGuid;
    std::string name;
    EditorVfxSimulationTarget simulationTarget = EditorVfxSimulationTarget::GPU;
    uint32_t maxParticles = 65536;
    float fixedTimeStep = 1.0f / 60.0f;
    EditorGraph graph;
    uint64_t revision = 0;
};

struct EditorVfxCompileDiagnostic {
    EditorGraphIssueSeverity severity = EditorGraphIssueSeverity::Error;
    std::string code;
    std::string nodeId;
    std::string message;
};

struct EditorVfxEmitterProgram {
    std::string nodeId;
    std::string name;
    float spawnRate = 32.0f;
    uint32_t burstCount = 0;
    float lifetime = 1.0f;
    float initialVelocity[3] = {0.0f, 1.0f, 0.0f};
    float gravity[3] = {0.0f, -9.81f, 0.0f};
    float drag = 0.0f;
    float size = 0.1f;
    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    EditorVfxRendererKind renderer = EditorVfxRendererKind::Sprite;
    std::string materialAssetGuid;
    std::string textureAssetGuid;
};

struct EditorVfxCompileArtifact {
    bool succeeded = false;
    uint64_t sourceFingerprint = 0;
    std::string generatedProgram;
    std::string simulationHlsl;
    std::vector<EditorVfxEmitterProgram> emitters;
    std::vector<std::string> assetDependencies;
    std::vector<EditorVfxCompileDiagnostic> diagnostics;
};

EditorGraphSchema BuildEditorVfxGraphSchema();
EditorVfxGraphAsset MakeDefaultEditorVfxGraph(std::string assetGuid, std::string name);
EditorVfxCompileArtifact CompileEditorVfxGraph(
    const EditorVfxGraphAsset& asset,
    const EditorGraphSchema& schema);

class EditorVfxGraphService final : public IEditorExecutionService {
public:
    static constexpr std::string_view kServiceId = "editor.vfxGraph.execution";
    std::string_view ServiceId() const noexcept override { return kServiceId; }

    void Bind(
        EditorVfxGraphDocumentProvider* provider,
        EditorTransactionStack* transactions,
        EditorDocumentManager* documents,
        EffectRuntime* runtime = nullptr,
        const EditorAssetRegistry* assets = nullptr);
    void SetActiveDocument(EditorDocumentId document);
    const EditorDocumentId& ActiveDocument() const { return activeDocument_; }
    EditorVfxGraphAsset* ActiveAsset();
    const EditorVfxGraphAsset* ActiveAsset() const;
    const EditorVfxCompileArtifact& LastCompileArtifact() const { return lastCompileArtifact_; }
    const EditorVfxCompileArtifact& LastSuccessfulArtifact() const { return lastSuccessfulArtifact_; }
    const EditorGraphSchema& Schema() const { return schema_; }

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
    bool SetSimulationSettings(EditorVfxSimulationTarget target, uint32_t maxParticles,
        float fixedTimeStep, std::string& errorMessage);
    bool Recompile(std::string& errorMessage);
    bool ApplyPreview(std::string& errorMessage);

    bool PublishFromCommand(const EditorDocumentId& document,
        const EditorVfxGraphAsset& asset, std::string& errorMessage);
    void SetMutationCallback(
        std::function<void(const EditorDocumentId&, std::string_view)> callback) {
        mutationCallback_ = std::move(callback);
    }

private:
    bool CommitMutation(std::string_view label, EditorVfxGraphAsset before,
        EditorVfxGraphAsset after, std::string& errorMessage);
    bool ValidateConnection(const EditorVfxGraphAsset& asset,
        std::string_view fromNodeId, std::string_view fromPinId,
        std::string_view toNodeId, std::string_view toPinId,
        std::string& errorMessage) const;
    void UpdateCompileArtifact(const EditorVfxGraphAsset& asset);

    EditorGraphSchema schema_ = BuildEditorVfxGraphSchema();
    EditorVfxGraphDocumentProvider* provider_ = nullptr;
    EditorTransactionStack* transactions_ = nullptr;
    EditorDocumentManager* documents_ = nullptr;
    EffectRuntime* runtime_ = nullptr;
    const EditorAssetRegistry* assets_ = nullptr;
    EditorDocumentId activeDocument_{};
    EditorVfxCompileArtifact lastCompileArtifact_{};
    EditorVfxCompileArtifact lastSuccessfulArtifact_{};
    std::function<void(const EditorDocumentId&, std::string_view)> mutationCallback_;
};

const char* ToString(EditorVfxSimulationTarget value);
const char* ToString(EditorVfxRendererKind value);
bool EditorVfxSimulationTargetFromString(std::string_view value, EditorVfxSimulationTarget& output);

} // namespace editor
