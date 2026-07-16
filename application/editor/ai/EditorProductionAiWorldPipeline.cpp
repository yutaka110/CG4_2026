#include "EditorProductionAiWorldPipeline.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_set>

namespace editor {
namespace {

constexpr std::size_t kMaximumEqsAssetBytes = 4u * 1024u * 1024u;
constexpr float kEpsilon = 1.0e-5f;

void SetError(std::string* output, std::string value) {
    if (output != nullptr) *output = std::move(value);
}

float Length(Vector3 value) {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

Vector3 Add(Vector3 a, Vector3 b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 Subtract(Vector3 a, Vector3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vector3 Scale(Vector3 value, float scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

Vector3 NormalizeOrZero(Vector3 value) {
    const float length = Length(value);
    return length <= kEpsilon ? Vector3{} : Scale(value, 1.0f / length);
}

Vector3 ClampLength(Vector3 value, float maximum) {
    const float length = Length(value);
    return length <= maximum || length <= kEpsilon ? value : Scale(value, maximum / length);
}

float Dot(Vector3 a, Vector3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
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

int32_t IntProperty(
    const EditorSceneComponent* component, std::string_view name, int32_t fallback) {
    const EditorSceneProperty* property = Property(component, name);
    if (property == nullptr) return fallback;
    std::istringstream input(property->value);
    int32_t value = fallback;
    return input >> value ? value : fallback;
}

bool BoolProperty(
    const EditorSceneComponent* component, std::string_view name, bool fallback) {
    const EditorSceneProperty* property = Property(component, name);
    if (property == nullptr) return fallback;
    if (property->value == "true" || property->value == "1") return true;
    if (property->value == "false" || property->value == "0") return false;
    return fallback;
}

std::string StringProperty(
    const EditorSceneComponent* component, std::string_view name, std::string fallback) {
    const EditorSceneProperty* property = Property(component, name);
    return property == nullptr ? std::move(fallback) : property->value;
}

uint64_t HashAppend(uint64_t hash, std::string_view value) {
    for (unsigned char character : value) {
        hash ^= character;
        hash *= 1099511628211ull;
    }
    return hash;
}

uint64_t HashEqs(const EditorEqsAsset& asset) {
    std::ostringstream canonical;
    canonical << asset.assetGuid << '\n' << asset.name << '\n'
              << ToString(asset.generator) << '\n' << std::setprecision(9)
              << asset.radius << ' ' << asset.spacing << ' ' << asset.candidateCount
              << '\n' << asset.smartObjectType << '\n';
    for (const auto& test : asset.tests)
        canonical << test.id << ' ' << ToString(test.type) << ' ' << test.weight << ' '
                  << test.minimum << ' ' << test.maximum << ' ' << test.filter << ' '
                  << test.preferHigher << '\n';
    return HashAppend(1469598103934665603ull, canonical.str());
}

uint64_t FileStamp(const std::filesystem::path& path) {
    std::error_code error;
    const auto value = std::filesystem::last_write_time(path, error);
    return error ? 0 : static_cast<uint64_t>(value.time_since_epoch().count());
}

bool ParseGenerator(std::string_view text, EditorEqsGeneratorType& output) {
    for (EditorEqsGeneratorType value : {EditorEqsGeneratorType::Ring,
             EditorEqsGeneratorType::Grid, EditorEqsGeneratorType::SmartObjects})
        if (text == ToString(value)) { output = value; return true; }
    return false;
}

bool ParseTest(std::string_view text, EditorEqsTestType& output) {
    for (EditorEqsTestType value : {EditorEqsTestType::Distance,
             EditorEqsTestType::PathCost, EditorEqsTestType::Visibility,
             EditorEqsTestType::Crowding, EditorEqsTestType::SmartObjectAvailable})
        if (text == ToString(value)) { output = value; return true; }
    return false;
}

const EditorBlackboardValue* BlackboardValue(
    const EditorAiAgentDebugSnapshot& snapshot, std::string_view name) {
    const auto found = std::find_if(snapshot.blackboard.begin(), snapshot.blackboard.end(),
        [&](const EditorBlackboardKeyDefinition& value) { return value.name == name; });
    return found == snapshot.blackboard.end() ? nullptr : &found->defaultValue;
}

} // namespace

EditorEqsAsset MakeDefaultEditorEqsAsset(std::string assetGuid, std::string name) {
    EditorEqsAsset asset{};
    asset.assetGuid = std::move(assetGuid);
    asset.name = std::move(name);
    asset.generator = EditorEqsGeneratorType::Ring;
    asset.radius = 12.0f;
    asset.spacing = 2.0f;
    asset.candidateCount = 16;
    asset.tests = {
        {"reachable", EditorEqsTestType::PathCost, 2.0f, 0.0f, 1.0f, true, false},
        {"distance", EditorEqsTestType::Distance, 1.0f, 0.0f, 1.0f, false, false},
        {"visibility", EditorEqsTestType::Visibility, 1.0f, 0.0f, 1.0f, false, true},
        {"crowding", EditorEqsTestType::Crowding, 0.75f, 0.0f, 1.0f, false, false},
    };
    return asset;
}

EditorEqsCompileResult CompileEditorEqs(const EditorEqsAsset& asset) {
    EditorEqsCompileResult result{};
    const auto issue = [&](std::string code, std::string test, std::string message) {
        result.diagnostics.push_back({std::move(code), std::move(test), std::move(message)});
    };
    if (asset.schemaVersion != kEditorEqsSchemaVersion)
        issue("schema", {}, "EQS schema is unsupported.");
    if (asset.assetGuid.empty()) issue("asset-guid", {}, "EQS Asset GUID is empty.");
    if (!std::isfinite(asset.radius) || asset.radius <= 0.0f)
        issue("radius", {}, "EQS radius must be finite and positive.");
    if (!std::isfinite(asset.spacing) || asset.spacing <= 0.0f)
        issue("spacing", {}, "EQS spacing must be finite and positive.");
    if (asset.candidateCount == 0 || asset.candidateCount > kEditorEqsMaximumCandidates)
        issue("candidate-budget", {}, "EQS candidate count is empty or over schema budget.");
    if (asset.tests.empty() || asset.tests.size() > kEditorEqsMaximumTests)
        issue("test-budget", {}, "EQS test count is empty or over schema budget.");
    if (asset.generator == EditorEqsGeneratorType::SmartObjects &&
        asset.smartObjectType.empty())
        issue("smart-object-type", {}, "Smart Object generator requires a type.");
    std::unordered_set<std::string> testIds;
    for (const auto& test : asset.tests) {
        if (test.id.empty() || !testIds.insert(test.id).second)
            issue("test-id", test.id, "EQS test ID is empty or duplicated.");
        if (!std::isfinite(test.weight) || test.weight < 0.0f ||
            !std::isfinite(test.minimum) || !std::isfinite(test.maximum) ||
            test.minimum < 0.0f || test.maximum > 1.0f || test.minimum > test.maximum)
            issue("test-range", test.id, "EQS test weight or normalized range is invalid.");
        if (test.type == EditorEqsTestType::SmartObjectAvailable &&
            asset.generator != EditorEqsGeneratorType::SmartObjects)
            issue("smart-object-test", test.id,
                "SmartObjectAvailable requires a SmartObjects generator.");
    }
    result.program.sourceFingerprint = HashEqs(asset);
    result.program.generator = asset.generator;
    result.program.radius = asset.radius;
    result.program.spacing = asset.spacing;
    result.program.candidateCount = asset.candidateCount;
    result.program.smartObjectType = asset.smartObjectType;
    result.program.tests = asset.tests;
    result.succeeded = result.diagnostics.empty();
    return result;
}

bool EncodeEditorEqs(
    const EditorEqsAsset& asset, std::string& output, std::string* errorMessage) {
    const EditorEqsCompileResult compiled = CompileEditorEqs(asset);
    if (!compiled.succeeded) {
        SetError(errorMessage, compiled.diagnostics.front().message);
        return false;
    }
    std::ostringstream stream;
    stream << "AI_EQS " << kEditorEqsSchemaVersion << '\n';
    stream << "asset " << std::quoted(asset.assetGuid) << ' '
           << std::quoted(asset.name) << '\n';
    stream << "generator " << ToString(asset.generator) << ' '
           << std::setprecision(9) << asset.radius << ' ' << asset.spacing << ' '
           << asset.candidateCount << ' ' << std::quoted(asset.smartObjectType) << '\n';
    stream << "tests " << asset.tests.size() << '\n';
    for (const auto& test : asset.tests)
        stream << "test " << std::quoted(test.id) << ' ' << ToString(test.type) << ' '
               << test.weight << ' ' << test.minimum << ' ' << test.maximum << ' '
               << (test.filter ? 1 : 0) << ' ' << (test.preferHigher ? 1 : 0) << '\n';
    output = stream.str();
    return true;
}

bool DecodeEditorEqs(
    std::string_view input, EditorEqsAsset& asset, std::string* errorMessage) {
    if (input.size() > kMaximumEqsAssetBytes) {
        SetError(errorMessage, "EQS Asset exceeds 4 MiB.");
        return false;
    }
    std::istringstream stream{std::string(input)};
    std::string token;
    uint32_t schema = 0;
    EditorEqsAsset decoded{};
    std::string generatorText;
    if (!(stream >> token >> schema) || token != "AI_EQS" ||
        schema != kEditorEqsSchemaVersion || !(stream >> token) || token != "asset" ||
        !(stream >> std::quoted(decoded.assetGuid) >> std::quoted(decoded.name)) ||
        !(stream >> token) || token != "generator" ||
        !(stream >> generatorText >> decoded.radius >> decoded.spacing >>
          decoded.candidateCount >> std::quoted(decoded.smartObjectType)) ||
        !ParseGenerator(generatorText, decoded.generator) ||
        !(stream >> token) || token != "tests") {
        SetError(errorMessage, "EQS Asset header is invalid.");
        return false;
    }
    std::size_t testCount = 0;
    if (!(stream >> testCount) || testCount == 0 || testCount > kEditorEqsMaximumTests) {
        SetError(errorMessage, "EQS test count is invalid.");
        return false;
    }
    for (std::size_t index = 0; index < testCount; ++index) {
        EditorEqsTestDefinition test{};
        std::string typeText;
        int filter = 0;
        int preferHigher = 0;
        if (!(stream >> token) || token != "test" ||
            !(stream >> std::quoted(test.id) >> typeText >> test.weight >> test.minimum >>
              test.maximum >> filter >> preferHigher) || !ParseTest(typeText, test.type) ||
            (filter != 0 && filter != 1) || (preferHigher != 0 && preferHigher != 1)) {
            SetError(errorMessage, "EQS test record is invalid.");
            return false;
        }
        test.filter = filter != 0;
        test.preferHigher = preferHigher != 0;
        decoded.tests.push_back(std::move(test));
    }
    const EditorEqsCompileResult compiled = CompileEditorEqs(decoded);
    if (!compiled.succeeded) {
        SetError(errorMessage, compiled.diagnostics.front().message);
        return false;
    }
    asset = std::move(decoded);
    return true;
}

bool EditorProductionAiWorldPipeline::Initialize(
    EditorProductionAiWorldPolicy policy, std::string* errorMessage) {
    Shutdown();
    policy.maximumEqsCandidates = (std::max)(1u, policy.maximumEqsCandidates);
    policy.maximumEqsResults = (std::max)(1u, policy.maximumEqsResults);
    policy.maximumEqsTestsPerQuery = (std::max)(1u, policy.maximumEqsTestsPerQuery);
    policy.maximumResidentEqsPrograms = (std::max)(1u, policy.maximumResidentEqsPrograms);
    policy.maximumCrowdAgents = (std::max)(1u, policy.maximumCrowdAgents);
    policy.maximumNeighborsPerAgent = (std::max)(1u, policy.maximumNeighborsPerAgent);
    policy.maximumSmartObjectSlots = (std::max)(1u, policy.maximumSmartObjectSlots);
    if (!std::isfinite(policy.defaultNeighborRadius) || policy.defaultNeighborRadius <= 0.0f ||
        !std::isfinite(policy.defaultAvoidanceWeight) || policy.defaultAvoidanceWeight < 0.0f) {
        SetError(errorMessage, "E-15 AI World policy contains invalid steering values.");
        return false;
    }
    if (!std::isfinite(policy.crowdPredictionSeconds) ||
        policy.crowdPredictionSeconds <= 0.0f || policy.crowdPredictionSeconds > 10.0f) {
        SetError(errorMessage, "E-15 Crowd prediction horizon is invalid.");
        return false;
    }
    policy_ = policy;
    initialized_ = true;
    return true;
}

void EditorProductionAiWorldPipeline::Shutdown() {
    programs_.clear();
    reservations_.clear();
    slots_.clear();
    crowd_.clear();
    diagnostics_.clear();
    stats_ = {};
    worldTimeSeconds_ = 0.0;
    worldGeneration_ = 0;
    queryGeneration_ = 0;
    nextReservationToken_ = 1;
    initialized_ = false;
}

bool EditorProductionAiWorldPipeline::Sync(
    const EditorScene& scene,
    const EditorWorldPartitionPipeline& worldPartition,
    const EditorProductionAiPipeline& behavior,
    float deltaTime,
    std::string* errorMessage) {
    if (!initialized_) {
        SetError(errorMessage, "E-15 AI World pipeline is not initialized.");
        return false;
    }
    deltaTime = std::clamp(std::isfinite(deltaTime) ? deltaTime : 0.0f, 0.0f, 0.25f);
    worldTimeSeconds_ += deltaTime;
    const uint32_t successfulReservations = stats_.successfulReservations;
    const uint32_t rejectedReservations = stats_.rejectedReservations;
    const uint32_t releasedReservations = stats_.releasedReservations;
    const uint32_t expiredReservations = stats_.expiredReservations;
    const uint32_t hotReloads = stats_.eqsHotReloads;
    const uint32_t loadedPrograms = static_cast<uint32_t>(programs_.size());
    stats_ = {};
    stats_.successfulReservations = successfulReservations;
    stats_.rejectedReservations = rejectedReservations;
    stats_.releasedReservations = releasedReservations;
    stats_.expiredReservations = expiredReservations;
    stats_.eqsHotReloads = hotReloads;
    stats_.loadedEqsPrograms = loadedPrograms;
    diagnostics_.clear();

    std::unordered_map<std::string, Matrix4x4> worlds;
    std::unordered_set<std::string> visiting;
    const auto resolveWorld = [&](const auto& self, const EditorSceneEntity& entity) -> Matrix4x4 {
        if (const auto found = worlds.find(entity.guid); found != worlds.end()) return found->second;
        if (!visiting.insert(entity.guid).second) return MakeIdentity4x4();
        const EditorSceneComponent* transform = scene.FindComponent(entity, kEditorTransformComponentType);
        Vector3 translation{};
        Vector3 rotation{};
        Vector3 scale{1.0f, 1.0f, 1.0f};
        if (const EditorSceneProperty* value = Property(transform, "translation")) {
            std::istringstream input(value->value); input >> translation.x >> translation.y >> translation.z;
        }
        if (const EditorSceneProperty* value = Property(transform, "rotation")) {
            std::istringstream input(value->value); input >> rotation.x >> rotation.y >> rotation.z;
        }
        if (const EditorSceneProperty* value = Property(transform, "scale")) {
            std::istringstream input(value->value); input >> scale.x >> scale.y >> scale.z;
        }
        Matrix4x4 world = MakeAffineMatrix(scale, rotation, translation);
        if (!entity.parentGuid.empty())
            if (const EditorSceneEntity* parent = scene.FindEntity(entity.parentGuid))
                world = Multiply(world, self(self, *parent));
        visiting.erase(entity.guid);
        worlds.insert_or_assign(entity.guid, world);
        return world;
    };

    std::vector<EditorSmartObjectSlot> collectedSlots;
    for (const EditorSceneEntity& entity : scene.entities) {
        if (!worldPartition.SourceResidentEntities().contains(entity.guid)) continue;
        const EditorSceneComponent* component = scene.FindComponent(
            entity, kEditorSmartObjectComponentType);
        if (component == nullptr || !component->enabled ||
            !BoolProperty(component, "enabled", true)) continue;
        ++stats_.submittedSmartObjectSlots;
        const Matrix4x4 world = resolveWorld(resolveWorld, entity);
        EditorSmartObjectSlot slot{};
        slot.entityGuid = entity.guid;
        slot.slotId = StringProperty(component, "slotId", "Primary");
        slot.type = StringProperty(component, "type", "Generic");
        slot.position = {world.m[3][0], world.m[3][1], world.m[3][2]};
        slot.interactionRadius = (std::max)(0.01f,
            FloatProperty(component, "interactionRadius", 1.0f));
        slot.priority = IntProperty(component, "priority", 0);
        slot.leaseSeconds = std::clamp(
            FloatProperty(component, "leaseSeconds", 5.0f), 0.1f, 300.0f);
        collectedSlots.push_back(std::move(slot));
    }
    std::sort(collectedSlots.begin(), collectedSlots.end(), [](const auto& a, const auto& b) {
        if (a.entityGuid != b.entityGuid) return a.entityGuid < b.entityGuid;
        return a.slotId < b.slotId;
    });
    if (collectedSlots.size() > policy_.maximumSmartObjectSlots) {
        stats_.rejectedSmartObjectSlots = static_cast<uint32_t>(
            collectedSlots.size() - policy_.maximumSmartObjectSlots);
        collectedSlots.resize(policy_.maximumSmartObjectSlots);
    }

    std::unordered_set<uint64_t> activeReservations;
    for (EditorSmartObjectSlot& slot : collectedSlots) {
        const auto reservation = std::find_if(reservations_.begin(), reservations_.end(),
            [&](const auto& value) {
                return value.second.entityGuid == slot.entityGuid &&
                    value.second.slotId == slot.slotId;
            });
        if (reservation == reservations_.end()) continue;
        if (reservation->second.expiry <= worldTimeSeconds_) {
            ++stats_.expiredReservations;
            continue;
        }
        slot.reservedByEntityGuid = reservation->second.requesterGuid;
        slot.reservationToken = reservation->second.token;
        slot.leaseExpirySeconds = reservation->second.expiry;
        activeReservations.insert(reservation->first);
    }
    std::erase_if(reservations_, [&](const auto& value) {
        const bool remove = !activeReservations.contains(value.first);
        if (remove && value.second.expiry > worldTimeSeconds_) ++stats_.releasedReservations;
        return remove;
    });
    slots_ = std::move(collectedSlots);

    struct CrowdInput {
        std::string entityGuid;
        Vector3 position{};
        Vector3 target{};
        float radius = 0.5f;
        float maximumSpeed = 3.0f;
        float neighborRadius = 4.0f;
        float avoidanceWeight = 1.5f;
        Vector3 preferredVelocity{};
    };
    std::vector<CrowdInput> inputs;
    for (const EditorSceneEntity& entity : scene.entities) {
        if (!worldPartition.SourceResidentEntities().contains(entity.guid)) continue;
        const EditorSceneComponent* component = scene.FindComponent(entity, kEditorAiAgentComponentType);
        if (component == nullptr || !component->enabled ||
            !BoolProperty(component, "crowdEnabled", true)) continue;
        ++stats_.submittedCrowdAgents;
        const Matrix4x4 world = resolveWorld(resolveWorld, entity);
        const Vector3 position{world.m[3][0], world.m[3][1], world.m[3][2]};
        Vector3 target = position;
        if (const EditorAiAgentDebugSnapshot* debug = behavior.DebugSnapshot(entity.guid)) {
            if (!debug->lastPath.empty()) target = debug->lastPath.back();
            else if (const EditorBlackboardValue* value = BlackboardValue(*debug, "TargetLocation");
                     value != nullptr && value->type == EditorBlackboardValueType::Vector3)
                target = value->vectorValue;
        }
        const float maximumSpeed = (std::max)(0.0f,
            FloatProperty(component, "maximumSpeed", 3.0f));
        inputs.push_back({entity.guid, position, target,
            (std::max)(0.05f, FloatProperty(component, "agentRadius", 0.5f)), maximumSpeed,
            (std::max)(0.1f, FloatProperty(component, "neighborRadius",
                policy_.defaultNeighborRadius)),
            (std::max)(0.0f, FloatProperty(component, "avoidanceWeight",
                policy_.defaultAvoidanceWeight)),
            Scale(NormalizeOrZero(Subtract(target, position)), maximumSpeed)});
    }
    std::sort(inputs.begin(), inputs.end(),
        [](const auto& a, const auto& b) { return a.entityGuid < b.entityGuid; });
    if (inputs.size() > policy_.maximumCrowdAgents) {
        stats_.rejectedCrowdAgents = static_cast<uint32_t>(
            inputs.size() - policy_.maximumCrowdAgents);
        inputs.resize(policy_.maximumCrowdAgents);
    }
    crowd_.clear();
    crowd_.reserve(inputs.size());
    for (const CrowdInput& input : inputs) {
        EditorCrowdAgentSnapshot output{};
        output.entityGuid = input.entityGuid;
        output.position = input.position;
        output.radius = input.radius;
        output.maximumSpeed = input.maximumSpeed;
        output.preferredVelocity = input.preferredVelocity;
        Vector3 avoidance{};
        std::vector<const CrowdInput*> neighbors;
        for (const CrowdInput& other : inputs) {
            if (other.entityGuid == input.entityGuid) continue;
            ++stats_.crowdNeighborTests;
            if (Length(Subtract(input.position, other.position)) <= input.neighborRadius)
                neighbors.push_back(&other);
        }
        std::sort(neighbors.begin(), neighbors.end(), [&](const auto* a, const auto* b) {
            const float da = Length(Subtract(input.position, a->position));
            const float db = Length(Subtract(input.position, b->position));
            if (da != db) return da < db;
            return a->entityGuid < b->entityGuid;
        });
        if (neighbors.size() > policy_.maximumNeighborsPerAgent) {
            neighbors.resize(policy_.maximumNeighborsPerAgent);
            output.constrained = true;
            ++stats_.crowdNeighborBudgetHits;
        }
        output.consideredNeighbors = static_cast<uint32_t>(neighbors.size());
        for (const CrowdInput* neighbor : neighbors) {
            const Vector3 currentAway = Subtract(input.position, neighbor->position);
            const Vector3 relativePosition = Subtract(neighbor->position, input.position);
            const Vector3 relativeVelocity = Subtract(
                neighbor->preferredVelocity, input.preferredVelocity);
            const float relativeSpeedSquared = Dot(relativeVelocity, relativeVelocity);
            const float timeToClosest = relativeSpeedSquared <= kEpsilon ? 0.0f :
                std::clamp(-Dot(relativePosition, relativeVelocity) / relativeSpeedSquared,
                    0.0f, policy_.crowdPredictionSeconds);
            const Vector3 predictedAway = Subtract(
                Add(input.position, Scale(input.preferredVelocity, timeToClosest)),
                Add(neighbor->position, Scale(neighbor->preferredVelocity, timeToClosest)));
            const float currentDistance = Length(currentAway);
            const float predictedDistance = Length(predictedAway);
            Vector3 away = predictedDistance < currentDistance ? predictedAway : currentAway;
            float distance = (std::min)(currentDistance, predictedDistance);
            if (distance <= kEpsilon) {
                const bool lower = input.entityGuid < neighbor->entityGuid;
                away = lower ? Vector3{-1.0f, 0.0f, 0.0f} : Vector3{1.0f, 0.0f, 0.0f};
                distance = 0.0f;
            }
            const float separation = input.radius + neighbor->radius;
            const float strength = std::clamp(
                1.0f - distance / input.neighborRadius +
                (std::max)(0.0f, separation - distance) / (separation + kEpsilon),
                0.0f, 2.0f);
            avoidance = Add(avoidance,
                Scale(NormalizeOrZero(away), strength * input.avoidanceWeight));
        }
        output.steeringVelocity = ClampLength(
            Add(output.preferredVelocity, avoidance), input.maximumSpeed);
        if (Length(Subtract(output.steeringVelocity, output.preferredVelocity)) > 1.0e-3f) {
            output.constrained = true;
            ++stats_.crowdAvoidanceAdjustments;
        }
        crowd_.push_back(std::move(output));
    }
    ++worldGeneration_;
    stats_.activeSmartObjectSlots = static_cast<uint32_t>(slots_.size());
    stats_.activeCrowdAgents = static_cast<uint32_t>(crowd_.size());
    stats_.worldGeneration = worldGeneration_;
    stats_.queryGeneration = queryGeneration_;
    return true;
}

bool EditorProductionAiWorldPipeline::ResolveEqsProgram(
    const EditorAssetRecord& record,
    const ResidentEqsProgram*& output,
    std::string* errorMessage) {
    output = nullptr;
    const uint64_t fileStamp = FileStamp(record.sourcePath);
    auto resident = programs_.find(record.guid);
    if (resident != programs_.end() &&
        resident->second.sourceTimestamp == record.sourceTimestamp &&
        resident->second.fileStamp == fileStamp) {
        resident->second.lastUsedQueryGeneration = queryGeneration_ + 1;
        output = &resident->second;
        return true;
    }
    if (resident == programs_.end() &&
        programs_.size() >= policy_.maximumResidentEqsPrograms) {
        const auto eviction = std::min_element(programs_.begin(), programs_.end(),
            [](const auto& a, const auto& b) {
                if (a.second.lastUsedQueryGeneration != b.second.lastUsedQueryGeneration)
                    return a.second.lastUsedQueryGeneration < b.second.lastUsedQueryGeneration;
                return a.first < b.first;
            });
        if (eviction != programs_.end()) {
            diagnostics_.push_back("EQS program cache evicted inactive Asset: " + eviction->first);
            programs_.erase(eviction);
            ++stats_.evictedEqsPrograms;
        }
    }
    std::ifstream file(record.sourcePath, std::ios::binary);
    if (!file) {
        SetError(errorMessage, "EQS source cannot be opened: " + record.sourcePath);
        return false;
    }
    std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    EditorEqsAsset asset{};
    std::string decodeError;
    if (!DecodeEditorEqs(text, asset, &decodeError) || asset.assetGuid != record.guid) {
        SetError(errorMessage, decodeError.empty()
            ? "EQS durable GUID does not match its registry record." : decodeError);
        return false;
    }
    EditorEqsCompileResult compiled = CompileEditorEqs(asset);
    if (!compiled.succeeded) {
        SetError(errorMessage, compiled.diagnostics.front().message);
        return false;
    }
    ResidentEqsProgram loaded{};
    loaded.program = std::move(compiled.program);
    loaded.sourceTimestamp = record.sourceTimestamp;
    loaded.fileStamp = fileStamp;
    loaded.lastUsedQueryGeneration = queryGeneration_ + 1;
    if (resident != programs_.end()) ++stats_.eqsHotReloads;
    programs_.insert_or_assign(record.guid, std::move(loaded));
    stats_.loadedEqsPrograms = static_cast<uint32_t>(programs_.size());
    output = &programs_.at(record.guid);
    return true;
}

EditorEqsQueryResult EditorProductionAiWorldPipeline::QueryAsset(
    std::string_view assetGuid,
    const EditorAssetRegistry& registry,
    const EditorEqsQueryContext& context,
    const EditorProductionScenePipeline& productionScene,
    EditorProductionNavigationPipeline& navigation,
    std::string* errorMessage) {
    const EditorAssetRecord* record = registry.FindByGuid(assetGuid);
    const ResidentEqsProgram* resident = nullptr;
    if (record == nullptr || record->kind != EditorAssetKind::EnvironmentQuery ||
        record->missing || !ResolveEqsProgram(*record, resident, errorMessage) ||
        resident == nullptr) {
        EditorEqsQueryResult result{};
        result.status = EditorEqsQueryStatus::InvalidAsset;
        return result;
    }
    return Query(resident->program, context, productionScene, navigation);
}

EditorEqsQueryResult EditorProductionAiWorldPipeline::Query(
    const EditorEqsProgram& program,
    const EditorEqsQueryContext& context,
    const EditorProductionScenePipeline& productionScene,
    EditorProductionNavigationPipeline& navigation) {
    EditorEqsQueryResult result{};
    if (!initialized_ || program.tests.empty() || program.radius <= 0.0f) return result;
    ++queryGeneration_;
    ++stats_.eqsQueries;
    result.queryGeneration = queryGeneration_;
    const auto navigationSnapshot = navigation.Snapshot();
    result.navigationGeneration = navigationSnapshot != nullptr
        ? navigationSnapshot->generation : 0;
    const bool overBudget = program.candidateCount > policy_.maximumEqsCandidates ||
        program.tests.size() > policy_.maximumEqsTestsPerQuery;
    if (overBudget) {
        ++stats_.eqsBudgetFailures;
        result.status = EditorEqsQueryStatus::BudgetExceeded;
        return result;
    }

    std::vector<EditorEqsItem> candidates;
    if (program.generator == EditorEqsGeneratorType::SmartObjects) {
        const std::string_view type = context.smartObjectType.empty()
            ? std::string_view(program.smartObjectType) : std::string_view(context.smartObjectType);
        for (const EditorSmartObjectSlot& slot : slots_) {
            if (slot.type != type) continue;
            EditorEqsItem item{};
            item.position = slot.position;
            item.smartObjectEntityGuid = slot.entityGuid;
            item.smartObjectSlotId = slot.slotId;
            item.smartObjectAvailable = slot.Available(worldTimeSeconds_) ||
                slot.reservedByEntityGuid == context.ownerEntityGuid;
            candidates.push_back(std::move(item));
            if (candidates.size() >= program.candidateCount) break;
        }
    } else if (program.generator == EditorEqsGeneratorType::Ring) {
        for (uint32_t index = 0; index < program.candidateCount; ++index) {
            const float angle = static_cast<float>(index) /
                static_cast<float>(program.candidateCount) * 6.2831853071795864769f;
            const float ring = program.radius *
                (0.5f + 0.5f * static_cast<float>((index % 3) + 1) / 3.0f);
            const Vector3 raw{context.origin.x + std::cos(angle) * ring,
                context.origin.y, context.origin.z + std::sin(angle) * ring};
            const EditorNavigationProjectionResult projected = navigation.ProjectPoint(
                navigationSnapshot, raw, {program.spacing, program.radius, program.spacing});
            if (projected.succeeded) candidates.push_back({projected.position});
        }
    } else {
        const uint32_t side = static_cast<uint32_t>(std::ceil(
            std::sqrt(static_cast<float>(program.candidateCount))));
        for (uint32_t z = 0; z < side && candidates.size() < program.candidateCount; ++z) {
            for (uint32_t x = 0; x < side && candidates.size() < program.candidateCount; ++x) {
                const Vector3 raw{context.origin.x +
                        (static_cast<float>(x) - static_cast<float>(side - 1) * 0.5f) * program.spacing,
                    context.origin.y,
                    context.origin.z +
                        (static_cast<float>(z) - static_cast<float>(side - 1) * 0.5f) * program.spacing};
                if (Length(Subtract(raw, context.origin)) > program.radius) continue;
                const EditorNavigationProjectionResult projected = navigation.ProjectPoint(
                    navigationSnapshot, raw, {program.spacing, program.radius, program.spacing});
                if (projected.succeeded) candidates.push_back({projected.position});
            }
        }
    }
    result.generatedCandidates = static_cast<uint32_t>(candidates.size());
    if (program.generator != EditorEqsGeneratorType::SmartObjects) {
        std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
            if (a.position.x != b.position.x) return a.position.x < b.position.x;
            if (a.position.z != b.position.z) return a.position.z < b.position.z;
            return a.position.y < b.position.y;
        });
        candidates.erase(std::unique(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
            return Length(Subtract(a.position, b.position)) <= 1.0e-4f;
        }), candidates.end());
        result.generatedCandidates = static_cast<uint32_t>(candidates.size());
    }
    stats_.eqsGeneratedCandidates += result.generatedCandidates;
    for (EditorEqsItem& item : candidates) {
        bool rejected = false;
        float weightedScore = 0.0f;
        float totalWeight = 0.0f;
        item.distance = Length(Subtract(item.position, context.target));
        for (const EditorEqsTestDefinition& test : program.tests) {
            float normalized = 0.0f;
            bool valid = true;
            switch (test.type) {
            case EditorEqsTestType::Distance:
                normalized = std::clamp(item.distance / program.radius, 0.0f, 1.0f);
                break;
            case EditorEqsTestType::PathCost: {
                ++stats_.eqsNavigationQueries;
                const EditorNavigationPathResult path = navigation.FindPath(
                    navigationSnapshot, context.origin, item.position);
                valid = path.Succeeded();
                item.pathCost = valid ? path.totalCost : std::numeric_limits<float>::infinity();
                normalized = valid
                    ? std::clamp(item.pathCost / (program.radius * 2.0f), 0.0f, 1.0f) : 1.0f;
                break;
            }
            case EditorEqsTestType::Visibility: {
                ++stats_.eqsVisibilityQueries;
                const Vector3 delta = Subtract(item.position, context.origin);
                const float distance = Length(delta);
                const EditorProductionSceneRayHit hit = productionScene.Raycast(
                    context.origin, NormalizeOrZero(delta), distance);
                item.visible = !hit.valid || hit.distance >= distance - 0.1f ||
                    hit.entityGuid == item.smartObjectEntityGuid;
                normalized = item.visible ? 1.0f : 0.0f;
                break;
            }
            case EditorEqsTestType::Crowding:
                item.nearbyAgents = static_cast<uint32_t>(std::count_if(
                    crowd_.begin(), crowd_.end(), [&](const auto& agent) {
                        return agent.entityGuid != context.ownerEntityGuid &&
                            Length(Subtract(agent.position, item.position)) <=
                                policy_.defaultNeighborRadius;
                    }));
                normalized = std::clamp(static_cast<float>(item.nearbyAgents) / 8.0f,
                    0.0f, 1.0f);
                break;
            case EditorEqsTestType::SmartObjectAvailable:
                normalized = item.smartObjectAvailable ? 1.0f : 0.0f;
                valid = item.smartObjectAvailable;
                break;
            }
            if (test.filter && (!valid || normalized < test.minimum || normalized > test.maximum)) {
                rejected = true;
                break;
            }
            const float scored = test.preferHigher ? normalized : 1.0f - normalized;
            weightedScore += scored * test.weight;
            totalWeight += test.weight;
        }
        ++result.testedCandidates;
        if (rejected) {
            ++result.rejectedCandidates;
            item.score = -std::numeric_limits<float>::infinity();
        } else {
            item.score = totalWeight > kEpsilon ? weightedScore / totalWeight : 0.0f;
        }
    }
    std::erase_if(candidates,
        [](const EditorEqsItem& value) { return !std::isfinite(value.score); });
    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
        if (a.score != b.score) return a.score > b.score;
        if (a.smartObjectEntityGuid != b.smartObjectEntityGuid)
            return a.smartObjectEntityGuid < b.smartObjectEntityGuid;
        if (a.smartObjectSlotId != b.smartObjectSlotId)
            return a.smartObjectSlotId < b.smartObjectSlotId;
        if (a.position.x != b.position.x) return a.position.x < b.position.x;
        if (a.position.z != b.position.z) return a.position.z < b.position.z;
        return a.position.y < b.position.y;
    });
    if (candidates.size() > policy_.maximumEqsResults)
        candidates.resize(policy_.maximumEqsResults);
    result.items = std::move(candidates);
    result.status = result.items.empty()
        ? EditorEqsQueryStatus::NoCandidates : EditorEqsQueryStatus::Succeeded;
    stats_.eqsTestedCandidates += result.testedCandidates;
    stats_.eqsRejectedCandidates += result.rejectedCandidates;
    stats_.queryGeneration = queryGeneration_;
    return result;
}

EditorSmartObjectReservation EditorProductionAiWorldPipeline::ReserveSmartObject(
    const EditorSmartObjectReservationRequest& request) {
    EditorSmartObjectReservation output{};
    if (!initialized_ || request.requesterEntityGuid.empty() ||
        !std::isfinite(request.maximumDistance) || request.maximumDistance < 0.0f) {
        ++stats_.rejectedReservations;
        return output;
    }
    std::vector<EditorSmartObjectSlot*> candidates;
    for (EditorSmartObjectSlot& slot : slots_) {
        if (slot.reservationToken != 0 && slot.leaseExpirySeconds <= worldTimeSeconds_) {
            reservations_.erase(slot.reservationToken);
            slot.reservedByEntityGuid.clear();
            slot.reservationToken = 0;
            slot.leaseExpirySeconds = 0.0;
            ++stats_.expiredReservations;
        }
        if (!request.smartObjectEntityGuid.empty() &&
            slot.entityGuid != request.smartObjectEntityGuid) continue;
        if (!request.slotId.empty() && slot.slotId != request.slotId) continue;
        if (!request.type.empty() && slot.type != request.type) continue;
        if (!slot.Available(worldTimeSeconds_) &&
            slot.reservedByEntityGuid != request.requesterEntityGuid) continue;
        if (Length(Subtract(slot.position, request.requesterPosition)) > request.maximumDistance)
            continue;
        candidates.push_back(&slot);
    }
    std::sort(candidates.begin(), candidates.end(), [&](const auto* a, const auto* b) {
        if (a->priority != b->priority) return a->priority > b->priority;
        const float da = Length(Subtract(a->position, request.requesterPosition));
        const float db = Length(Subtract(b->position, request.requesterPosition));
        if (da != db) return da < db;
        if (a->entityGuid != b->entityGuid) return a->entityGuid < b->entityGuid;
        return a->slotId < b->slotId;
    });
    if (candidates.empty()) {
        ++stats_.rejectedReservations;
        return output;
    }
    EditorSmartObjectSlot& slot = *candidates.front();
    if (slot.reservationToken != 0 &&
        slot.reservedByEntityGuid == request.requesterEntityGuid) {
        return {true, slot.entityGuid, slot.slotId, slot.reservationToken,
            slot.leaseExpirySeconds};
    }
    const uint64_t token = nextReservationToken_++;
    ReservationState state{};
    state.entityGuid = slot.entityGuid;
    state.slotId = slot.slotId;
    state.requesterGuid = request.requesterEntityGuid;
    state.token = token;
    state.expiry = worldTimeSeconds_ + slot.leaseSeconds;
    reservations_.emplace(token, state);
    slot.reservedByEntityGuid = state.requesterGuid;
    slot.reservationToken = token;
    slot.leaseExpirySeconds = state.expiry;
    ++stats_.successfulReservations;
    return {true, slot.entityGuid, slot.slotId, token, state.expiry};
}

bool EditorProductionAiWorldPipeline::RenewSmartObjectReservation(uint64_t token) {
    const auto reservation = reservations_.find(token);
    if (reservation == reservations_.end()) return false;
    EditorSmartObjectSlot* slot = nullptr;
    for (EditorSmartObjectSlot& value : slots_)
        if (value.entityGuid == reservation->second.entityGuid &&
            value.slotId == reservation->second.slotId) { slot = &value; break; }
    if (slot == nullptr || reservation->second.expiry <= worldTimeSeconds_) return false;
    reservation->second.expiry = worldTimeSeconds_ + slot->leaseSeconds;
    slot->leaseExpirySeconds = reservation->second.expiry;
    return true;
}

bool EditorProductionAiWorldPipeline::ReleaseSmartObjectReservation(uint64_t token) {
    const auto reservation = reservations_.find(token);
    if (reservation == reservations_.end()) return false;
    for (EditorSmartObjectSlot& slot : slots_)
        if (slot.reservationToken == token) {
            slot.reservedByEntityGuid.clear();
            slot.reservationToken = 0;
            slot.leaseExpirySeconds = 0.0;
            break;
        }
    reservations_.erase(reservation);
    ++stats_.releasedReservations;
    return true;
}

const EditorSmartObjectSlot* EditorProductionAiWorldPipeline::FindSmartObjectSlot(
    std::string_view entityGuid, std::string_view slotId) const {
    const auto found = std::find_if(slots_.begin(), slots_.end(), [&](const auto& value) {
        return value.entityGuid == entityGuid && value.slotId == slotId;
    });
    return found == slots_.end() ? nullptr : &*found;
}

const EditorCrowdAgentSnapshot* EditorProductionAiWorldPipeline::CrowdSnapshot(
    std::string_view entityGuid) const {
    const auto found = std::find_if(crowd_.begin(), crowd_.end(),
        [&](const auto& value) { return value.entityGuid == entityGuid; });
    return found == crowd_.end() ? nullptr : &*found;
}

const char* ToString(EditorEqsGeneratorType value) noexcept {
    switch (value) {
    case EditorEqsGeneratorType::Ring: return "Ring";
    case EditorEqsGeneratorType::Grid: return "Grid";
    case EditorEqsGeneratorType::SmartObjects: return "SmartObjects";
    }
    return "Ring";
}

const char* ToString(EditorEqsTestType value) noexcept {
    switch (value) {
    case EditorEqsTestType::Distance: return "Distance";
    case EditorEqsTestType::PathCost: return "PathCost";
    case EditorEqsTestType::Visibility: return "Visibility";
    case EditorEqsTestType::Crowding: return "Crowding";
    case EditorEqsTestType::SmartObjectAvailable: return "SmartObjectAvailable";
    }
    return "Distance";
}

const char* ToString(EditorEqsQueryStatus value) noexcept {
    switch (value) {
    case EditorEqsQueryStatus::Succeeded: return "Succeeded";
    case EditorEqsQueryStatus::NoCandidates: return "NoCandidates";
    case EditorEqsQueryStatus::InvalidAsset: return "InvalidAsset";
    case EditorEqsQueryStatus::BudgetExceeded: return "BudgetExceeded";
    }
    return "InvalidAsset";
}

} // namespace editor
