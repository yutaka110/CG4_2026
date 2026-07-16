#include "EditorProductionAiPipeline.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_set>

namespace editor {
namespace {

constexpr std::size_t kMaximumBehaviorAssetBytes = 16u * 1024u * 1024u;
constexpr float kEpsilon = 1.0e-5f;

void SetError(std::string* output, std::string value) {
    if (output != nullptr) *output = std::move(value);
}

uint64_t HashAppend(uint64_t value, std::string_view text) {
    for (unsigned char character : text) {
        value ^= character;
        value *= 1099511628211ull;
    }
    return value;
}

std::string ValueText(const EditorBlackboardValue& value);

uint64_t HashAsset(const EditorBehaviorTreeAsset& asset) {
    uint64_t hash = 1469598103934665603ull;
    hash = HashAppend(hash, asset.assetGuid);
    hash = HashAppend(hash, asset.name);
    for (const auto& key : asset.blackboard) {
        hash = HashAppend(hash, key.name);
        hash = HashAppend(hash, ToString(key.defaultValue.type));
        hash = HashAppend(hash, ValueText(key.defaultValue));
    }
    for (const auto& node : asset.nodes) {
        hash = HashAppend(hash, node.id);
        hash = HashAppend(hash, ToString(node.type));
        hash = HashAppend(hash, node.parentId);
        hash = HashAppend(hash, node.blackboardKey);
        hash = HashAppend(hash, node.operation);
        hash = HashAppend(hash, node.value);
        hash ^= node.order;
        const auto* durationBytes = reinterpret_cast<const unsigned char*>(&node.durationSeconds);
        for (std::size_t index = 0; index < sizeof(node.durationSeconds); ++index) {
            hash ^= durationBytes[index];
            hash *= 1099511628211ull;
        }
    }
    return hash;
}

const EditorSceneProperty* Property(
    const EditorSceneComponent* component, std::string_view name) {
    if (component == nullptr) return nullptr;
    const auto found = std::find_if(component->properties.begin(), component->properties.end(),
        [&](const EditorSceneProperty& value) { return value.name == name; });
    return found == component->properties.end() ? nullptr : &*found;
}

float FloatProperty(
    const EditorSceneComponent* component, std::string_view name, float fallback) {
    const EditorSceneProperty* property = Property(component, name);
    if (property == nullptr) return fallback;
    std::istringstream input(property->value);
    float value = fallback;
    return input >> value && std::isfinite(value) ? value : fallback;
}

bool BoolProperty(
    const EditorSceneComponent* component, std::string_view name, bool fallback) {
    const EditorSceneProperty* property = Property(component, name);
    if (property == nullptr) return fallback;
    if (property->value == "1" || property->value == "true" || property->value == "True") return true;
    if (property->value == "0" || property->value == "false" || property->value == "False") return false;
    return fallback;
}

int32_t IntProperty(
    const EditorSceneComponent* component, std::string_view name, int32_t fallback) {
    const EditorSceneProperty* property = Property(component, name);
    if (property == nullptr) return fallback;
    std::istringstream input(property->value);
    int32_t value = fallback;
    return input >> value ? value : fallback;
}

Vector3 VectorProperty(
    const EditorSceneComponent* component, std::string_view name, Vector3 fallback) {
    const EditorSceneProperty* property = Property(component, name);
    if (property == nullptr) return fallback;
    std::istringstream input(property->value);
    Vector3 value{};
    return input >> value.x >> value.y >> value.z && std::isfinite(value.x) &&
        std::isfinite(value.y) && std::isfinite(value.z) ? value : fallback;
}

std::string AssetReference(const EditorSceneComponent& component) {
    const auto found = std::find_if(component.references.begin(), component.references.end(),
        [](const EditorSceneObjectReference& value) {
            return value.property == "behaviorTree" || value.property == "asset";
        });
    return found == component.references.end() ? std::string{} : found->assetGuid;
}

bool ParseValue(
    EditorBlackboardValueType type, std::string_view text,
    EditorBlackboardValue& output) {
    output = {};
    output.type = type;
    std::istringstream input{std::string(text)};
    switch (type) {
    case EditorBlackboardValueType::Bool:
        if (text == "true" || text == "1") output.boolValue = true;
        else if (text == "false" || text == "0") output.boolValue = false;
        else return false;
        return true;
    case EditorBlackboardValueType::Int:
        return static_cast<bool>(input >> output.intValue);
    case EditorBlackboardValueType::Float:
        return static_cast<bool>(input >> output.floatValue) && std::isfinite(output.floatValue);
    case EditorBlackboardValueType::Vector3:
        return static_cast<bool>(input >> output.vectorValue.x >> output.vectorValue.y >>
            output.vectorValue.z) && std::isfinite(output.vectorValue.x) &&
            std::isfinite(output.vectorValue.y) && std::isfinite(output.vectorValue.z);
    case EditorBlackboardValueType::Entity:
    case EditorBlackboardValueType::String:
        output.textValue = text;
        return true;
    }
    return false;
}

std::string ValueText(const EditorBlackboardValue& value) {
    std::ostringstream output;
    output << std::setprecision(17);
    switch (value.type) {
    case EditorBlackboardValueType::Bool: return value.boolValue ? "true" : "false";
    case EditorBlackboardValueType::Int: output << value.intValue; break;
    case EditorBlackboardValueType::Float: output << value.floatValue; break;
    case EditorBlackboardValueType::Vector3:
        output << value.vectorValue.x << ' ' << value.vectorValue.y << ' '
               << value.vectorValue.z;
        break;
    case EditorBlackboardValueType::Entity:
    case EditorBlackboardValueType::String: return value.textValue;
    }
    return output.str();
}

bool ParseValueType(std::string_view text, EditorBlackboardValueType& output) {
    for (EditorBlackboardValueType value : {EditorBlackboardValueType::Bool,
             EditorBlackboardValueType::Int, EditorBlackboardValueType::Float,
             EditorBlackboardValueType::Vector3, EditorBlackboardValueType::Entity,
             EditorBlackboardValueType::String})
        if (text == ToString(value)) { output = value; return true; }
    return false;
}

bool ParseNodeType(std::string_view text, EditorBehaviorNodeType& output) {
    for (EditorBehaviorNodeType value : {EditorBehaviorNodeType::Root,
             EditorBehaviorNodeType::Selector, EditorBehaviorNodeType::Sequence,
             EditorBehaviorNodeType::Condition, EditorBehaviorNodeType::SetBlackboard,
             EditorBehaviorNodeType::Wait, EditorBehaviorNodeType::MoveTo,
             EditorBehaviorNodeType::Succeed, EditorBehaviorNodeType::Fail})
        if (text == ToString(value)) { output = value; return true; }
    return false;
}

float Length(Vector3 value) {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

Vector3 NormalizeOr(Vector3 value, Vector3 fallback = {0.0f, 0.0f, 1.0f}) {
    const float length = Length(value);
    if (length <= kEpsilon) return fallback;
    return {value.x / length, value.y / length, value.z / length};
}

float Dot(Vector3 a, Vector3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

uint64_t FileStamp(const std::filesystem::path& path) {
    std::error_code error;
    const auto value = std::filesystem::last_write_time(path, error);
    return error ? 0 : static_cast<uint64_t>(value.time_since_epoch().count());
}

bool CompareValue(
    const EditorBlackboardValue& current,
    std::string_view operation,
    std::string_view expectedText) {
    EditorBlackboardValue expected{};
    if (!ParseValue(current.type, expectedText, expected)) return false;
    bool equal = false;
    switch (current.type) {
    case EditorBlackboardValueType::Bool: equal = current.boolValue == expected.boolValue; break;
    case EditorBlackboardValueType::Int:
        if (operation == "greater") return current.intValue > expected.intValue;
        if (operation == "less") return current.intValue < expected.intValue;
        equal = current.intValue == expected.intValue;
        break;
    case EditorBlackboardValueType::Float:
        if (operation == "greater") return current.floatValue > expected.floatValue;
        if (operation == "less") return current.floatValue < expected.floatValue;
        equal = std::abs(current.floatValue - expected.floatValue) <= 1.0e-6;
        break;
    case EditorBlackboardValueType::Vector3:
        equal = Length({current.vectorValue.x - expected.vectorValue.x,
            current.vectorValue.y - expected.vectorValue.y,
            current.vectorValue.z - expected.vectorValue.z}) <= 1.0e-4f;
        break;
    case EditorBlackboardValueType::Entity:
    case EditorBlackboardValueType::String: equal = current.textValue == expected.textValue; break;
    }
    return operation == "not-equals" ? !equal : equal;
}

} // namespace

EditorBehaviorTreeAsset MakeDefaultEditorBehaviorTree(
    std::string assetGuid, std::string name) {
    EditorBehaviorTreeAsset asset{};
    asset.assetGuid = std::move(assetGuid);
    asset.name = std::move(name);
    asset.blackboard = {
        {"TargetEntity", {EditorBlackboardValueType::Entity}},
        {"TargetLocation", {EditorBlackboardValueType::Vector3}},
        {"HasLineOfSight", {EditorBlackboardValueType::Bool}},
        {"HeardStimulus", {EditorBlackboardValueType::Bool}},
    };
    asset.nodes = {
        {"root", EditorBehaviorNodeType::Root, {}, 0},
        {"selector", EditorBehaviorNodeType::Selector, "root", 0},
        {"chase", EditorBehaviorNodeType::Sequence, "selector", 0},
        {"has-target", EditorBehaviorNodeType::Condition, "chase", 0,
            "TargetEntity", "not-equals", ""},
        {"move", EditorBehaviorNodeType::MoveTo, "chase", 1,
            "TargetLocation"},
        {"idle", EditorBehaviorNodeType::Wait, "selector", 1,
            {}, {}, {}, 0.1f},
    };
    return asset;
}

EditorBehaviorTreeCompileResult CompileEditorBehaviorTree(
    const EditorBehaviorTreeAsset& asset) {
    EditorBehaviorTreeCompileResult result{};
    const auto issue = [&](std::string code, std::string node, std::string message) {
        result.diagnostics.push_back({std::move(code), std::move(node), std::move(message)});
    };
    if (asset.schemaVersion != kEditorBehaviorTreeSchemaVersion)
        issue("schema", {}, "Behavior Tree schema is unsupported.");
    if (asset.assetGuid.empty()) issue("asset-guid", {}, "Behavior Tree Asset GUID is empty.");
    if (asset.nodes.empty() || asset.nodes.size() > kEditorBehaviorTreeMaximumNodes)
        issue("node-budget", {}, "Behavior Tree node count is empty or over budget.");
    if (asset.blackboard.size() > kEditorBehaviorTreeMaximumBlackboardKeys)
        issue("blackboard-budget", {}, "Blackboard key count is over budget.");
    std::unordered_set<std::string> keys;
    std::unordered_map<std::string, const EditorBlackboardKeyDefinition*> keyDefinitions;
    for (const auto& key : asset.blackboard)
        if (key.name.empty() || !keys.insert(key.name).second) {
            issue("blackboard-key", {}, "Blackboard key is empty or duplicated.");
        } else {
            keyDefinitions.emplace(key.name, &key);
        }
    std::unordered_map<std::string, uint32_t> nodeIndices;
    uint32_t roots = 0;
    for (uint32_t index = 0; index < asset.nodes.size(); ++index) {
        const auto& node = asset.nodes[index];
        if (node.id.empty() || !nodeIndices.emplace(node.id, index).second)
            issue("node-id", node.id, "Behavior node ID is empty or duplicated.");
        if (node.type == EditorBehaviorNodeType::Root) {
            ++roots;
            if (!node.parentId.empty()) issue("root-parent", node.id, "Root cannot have a parent.");
            result.program.rootNodeId = node.id;
        }
    }
    if (roots != 1) issue("root-count", {}, "Behavior Tree requires exactly one Root.");
    result.program.nodes = asset.nodes;
    result.program.blackboard = asset.blackboard;
    for (uint32_t index = 0; index < asset.nodes.size(); ++index) {
        const auto& node = asset.nodes[index];
        if (node.type != EditorBehaviorNodeType::Root && !nodeIndices.contains(node.parentId))
            issue("parent", node.id, "Behavior node parent is missing.");
        if ((node.type == EditorBehaviorNodeType::Condition ||
             node.type == EditorBehaviorNodeType::SetBlackboard ||
             node.type == EditorBehaviorNodeType::MoveTo) &&
            !keys.contains(node.blackboardKey))
            issue("blackboard-reference", node.id, "Behavior node Blackboard key is missing.");
        if (node.type == EditorBehaviorNodeType::MoveTo) {
            const auto definition = keyDefinitions.find(node.blackboardKey);
            if (definition != keyDefinitions.end() &&
                definition->second->defaultValue.type != EditorBlackboardValueType::Vector3)
                issue("move-to-type", node.id, "MoveTo requires a Vector3 Blackboard key.");
        }
        if (node.type == EditorBehaviorNodeType::Condition) {
            const auto definition = keyDefinitions.find(node.blackboardKey);
            EditorBlackboardValue parsed{};
            if (node.operation != "equals" && node.operation != "not-equals" &&
                node.operation != "greater" && node.operation != "less")
                issue("condition-operation", node.id, "Condition operation is unsupported.");
            if (definition != keyDefinitions.end() &&
                !ParseValue(definition->second->defaultValue.type, node.value, parsed))
                issue("condition-value", node.id, "Condition comparison value is invalid.");
            if (definition != keyDefinitions.end() &&
                (node.operation == "greater" || node.operation == "less") &&
                definition->second->defaultValue.type != EditorBlackboardValueType::Int &&
                definition->second->defaultValue.type != EditorBlackboardValueType::Float)
                issue("condition-ordering", node.id,
                    "Ordered comparison requires an Int or Float Blackboard key.");
        }
        if (node.type == EditorBehaviorNodeType::SetBlackboard) {
            const auto definition = keyDefinitions.find(node.blackboardKey);
            EditorBlackboardValue parsed{};
            if (definition != keyDefinitions.end() &&
                !ParseValue(definition->second->defaultValue.type, node.value, parsed))
                issue("set-value", node.id, "SetBlackboard value is invalid.");
        }
        if (node.type == EditorBehaviorNodeType::Wait &&
            (!std::isfinite(node.durationSeconds) || node.durationSeconds < 0.0f))
            issue("wait-duration", node.id, "Wait duration must be finite and non-negative.");
        if (!node.parentId.empty()) result.program.children[node.parentId].push_back(index);
    }
    for (const auto& node : asset.nodes) {
        const auto children = result.program.children.find(node.id);
        const std::size_t childCount = children == result.program.children.end()
            ? 0u : children->second.size();
        if (node.type == EditorBehaviorNodeType::Root && childCount != 1)
            issue("root-child", node.id, "Root requires exactly one child.");
        if ((node.type == EditorBehaviorNodeType::Selector ||
             node.type == EditorBehaviorNodeType::Sequence) && childCount == 0)
            issue("composite-child", node.id, "Composite node requires at least one child.");
        if (node.type != EditorBehaviorNodeType::Root &&
            node.type != EditorBehaviorNodeType::Selector &&
            node.type != EditorBehaviorNodeType::Sequence && childCount != 0)
            issue("leaf-child", node.id, "Leaf node cannot have children.");
    }
    for (auto& [parent, children] : result.program.children) {
        (void)parent;
        std::sort(children.begin(), children.end(), [&](uint32_t a, uint32_t b) {
            if (asset.nodes[a].order != asset.nodes[b].order)
                return asset.nodes[a].order < asset.nodes[b].order;
            return asset.nodes[a].id < asset.nodes[b].id;
        });
    }
    std::unordered_set<std::string> visiting;
    std::unordered_set<std::string> visited;
    const auto walk = [&](const auto& self, std::string_view id) -> bool {
        if (visited.contains(std::string(id))) return true;
        if (!visiting.insert(std::string(id)).second) return false;
        const auto children = result.program.children.find(std::string(id));
        if (children != result.program.children.end())
            for (uint32_t child : children->second)
                if (!self(self, asset.nodes[child].id)) return false;
        visiting.erase(std::string(id));
        visited.insert(std::string(id));
        return true;
    };
    if (!result.program.rootNodeId.empty() && !walk(walk, result.program.rootNodeId))
        issue("cycle", result.program.rootNodeId, "Behavior Tree contains a cycle.");
    if (visited.size() != asset.nodes.size())
        issue("unreachable", {}, "Behavior Tree contains nodes unreachable from Root.");
    result.program.sourceFingerprint = HashAsset(asset);
    result.succeeded = result.diagnostics.empty();
    return result;
}

bool EncodeEditorBehaviorTree(
    const EditorBehaviorTreeAsset& asset,
    std::string& output,
    std::string* errorMessage) {
    const EditorBehaviorTreeCompileResult compiled = CompileEditorBehaviorTree(asset);
    if (!compiled.succeeded) {
        SetError(errorMessage, compiled.diagnostics.front().message);
        return false;
    }
    std::ostringstream stream;
    stream << "AI_BEHAVIOR_TREE " << kEditorBehaviorTreeSchemaVersion << '\n';
    stream << "asset " << std::quoted(asset.assetGuid) << ' ' << std::quoted(asset.name) << '\n';
    stream << "blackboard " << asset.blackboard.size() << '\n';
    for (const auto& key : asset.blackboard)
        stream << "key " << std::quoted(key.name) << ' '
               << ToString(key.defaultValue.type) << ' '
               << std::quoted(ValueText(key.defaultValue)) << '\n';
    stream << "nodes " << asset.nodes.size() << '\n';
    for (const auto& node : asset.nodes)
        stream << "node " << std::quoted(node.id) << ' ' << ToString(node.type) << ' '
               << std::quoted(node.parentId) << ' ' << node.order << ' '
               << std::quoted(node.blackboardKey) << ' ' << std::quoted(node.operation) << ' '
               << std::quoted(node.value) << ' ' << std::setprecision(9)
               << node.durationSeconds << '\n';
    output = stream.str();
    return true;
}

bool DecodeEditorBehaviorTree(
    std::string_view input,
    EditorBehaviorTreeAsset& asset,
    std::string* errorMessage) {
    if (input.size() > kMaximumBehaviorAssetBytes) {
        SetError(errorMessage, "Behavior Tree Asset exceeds 16 MiB.");
        return false;
    }
    std::istringstream stream{std::string(input)};
    std::string token;
    uint32_t schema = 0;
    if (!(stream >> token >> schema) || token != "AI_BEHAVIOR_TREE" ||
        schema != kEditorBehaviorTreeSchemaVersion || !(stream >> token) || token != "asset") {
        SetError(errorMessage, "Behavior Tree header is invalid.");
        return false;
    }
    EditorBehaviorTreeAsset decoded{};
    if (!(stream >> std::quoted(decoded.assetGuid) >> std::quoted(decoded.name)) ||
        !(stream >> token) || token != "blackboard") return false;
    size_t keyCount = 0;
    if (!(stream >> keyCount) || keyCount > kEditorBehaviorTreeMaximumBlackboardKeys) return false;
    for (size_t index = 0; index < keyCount; ++index) {
        std::string typeText;
        std::string valueText;
        EditorBlackboardKeyDefinition key{};
        if (!(stream >> token) || token != "key" ||
            !(stream >> std::quoted(key.name) >> typeText >> std::quoted(valueText)) ||
            !ParseValueType(typeText, key.defaultValue.type) ||
            !ParseValue(key.defaultValue.type, valueText, key.defaultValue)) return false;
        decoded.blackboard.push_back(std::move(key));
    }
    if (!(stream >> token) || token != "nodes") return false;
    size_t nodeCount = 0;
    if (!(stream >> nodeCount) || nodeCount == 0 ||
        nodeCount > kEditorBehaviorTreeMaximumNodes) return false;
    for (size_t index = 0; index < nodeCount; ++index) {
        std::string typeText;
        EditorBehaviorTreeNode node{};
        if (!(stream >> token) || token != "node" ||
            !(stream >> std::quoted(node.id) >> typeText >> std::quoted(node.parentId) >>
              node.order >> std::quoted(node.blackboardKey) >>
              std::quoted(node.operation) >> std::quoted(node.value) >>
              node.durationSeconds) || !ParseNodeType(typeText, node.type) ||
            !std::isfinite(node.durationSeconds)) return false;
        decoded.nodes.push_back(std::move(node));
    }
    const EditorBehaviorTreeCompileResult compiled = CompileEditorBehaviorTree(decoded);
    if (!compiled.succeeded) {
        SetError(errorMessage, compiled.diagnostics.front().message);
        return false;
    }
    asset = std::move(decoded);
    return true;
}

bool EditorProductionAiPipeline::Initialize(
    EditorProductionAiPolicy policy, std::string* errorMessage) {
    Shutdown();
    policy.maximumAgents = (std::max)(1u, policy.maximumAgents);
    policy.maximumStimuli = (std::max)(1u, policy.maximumStimuli);
    policy.maximumPerceivedPerAgent = (std::max)(1u, policy.maximumPerceivedPerAgent);
    policy.maximumNodeExecutionsPerTick = (std::max)(1u, policy.maximumNodeExecutionsPerTick);
    policy.minimumTickInterval = (std::max)(0.001f, policy.minimumTickInterval);
    policy.maximumTickInterval = (std::max)(policy.minimumTickInterval, policy.maximumTickInterval);
    if (!std::isfinite(policy.minimumTickInterval) ||
        !std::isfinite(policy.maximumTickInterval)) {
        SetError(errorMessage, "E-14 AI tick policy contains a non-finite value.");
        return false;
    }
    policy_ = policy;
    initialized_ = true;
    return true;
}

void EditorProductionAiPipeline::Shutdown() {
    programs_.clear();
    agents_.clear();
    debugSnapshots_.clear();
    diagnostics_.clear();
    stats_ = {};
    tickGeneration_ = 0;
    perceptionGeneration_ = 0;
    initialized_ = false;
}

bool EditorProductionAiPipeline::ResolveProgram(
    const EditorAssetRecord& record,
    const ResidentProgram*& output,
    std::string* errorMessage) {
    output = nullptr;
    const uint64_t fileStamp = FileStamp(record.sourcePath);
    const auto resident = programs_.find(record.guid);
    if (resident != programs_.end() &&
        resident->second.sourceTimestamp == record.sourceTimestamp &&
        resident->second.fileStamp == fileStamp) {
        output = &resident->second;
        return true;
    }
    std::ifstream file(record.sourcePath, std::ios::binary);
    if (!file) {
        SetError(errorMessage, "Behavior Tree source cannot be opened: " + record.sourcePath);
        return false;
    }
    std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    EditorBehaviorTreeAsset asset{};
    std::string decodeError;
    if (!DecodeEditorBehaviorTree(text, asset, &decodeError) || asset.assetGuid != record.guid) {
        SetError(errorMessage, decodeError.empty()
            ? "Behavior Tree durable GUID does not match its registry record." : decodeError);
        return false;
    }
    EditorBehaviorTreeCompileResult compiled = CompileEditorBehaviorTree(asset);
    if (!compiled.succeeded) {
        SetError(errorMessage, compiled.diagnostics.front().message);
        return false;
    }
    ResidentProgram loaded{};
    loaded.program = std::move(compiled.program);
    loaded.sourceTimestamp = record.sourceTimestamp;
    loaded.fileStamp = fileStamp;
    const bool hotReload = resident != programs_.end();
    programs_.insert_or_assign(record.guid, std::move(loaded));
    if (hotReload) ++stats_.hotReloads;
    output = &programs_.at(record.guid);
    return true;
}

bool EditorProductionAiPipeline::Sync(
    const EditorScene& scene,
    const EditorAssetRegistry& registry,
    const EditorProductionScenePipeline& productionScene,
    const EditorWorldPartitionPipeline& worldPartition,
    EditorProductionNavigationPipeline& navigation,
    float deltaTime,
    std::string* errorMessage) {
    if (!initialized_) {
        SetError(errorMessage, "E-14 AI pipeline is not initialized.");
        return false;
    }
    deltaTime = std::clamp(std::isfinite(deltaTime) ? deltaTime : 0.0f, 0.0f, 0.25f);
    const uint32_t hotReloads = stats_.hotReloads;
    stats_ = {};
    stats_.hotReloads = hotReloads;
    diagnostics_.clear();
    debugSnapshots_.clear();

    std::unordered_map<std::string, Matrix4x4> worlds;
    std::unordered_set<std::string> visiting;
    const auto resolveWorld = [&](const auto& self, const EditorSceneEntity& entity) -> Matrix4x4 {
        if (const auto found = worlds.find(entity.guid); found != worlds.end()) return found->second;
        if (!visiting.insert(entity.guid).second) return MakeIdentity4x4();
        const EditorSceneComponent* transform = scene.FindComponent(entity, kEditorTransformComponentType);
        Matrix4x4 world = MakeAffineMatrix(
            VectorProperty(transform, "scale", {1.0f, 1.0f, 1.0f}),
            VectorProperty(transform, "rotation", {}),
            VectorProperty(transform, "translation", {}));
        if (!entity.parentGuid.empty())
            if (const EditorSceneEntity* parent = scene.FindEntity(entity.parentGuid))
                world = Multiply(world, self(self, *parent));
        visiting.erase(entity.guid);
        worlds.insert_or_assign(entity.guid, world);
        return world;
    };

    struct StimulusInput {
        std::string entityGuid;
        Vector3 position{};
        int32_t team = 0;
        bool visible = true;
        bool audible = true;
        float loudness = 1.0f;
    };
    std::vector<StimulusInput> stimuli;
    for (const EditorSceneEntity& entity : scene.entities) {
        if (!worldPartition.SourceResidentEntities().contains(entity.guid)) continue;
        const EditorSceneComponent* component = scene.FindComponent(
            entity, kEditorAiStimulusComponentType);
        if (component == nullptr || !component->enabled ||
            !BoolProperty(component, "enabled", true)) continue;
        ++stats_.submittedStimuli;
        if (stimuli.size() >= policy_.maximumStimuli) {
            ++stats_.rejectedStimuli;
            continue;
        }
        const Matrix4x4 world = resolveWorld(resolveWorld, entity);
        stimuli.push_back({entity.guid, {world.m[3][0], world.m[3][1], world.m[3][2]},
            IntProperty(component, "team", 0), BoolProperty(component, "visible", true),
            BoolProperty(component, "audible", true),
            (std::max)(0.0f, FloatProperty(component, "loudness", 1.0f))});
    }
    std::sort(stimuli.begin(), stimuli.end(),
        [](const auto& a, const auto& b) { return a.entityGuid < b.entityGuid; });
    ++perceptionGeneration_;

    struct AgentInput {
        const EditorSceneEntity* entity = nullptr;
        const EditorSceneComponent* component = nullptr;
        Vector3 position{};
        Vector3 forward{};
        std::string assetGuid;
    };
    std::vector<AgentInput> agentInputs;
    for (const EditorSceneEntity& entity : scene.entities) {
        if (!worldPartition.SourceResidentEntities().contains(entity.guid)) continue;
        const EditorSceneComponent* component = scene.FindComponent(entity, kEditorAiAgentComponentType);
        if (component == nullptr || !component->enabled ||
            !BoolProperty(component, "enabled", true)) continue;
        ++stats_.submittedAgents;
        if (agentInputs.size() >= policy_.maximumAgents) {
            ++stats_.rejectedAgents;
            continue;
        }
        const Matrix4x4 world = resolveWorld(resolveWorld, entity);
        agentInputs.push_back({&entity, component,
            {world.m[3][0], world.m[3][1], world.m[3][2]},
            NormalizeOr({world.m[2][0], world.m[2][1], world.m[2][2]}),
            AssetReference(*component)});
    }
    std::sort(agentInputs.begin(), agentInputs.end(), [](const auto& a, const auto& b) {
        return a.entity->guid < b.entity->guid;
    });
    std::unordered_set<std::string> activeAgents;
    std::unordered_set<std::string> activePrograms;
    const auto navigationSnapshot = navigation.Snapshot();
    for (const AgentInput& input : agentInputs) {
        activeAgents.insert(input.entity->guid);
        AgentRuntime& runtime = agents_[input.entity->guid];
        runtime.behaviorAssetGuid = input.assetGuid;
        runtime.perceived.clear();
        runtime.trace.clear();
        runtime.lastPath.clear();
        runtime.executedNodes = 0;
        const EditorAssetRecord* record = registry.FindByGuid(input.assetGuid);
        const ResidentProgram* resident = nullptr;
        std::string programError;
        if (record == nullptr || record->kind != EditorAssetKind::BehaviorTree ||
            record->missing || !ResolveProgram(*record, resident, &programError) ||
            resident == nullptr) {
            runtime.status = EditorBehaviorStatus::InvalidProgram;
            diagnostics_.push_back("AI Agent '" + input.entity->guid +
                "' has no valid Behavior Tree: " +
                (programError.empty() ? "unresolved durable Asset." : programError));
            EditorAiAgentDebugSnapshot debug{};
            debug.entityGuid = input.entity->guid;
            debug.behaviorAssetGuid = input.assetGuid;
            debug.status = runtime.status;
            debug.tickGeneration = runtime.tickGeneration;
            debug.perceptionGeneration = perceptionGeneration_;
            debugSnapshots_.push_back(std::move(debug));
            continue;
        }
        activePrograms.insert(record->guid);
        const EditorBehaviorTreeProgram& program = resident->program;
        if (runtime.programFingerprint != program.sourceFingerprint) {
            runtime.blackboard.clear();
            for (const auto& key : program.blackboard)
                runtime.blackboard.emplace(key.name, key.defaultValue);
            runtime.waitElapsed.clear();
            runtime.programFingerprint = program.sourceFingerprint;
        }

        const int32_t team = IntProperty(input.component, "team", 0);
        const float sightRadius = (std::max)(0.0f,
            FloatProperty(input.component, "sightRadius", 30.0f));
        const float sightFov = std::clamp(
            FloatProperty(input.component, "sightFovDegrees", 90.0f), 1.0f, 360.0f);
        const float hearingRadius = (std::max)(0.0f,
            FloatProperty(input.component, "hearingRadius", 20.0f));
        const bool detectSameTeam = BoolProperty(input.component, "detectSameTeam", false);
        const float minimumSightDot = std::cos(
            sightFov * 0.5f * 3.14159265358979323846f / 180.0f);
        for (const StimulusInput& stimulus : stimuli) {
            if (stimulus.entityGuid == input.entity->guid ||
                (!detectSameTeam && stimulus.team == team)) continue;
            const Vector3 delta{stimulus.position.x - input.position.x,
                stimulus.position.y - input.position.y,
                stimulus.position.z - input.position.z};
            const float distance = Length(delta);
            EditorAiPerceivedStimulus perceived{};
            perceived.entityGuid = stimulus.entityGuid;
            perceived.position = stimulus.position;
            if (stimulus.visible && distance <= sightRadius) {
                ++stats_.sightTests;
                const Vector3 direction = NormalizeOr(delta);
                if (Dot(input.forward, direction) >= minimumSightDot) {
                    const EditorProductionSceneRayHit hit = productionScene.Raycast(
                        input.position, direction, distance);
                    const bool blocked = hit.valid && hit.entityGuid != stimulus.entityGuid &&
                        hit.distance < distance - 0.1f;
                    if (!blocked) {
                        perceived.seen = true;
                        perceived.strength = (std::max)(perceived.strength,
                            sightRadius > kEpsilon ? 1.0f - distance / sightRadius : 1.0f);
                        ++stats_.sightHits;
                    }
                }
            }
            if (stimulus.audible && distance <= hearingRadius * stimulus.loudness) {
                ++stats_.hearingTests;
                perceived.heard = true;
                perceived.strength = (std::max)(perceived.strength,
                    hearingRadius > kEpsilon ? stimulus.loudness *
                        (1.0f - distance / (hearingRadius * stimulus.loudness + kEpsilon)) : 1.0f);
                ++stats_.hearingHits;
            }
            if (perceived.seen || perceived.heard) runtime.perceived.push_back(std::move(perceived));
        }
        std::sort(runtime.perceived.begin(), runtime.perceived.end(), [](const auto& a, const auto& b) {
            if (a.strength != b.strength) return a.strength > b.strength;
            return a.entityGuid < b.entityGuid;
        });
        if (runtime.perceived.size() > policy_.maximumPerceivedPerAgent)
            runtime.perceived.resize(policy_.maximumPerceivedPerAgent);
        const EditorAiPerceivedStimulus* best = runtime.perceived.empty()
            ? nullptr : &runtime.perceived.front();
        const auto setBuiltin = [&](std::string_view name, EditorBlackboardValue value) {
            const auto found = runtime.blackboard.find(std::string(name));
            if (found != runtime.blackboard.end() && found->second.type == value.type)
                found->second = std::move(value);
        };
        EditorBlackboardValue targetEntity{};
        targetEntity.type = EditorBlackboardValueType::Entity;
        targetEntity.textValue = best != nullptr ? best->entityGuid : std::string{};
        setBuiltin("TargetEntity", targetEntity);
        EditorBlackboardValue targetLocation{};
        targetLocation.type = EditorBlackboardValueType::Vector3;
        targetLocation.vectorValue = best != nullptr ? best->position : input.position;
        setBuiltin("TargetLocation", targetLocation);
        EditorBlackboardValue hasSight{};
        hasSight.type = EditorBlackboardValueType::Bool;
        hasSight.boolValue = best != nullptr && best->seen;
        setBuiltin("HasLineOfSight", hasSight);
        EditorBlackboardValue heard{};
        heard.type = EditorBlackboardValueType::Bool;
        heard.boolValue = std::any_of(runtime.perceived.begin(), runtime.perceived.end(),
            [](const auto& value) { return value.heard; });
        setBuiltin("HeardStimulus", heard);

        const float tickInterval = std::clamp(
            FloatProperty(input.component, "tickInterval", 0.1f),
            policy_.minimumTickInterval, policy_.maximumTickInterval);
        runtime.accumulator += deltaTime;
        if (runtime.accumulator + kEpsilon >= tickInterval) {
            runtime.accumulator = std::fmod(runtime.accumulator, tickInterval);
            ++tickGeneration_;
            runtime.tickGeneration = tickGeneration_;
            ++stats_.behaviorTicks;
            std::unordered_map<std::string, uint32_t> nodeIndices;
            for (uint32_t index = 0; index < program.nodes.size(); ++index)
                nodeIndices.emplace(program.nodes[index].id, index);
            const auto execute = [&](const auto& self, uint32_t nodeIndex) -> EditorBehaviorStatus {
                if (++runtime.executedNodes > policy_.maximumNodeExecutionsPerTick)
                    return EditorBehaviorStatus::BudgetExceeded;
                const EditorBehaviorTreeNode& node = program.nodes[nodeIndex];
                runtime.trace.push_back(node.id);
                const auto children = program.children.find(node.id);
                const auto executeChildren = [&](bool selector) {
                    if (children == program.children.end() || children->second.empty())
                        return EditorBehaviorStatus::Failed;
                    for (uint32_t child : children->second) {
                        const EditorBehaviorStatus status = self(self, child);
                        if (status == EditorBehaviorStatus::BudgetExceeded ||
                            status == EditorBehaviorStatus::InvalidProgram ||
                            status == EditorBehaviorStatus::Running) return status;
                        if (selector && status == EditorBehaviorStatus::Succeeded)
                            return EditorBehaviorStatus::Succeeded;
                        if (!selector && status == EditorBehaviorStatus::Failed)
                            return EditorBehaviorStatus::Failed;
                    }
                    return selector ? EditorBehaviorStatus::Failed
                                    : EditorBehaviorStatus::Succeeded;
                };
                switch (node.type) {
                case EditorBehaviorNodeType::Root:
                case EditorBehaviorNodeType::Sequence: return executeChildren(false);
                case EditorBehaviorNodeType::Selector: return executeChildren(true);
                case EditorBehaviorNodeType::Condition: {
                    const auto value = runtime.blackboard.find(node.blackboardKey);
                    return value != runtime.blackboard.end() &&
                        CompareValue(value->second, node.operation, node.value)
                        ? EditorBehaviorStatus::Succeeded : EditorBehaviorStatus::Failed;
                }
                case EditorBehaviorNodeType::SetBlackboard: {
                    const auto value = runtime.blackboard.find(node.blackboardKey);
                    EditorBlackboardValue parsed{};
                    if (value == runtime.blackboard.end() ||
                        !ParseValue(value->second.type, node.value, parsed))
                        return EditorBehaviorStatus::Failed;
                    value->second = std::move(parsed);
                    return EditorBehaviorStatus::Succeeded;
                }
                case EditorBehaviorNodeType::Wait: {
                    float& elapsed = runtime.waitElapsed[node.id];
                    elapsed += tickInterval;
                    if (elapsed + kEpsilon < (std::max)(0.0f, node.durationSeconds))
                        return EditorBehaviorStatus::Running;
                    elapsed = 0.0f;
                    return EditorBehaviorStatus::Succeeded;
                }
                case EditorBehaviorNodeType::MoveTo: {
                    const auto value = runtime.blackboard.find(node.blackboardKey);
                    if (value == runtime.blackboard.end() ||
                        value->second.type != EditorBlackboardValueType::Vector3)
                        return EditorBehaviorStatus::Failed;
                    ++stats_.navigationQueries;
                    const EditorNavigationPathResult path = navigation.FindPath(
                        navigationSnapshot, input.position, value->second.vectorValue);
                    runtime.lastPath = path.points;
                    if (!path.Succeeded()) {
                        ++stats_.navigationFailures;
                        return EditorBehaviorStatus::Failed;
                    }
                    return EditorBehaviorStatus::Succeeded;
                }
                case EditorBehaviorNodeType::Succeed: return EditorBehaviorStatus::Succeeded;
                case EditorBehaviorNodeType::Fail: return EditorBehaviorStatus::Failed;
                }
                return EditorBehaviorStatus::InvalidProgram;
            };
            const auto root = nodeIndices.find(program.rootNodeId);
            runtime.status = root == nodeIndices.end()
                ? EditorBehaviorStatus::InvalidProgram : execute(execute, root->second);
            if (runtime.status == EditorBehaviorStatus::Succeeded) ++stats_.successfulTicks;
            else if (runtime.status == EditorBehaviorStatus::Running) ++stats_.runningTicks;
            else {
                ++stats_.failedTicks;
                if (runtime.status == EditorBehaviorStatus::BudgetExceeded) ++stats_.budgetFailures;
            }
        }

        EditorAiAgentDebugSnapshot debug{};
        debug.entityGuid = input.entity->guid;
        debug.behaviorAssetGuid = input.assetGuid;
        debug.status = runtime.status;
        debug.tickGeneration = runtime.tickGeneration;
        debug.perceptionGeneration = perceptionGeneration_;
        debug.activeNodeTrace = runtime.trace;
        debug.perceived = runtime.perceived;
        debug.lastPath = runtime.lastPath;
        debug.executedNodes = runtime.executedNodes;
        for (const auto& definition : program.blackboard) {
            const auto value = runtime.blackboard.find(definition.name);
            if (value != runtime.blackboard.end()) debug.blackboard.push_back(
                {definition.name, value->second});
        }
        debugSnapshots_.push_back(std::move(debug));
    }
    std::erase_if(agents_, [&](const auto& value) { return !activeAgents.contains(value.first); });
    std::erase_if(programs_, [&](const auto& value) { return !activePrograms.contains(value.first); });
    std::sort(debugSnapshots_.begin(), debugSnapshots_.end(),
        [](const auto& a, const auto& b) { return a.entityGuid < b.entityGuid; });
    stats_.activeAgents = static_cast<uint32_t>(debugSnapshots_.size());
    stats_.loadedPrograms = static_cast<uint32_t>(programs_.size());
    stats_.tickGeneration = tickGeneration_;
    stats_.perceptionGeneration = perceptionGeneration_;
    if (!diagnostics_.empty() && errorMessage != nullptr) *errorMessage = diagnostics_.front();
    return true;
}

const EditorAiAgentDebugSnapshot* EditorProductionAiPipeline::DebugSnapshot(
    std::string_view entityGuid) const {
    const auto found = std::find_if(debugSnapshots_.begin(), debugSnapshots_.end(),
        [&](const auto& value) { return value.entityGuid == entityGuid; });
    return found == debugSnapshots_.end() ? nullptr : &*found;
}

const char* ToString(EditorBlackboardValueType type) noexcept {
    switch (type) {
    case EditorBlackboardValueType::Bool: return "Bool";
    case EditorBlackboardValueType::Int: return "Int";
    case EditorBlackboardValueType::Float: return "Float";
    case EditorBlackboardValueType::Vector3: return "Vector3";
    case EditorBlackboardValueType::Entity: return "Entity";
    case EditorBlackboardValueType::String: return "String";
    }
    return "Bool";
}

const char* ToString(EditorBehaviorNodeType type) noexcept {
    switch (type) {
    case EditorBehaviorNodeType::Root: return "Root";
    case EditorBehaviorNodeType::Selector: return "Selector";
    case EditorBehaviorNodeType::Sequence: return "Sequence";
    case EditorBehaviorNodeType::Condition: return "Condition";
    case EditorBehaviorNodeType::SetBlackboard: return "SetBlackboard";
    case EditorBehaviorNodeType::Wait: return "Wait";
    case EditorBehaviorNodeType::MoveTo: return "MoveTo";
    case EditorBehaviorNodeType::Succeed: return "Succeed";
    case EditorBehaviorNodeType::Fail: return "Fail";
    }
    return "Fail";
}

const char* ToString(EditorBehaviorStatus status) noexcept {
    switch (status) {
    case EditorBehaviorStatus::Succeeded: return "Succeeded";
    case EditorBehaviorStatus::Failed: return "Failed";
    case EditorBehaviorStatus::Running: return "Running";
    case EditorBehaviorStatus::BudgetExceeded: return "BudgetExceeded";
    case EditorBehaviorStatus::InvalidProgram: return "InvalidProgram";
    }
    return "InvalidProgram";
}

} // namespace editor
