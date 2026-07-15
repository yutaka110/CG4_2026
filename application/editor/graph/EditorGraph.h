#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace editor {

inline constexpr std::size_t kEditorGraphMaxNodes = 4096;
inline constexpr std::size_t kEditorGraphMaxLinks = 16384;

enum class EditorGraphPinDirection {
    Input,
    Output,
};

struct EditorGraphPinDefinition {
    std::string id;
    std::string label;
    std::string typeId;
    EditorGraphPinDirection direction = EditorGraphPinDirection::Input;
    bool required = false;
    bool allowMultipleLinks = false;
};

struct EditorGraphNodeTypeDefinition {
    std::string typeId;
    std::string displayName;
    std::string category;
    std::vector<EditorGraphPinDefinition> pins;
};

struct EditorGraphNode {
    std::string id;
    std::string typeId;
    std::string label;
    float positionX = 0.0f;
    float positionY = 0.0f;
    std::map<std::string, std::string> properties;
};

struct EditorGraphLink {
    std::string id;
    std::string fromNodeId;
    std::string fromPinId;
    std::string toNodeId;
    std::string toPinId;
};

struct EditorGraph {
    std::vector<EditorGraphNode> nodes;
    std::vector<EditorGraphLink> links;
    uint64_t revision = 0;
};

enum class EditorGraphIssueSeverity {
    Warning,
    Error,
};

struct EditorGraphIssue {
    EditorGraphIssueSeverity severity = EditorGraphIssueSeverity::Error;
    std::string code;
    std::string nodeId;
    std::string linkId;
    std::string message;
};

struct EditorGraphValidationReport {
    std::vector<EditorGraphIssue> issues;
    bool HasErrors() const noexcept;
    bool Succeeded() const noexcept { return !HasErrors(); }
};

class EditorGraphSchema {
public:
    bool RegisterNodeType(EditorGraphNodeTypeDefinition definition);
    bool RegisterConversion(std::string fromTypeId, std::string toTypeId);
    const EditorGraphNodeTypeDefinition* FindNodeType(std::string_view typeId) const;
    const EditorGraphPinDefinition* FindPin(
        std::string_view nodeTypeId,
        std::string_view pinId) const;
    bool CanConnect(std::string_view fromTypeId, std::string_view toTypeId) const;
    void SetCyclesAllowed(bool allowed) noexcept { cyclesAllowed_ = allowed; }
    bool CyclesAllowed() const noexcept { return cyclesAllowed_; }
    const std::vector<EditorGraphNodeTypeDefinition>& NodeTypes() const { return nodeTypes_; }

private:
    std::vector<EditorGraphNodeTypeDefinition> nodeTypes_;
    std::unordered_map<std::string, std::vector<std::string>> conversions_;
    bool cyclesAllowed_ = false;
};

EditorGraphValidationReport ValidateEditorGraph(
    const EditorGraph& graph,
    const EditorGraphSchema& schema);
const EditorGraphNode* FindEditorGraphNode(const EditorGraph& graph, std::string_view nodeId);
EditorGraphNode* FindEditorGraphNode(EditorGraph& graph, std::string_view nodeId);
const EditorGraphLink* FindEditorGraphLink(const EditorGraph& graph, std::string_view linkId);
const EditorGraphLink* FindEditorGraphIncomingLink(
    const EditorGraph& graph,
    std::string_view nodeId,
    std::string_view pinId);
bool EditorGraphWouldCreateCycle(
    const EditorGraph& graph,
    std::string_view fromNodeId,
    std::string_view toNodeId);
bool RemoveEditorGraphNode(EditorGraph& graph, std::string_view nodeId);
std::string MakeEditorGraphElementId(std::string_view prefix);

} // namespace editor
