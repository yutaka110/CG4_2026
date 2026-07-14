#include "EditorGraph.h"

#include <algorithm>
#include <atomic>
#include <iomanip>
#include <set>
#include <sstream>
#include <unordered_set>

namespace editor {
namespace {

void AddIssue(
    EditorGraphValidationReport& report,
    EditorGraphIssueSeverity severity,
    std::string code,
    std::string nodeId,
    std::string linkId,
    std::string message) {
    report.issues.push_back({
        severity,
        std::move(code),
        std::move(nodeId),
        std::move(linkId),
        std::move(message)});
}

} // namespace

bool EditorGraphValidationReport::HasErrors() const noexcept {
    return std::any_of(issues.begin(), issues.end(), [](const EditorGraphIssue& issue) {
        return issue.severity == EditorGraphIssueSeverity::Error;
    });
}

bool EditorGraphSchema::RegisterNodeType(EditorGraphNodeTypeDefinition definition) {
    if (definition.typeId.empty() || definition.displayName.empty() ||
        FindNodeType(definition.typeId) != nullptr) {
        return false;
    }
    std::set<std::string> pinIds;
    for (const EditorGraphPinDefinition& pin : definition.pins) {
        if (pin.id.empty() || pin.typeId.empty() || !pinIds.insert(pin.id).second) return false;
    }
    nodeTypes_.push_back(std::move(definition));
    return true;
}

bool EditorGraphSchema::RegisterConversion(std::string fromTypeId, std::string toTypeId) {
    if (fromTypeId.empty() || toTypeId.empty() || fromTypeId == toTypeId) return false;
    std::vector<std::string>& targets = conversions_[std::move(fromTypeId)];
    if (std::find(targets.begin(), targets.end(), toTypeId) != targets.end()) return false;
    targets.push_back(std::move(toTypeId));
    return true;
}

const EditorGraphNodeTypeDefinition* EditorGraphSchema::FindNodeType(
    std::string_view typeId) const {
    const auto found = std::find_if(nodeTypes_.begin(), nodeTypes_.end(), [&](const auto& type) {
        return type.typeId == typeId;
    });
    return found == nodeTypes_.end() ? nullptr : &*found;
}

const EditorGraphPinDefinition* EditorGraphSchema::FindPin(
    std::string_view nodeTypeId,
    std::string_view pinId) const {
    const EditorGraphNodeTypeDefinition* type = FindNodeType(nodeTypeId);
    if (type == nullptr) return nullptr;
    const auto found = std::find_if(type->pins.begin(), type->pins.end(), [&](const auto& pin) {
        return pin.id == pinId;
    });
    return found == type->pins.end() ? nullptr : &*found;
}

bool EditorGraphSchema::CanConnect(
    std::string_view fromTypeId,
    std::string_view toTypeId) const {
    if (fromTypeId == toTypeId) return true;
    const auto found = conversions_.find(std::string(fromTypeId));
    return found != conversions_.end() &&
        std::find(found->second.begin(), found->second.end(), toTypeId) != found->second.end();
}

const EditorGraphNode* FindEditorGraphNode(const EditorGraph& graph, std::string_view nodeId) {
    const auto found = std::find_if(graph.nodes.begin(), graph.nodes.end(), [&](const auto& node) {
        return node.id == nodeId;
    });
    return found == graph.nodes.end() ? nullptr : &*found;
}

EditorGraphNode* FindEditorGraphNode(EditorGraph& graph, std::string_view nodeId) {
    return const_cast<EditorGraphNode*>(FindEditorGraphNode(
        static_cast<const EditorGraph&>(graph), nodeId));
}

const EditorGraphLink* FindEditorGraphLink(const EditorGraph& graph, std::string_view linkId) {
    const auto found = std::find_if(graph.links.begin(), graph.links.end(), [&](const auto& link) {
        return link.id == linkId;
    });
    return found == graph.links.end() ? nullptr : &*found;
}

const EditorGraphLink* FindEditorGraphIncomingLink(
    const EditorGraph& graph,
    std::string_view nodeId,
    std::string_view pinId) {
    const auto found = std::find_if(graph.links.begin(), graph.links.end(), [&](const auto& link) {
        return link.toNodeId == nodeId && link.toPinId == pinId;
    });
    return found == graph.links.end() ? nullptr : &*found;
}

bool EditorGraphWouldCreateCycle(
    const EditorGraph& graph,
    std::string_view fromNodeId,
    std::string_view toNodeId) {
    if (fromNodeId.empty() || toNodeId.empty() || fromNodeId == toNodeId) return true;
    std::unordered_set<std::string> visited;
    std::vector<std::string> pending{std::string(toNodeId)};
    while (!pending.empty()) {
        const std::string current = std::move(pending.back());
        pending.pop_back();
        if (current == fromNodeId) return true;
        if (!visited.insert(current).second) continue;
        for (const EditorGraphLink& link : graph.links) {
            if (link.fromNodeId == current) pending.push_back(link.toNodeId);
        }
    }
    return false;
}

EditorGraphValidationReport ValidateEditorGraph(
    const EditorGraph& graph,
    const EditorGraphSchema& schema) {
    EditorGraphValidationReport report;
    if (graph.nodes.size() > kEditorGraphMaxNodes) {
        AddIssue(report, EditorGraphIssueSeverity::Error, "graph.node_limit", {}, {},
            "Graph exceeds the 4096 node safety limit.");
    }
    if (graph.links.size() > kEditorGraphMaxLinks) {
        AddIssue(report, EditorGraphIssueSeverity::Error, "graph.link_limit", {}, {},
            "Graph exceeds the 16384 link safety limit.");
    }

    std::unordered_set<std::string> nodeIds;
    for (const EditorGraphNode& node : graph.nodes) {
        if (node.id.empty() || !nodeIds.insert(node.id).second) {
            AddIssue(report, EditorGraphIssueSeverity::Error, "graph.node_identity", node.id, {},
                "Graph node identity is empty or duplicated.");
        }
        if (schema.FindNodeType(node.typeId) == nullptr) {
            AddIssue(report, EditorGraphIssueSeverity::Error, "graph.node_type", node.id, {},
                "Graph node type is unavailable: " + node.typeId);
        }
    }

    std::unordered_set<std::string> linkIds;
    std::set<std::pair<std::string, std::string>> singleInputs;
    for (const EditorGraphLink& link : graph.links) {
        if (link.id.empty() || !linkIds.insert(link.id).second) {
            AddIssue(report, EditorGraphIssueSeverity::Error, "graph.link_identity", {}, link.id,
                "Graph link identity is empty or duplicated.");
            continue;
        }
        const EditorGraphNode* fromNode = FindEditorGraphNode(graph, link.fromNodeId);
        const EditorGraphNode* toNode = FindEditorGraphNode(graph, link.toNodeId);
        if (fromNode == nullptr || toNode == nullptr) {
            AddIssue(report, EditorGraphIssueSeverity::Error, "graph.link_endpoint", {}, link.id,
                "Graph link references a missing node.");
            continue;
        }
        const EditorGraphPinDefinition* fromPin = schema.FindPin(fromNode->typeId, link.fromPinId);
        const EditorGraphPinDefinition* toPin = schema.FindPin(toNode->typeId, link.toPinId);
        if (fromPin == nullptr || toPin == nullptr ||
            fromPin->direction != EditorGraphPinDirection::Output ||
            toPin->direction != EditorGraphPinDirection::Input) {
            AddIssue(report, EditorGraphIssueSeverity::Error, "graph.link_pin", {}, link.id,
                "Graph link references an unavailable or incorrectly directed pin.");
            continue;
        }
        if (!schema.CanConnect(fromPin->typeId, toPin->typeId)) {
            AddIssue(report, EditorGraphIssueSeverity::Error, "graph.link_type", toNode->id, link.id,
                "Graph link type mismatch: " + fromPin->typeId + " -> " + toPin->typeId + ".");
        }
        const auto input = std::make_pair(toNode->id, toPin->id);
        if (!toPin->allowMultipleLinks && !singleInputs.insert(input).second) {
            AddIssue(report, EditorGraphIssueSeverity::Error, "graph.input_cardinality", toNode->id, link.id,
                "Graph input pin has more than one incoming link.");
        }
    }

    std::unordered_map<std::string, std::size_t> indegree;
    std::unordered_map<std::string, std::vector<std::string>> adjacency;
    for (const EditorGraphNode& node : graph.nodes) indegree.emplace(node.id, 0);
    for (const EditorGraphLink& link : graph.links) {
        if (indegree.find(link.fromNodeId) == indegree.end() ||
            indegree.find(link.toNodeId) == indegree.end()) continue;
        adjacency[link.fromNodeId].push_back(link.toNodeId);
        ++indegree[link.toNodeId];
    }
    std::vector<std::string> ready;
    for (const auto& [nodeId, count] : indegree) {
        if (count == 0) ready.push_back(nodeId);
    }
    std::size_t visitedCount = 0;
    while (!ready.empty()) {
        const std::string nodeId = std::move(ready.back());
        ready.pop_back();
        ++visitedCount;
        for (const std::string& dependent : adjacency[nodeId]) {
            auto found = indegree.find(dependent);
            if (found != indegree.end() && --found->second == 0) ready.push_back(dependent);
        }
    }
    if (!schema.CyclesAllowed() && visitedCount != indegree.size()) {
        AddIssue(report, EditorGraphIssueSeverity::Error, "graph.cycle", {}, {},
            "Graph contains a dependency cycle.");
    }

    for (const EditorGraphNode& node : graph.nodes) {
        const EditorGraphNodeTypeDefinition* type = schema.FindNodeType(node.typeId);
        if (type == nullptr) continue;
        for (const EditorGraphPinDefinition& pin : type->pins) {
            if (pin.direction == EditorGraphPinDirection::Input && pin.required &&
                FindEditorGraphIncomingLink(graph, node.id, pin.id) == nullptr) {
                AddIssue(report, EditorGraphIssueSeverity::Error, "graph.required_input", node.id, {},
                    "Required input is not connected: " + pin.label + ".");
            }
        }
    }
    return report;
}

bool RemoveEditorGraphNode(EditorGraph& graph, std::string_view nodeId) {
    const auto found = std::find_if(graph.nodes.begin(), graph.nodes.end(), [&](const auto& node) {
        return node.id == nodeId;
    });
    if (found == graph.nodes.end()) return false;
    graph.nodes.erase(found);
    graph.links.erase(
        std::remove_if(graph.links.begin(), graph.links.end(), [&](const auto& link) {
            return link.fromNodeId == nodeId || link.toNodeId == nodeId;
        }),
        graph.links.end());
    ++graph.revision;
    return true;
}

std::string MakeEditorGraphElementId(std::string_view prefix) {
    static std::atomic<uint64_t> nextId{1};
    std::ostringstream stream;
    stream << prefix << '-' << std::hex << std::setfill('0') << std::setw(16)
           << nextId.fetch_add(1, std::memory_order_relaxed);
    return stream.str();
}

} // namespace editor
