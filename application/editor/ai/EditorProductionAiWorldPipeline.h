#pragma once

#include "EditorProductionAiPipeline.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace editor {

inline constexpr uint32_t kEditorEqsSchemaVersion = 1;
inline constexpr uint32_t kEditorEqsMaximumTests = 64;
inline constexpr uint32_t kEditorEqsMaximumCandidates = 512;

enum class EditorEqsGeneratorType : uint8_t {
    Ring,
    Grid,
    SmartObjects,
};

enum class EditorEqsTestType : uint8_t {
    Distance,
    PathCost,
    Visibility,
    Crowding,
    SmartObjectAvailable,
};

struct EditorEqsTestDefinition {
    std::string id;
    EditorEqsTestType type = EditorEqsTestType::Distance;
    float weight = 1.0f;
    float minimum = 0.0f;
    float maximum = 1.0f;
    bool filter = false;
    bool preferHigher = true;
};

struct EditorEqsAsset {
    uint32_t schemaVersion = kEditorEqsSchemaVersion;
    std::string assetGuid;
    std::string name;
    EditorEqsGeneratorType generator = EditorEqsGeneratorType::Ring;
    float radius = 12.0f;
    float spacing = 2.0f;
    uint32_t candidateCount = 16;
    std::string smartObjectType;
    std::vector<EditorEqsTestDefinition> tests;
};

struct EditorEqsDiagnostic {
    std::string code;
    std::string testId;
    std::string message;
};

struct EditorEqsProgram {
    uint64_t sourceFingerprint = 0;
    EditorEqsGeneratorType generator = EditorEqsGeneratorType::Ring;
    float radius = 12.0f;
    float spacing = 2.0f;
    uint32_t candidateCount = 16;
    std::string smartObjectType;
    std::vector<EditorEqsTestDefinition> tests;
};

struct EditorEqsCompileResult {
    bool succeeded = false;
    EditorEqsProgram program{};
    std::vector<EditorEqsDiagnostic> diagnostics;
};

EditorEqsAsset MakeDefaultEditorEqsAsset(std::string assetGuid, std::string name);
EditorEqsCompileResult CompileEditorEqs(const EditorEqsAsset& asset);
bool EncodeEditorEqs(const EditorEqsAsset& asset, std::string& output,
    std::string* errorMessage = nullptr);
bool DecodeEditorEqs(std::string_view input, EditorEqsAsset& asset,
    std::string* errorMessage = nullptr);

enum class EditorEqsQueryStatus : uint8_t {
    Succeeded,
    NoCandidates,
    InvalidAsset,
    BudgetExceeded,
};

struct EditorEqsQueryContext {
    std::string ownerEntityGuid;
    Vector3 origin{};
    Vector3 target{};
    std::string smartObjectType;
};

struct EditorEqsItem {
    Vector3 position{};
    std::string smartObjectEntityGuid;
    std::string smartObjectSlotId;
    float score = 0.0f;
    float distance = 0.0f;
    float pathCost = 0.0f;
    uint32_t nearbyAgents = 0;
    bool visible = false;
    bool smartObjectAvailable = false;
};

struct EditorEqsQueryResult {
    EditorEqsQueryStatus status = EditorEqsQueryStatus::InvalidAsset;
    uint64_t queryGeneration = 0;
    uint64_t navigationGeneration = 0;
    uint32_t generatedCandidates = 0;
    uint32_t testedCandidates = 0;
    uint32_t rejectedCandidates = 0;
    std::vector<EditorEqsItem> items;
    bool Succeeded() const noexcept { return status == EditorEqsQueryStatus::Succeeded; }
};

struct EditorSmartObjectSlot {
    std::string entityGuid;
    std::string slotId;
    std::string type;
    Vector3 position{};
    float interactionRadius = 1.0f;
    int32_t priority = 0;
    float leaseSeconds = 5.0f;
    std::string reservedByEntityGuid;
    uint64_t reservationToken = 0;
    double leaseExpirySeconds = 0.0;

    bool Available(double nowSeconds) const noexcept {
        return reservedByEntityGuid.empty() || leaseExpirySeconds <= nowSeconds;
    }
};

struct EditorSmartObjectReservationRequest {
    std::string requesterEntityGuid;
    std::string smartObjectEntityGuid;
    std::string slotId;
    std::string type;
    Vector3 requesterPosition{};
    float maximumDistance = 100.0f;
};

struct EditorSmartObjectReservation {
    bool succeeded = false;
    std::string smartObjectEntityGuid;
    std::string slotId;
    uint64_t token = 0;
    double leaseExpirySeconds = 0.0;
};

struct EditorCrowdAgentSnapshot {
    std::string entityGuid;
    Vector3 position{};
    Vector3 preferredVelocity{};
    Vector3 steeringVelocity{};
    float radius = 0.5f;
    float maximumSpeed = 3.0f;
    uint32_t consideredNeighbors = 0;
    bool constrained = false;
};

struct EditorProductionAiWorldPolicy {
    uint32_t maximumEqsCandidates = 256;
    uint32_t maximumEqsResults = 16;
    uint32_t maximumEqsTestsPerQuery = 32;
    uint32_t maximumResidentEqsPrograms = 64;
    uint32_t maximumCrowdAgents = 256;
    uint32_t maximumNeighborsPerAgent = 16;
    uint32_t maximumSmartObjectSlots = 1024;
    float defaultNeighborRadius = 4.0f;
    float defaultAvoidanceWeight = 1.5f;
    float crowdPredictionSeconds = 1.0f;
};

struct EditorProductionAiWorldStats {
    uint32_t eqsQueries = 0;
    uint32_t eqsGeneratedCandidates = 0;
    uint32_t eqsTestedCandidates = 0;
    uint32_t eqsRejectedCandidates = 0;
    uint32_t eqsBudgetFailures = 0;
    uint32_t eqsNavigationQueries = 0;
    uint32_t eqsVisibilityQueries = 0;
    uint32_t loadedEqsPrograms = 0;
    uint32_t eqsHotReloads = 0;
    uint32_t evictedEqsPrograms = 0;
    uint32_t submittedCrowdAgents = 0;
    uint32_t activeCrowdAgents = 0;
    uint32_t rejectedCrowdAgents = 0;
    uint32_t crowdNeighborTests = 0;
    uint32_t crowdAvoidanceAdjustments = 0;
    uint32_t crowdNeighborBudgetHits = 0;
    uint32_t submittedSmartObjectSlots = 0;
    uint32_t activeSmartObjectSlots = 0;
    uint32_t rejectedSmartObjectSlots = 0;
    uint32_t successfulReservations = 0;
    uint32_t rejectedReservations = 0;
    uint32_t releasedReservations = 0;
    uint32_t expiredReservations = 0;
    uint64_t worldGeneration = 0;
    uint64_t queryGeneration = 0;
};

class EditorProductionAiWorldPipeline {
public:
    bool Initialize(EditorProductionAiWorldPolicy policy = {},
        std::string* errorMessage = nullptr);
    void Shutdown();

    bool Sync(const EditorScene& scene,
        const EditorWorldPartitionPipeline& worldPartition,
        const EditorProductionAiPipeline& behavior,
        float deltaTime,
        std::string* errorMessage = nullptr);

    EditorEqsQueryResult Query(const EditorEqsProgram& program,
        const EditorEqsQueryContext& context,
        const EditorProductionScenePipeline& productionScene,
        EditorProductionNavigationPipeline& navigation);
    EditorEqsQueryResult QueryAsset(std::string_view assetGuid,
        const EditorAssetRegistry& registry,
        const EditorEqsQueryContext& context,
        const EditorProductionScenePipeline& productionScene,
        EditorProductionNavigationPipeline& navigation,
        std::string* errorMessage = nullptr);

    EditorSmartObjectReservation ReserveSmartObject(
        const EditorSmartObjectReservationRequest& request);
    bool RenewSmartObjectReservation(uint64_t token);
    bool ReleaseSmartObjectReservation(uint64_t token);

    const EditorSmartObjectSlot* FindSmartObjectSlot(
        std::string_view entityGuid, std::string_view slotId) const;
    const EditorCrowdAgentSnapshot* CrowdSnapshot(std::string_view entityGuid) const;
    const std::vector<EditorSmartObjectSlot>& SmartObjectSlots() const noexcept { return slots_; }
    const std::vector<EditorCrowdAgentSnapshot>& CrowdSnapshots() const noexcept { return crowd_; }
    const EditorProductionAiWorldPolicy& Policy() const noexcept { return policy_; }
    const EditorProductionAiWorldStats& Stats() const noexcept { return stats_; }
    const std::vector<std::string>& Diagnostics() const noexcept { return diagnostics_; }

private:
    struct ResidentEqsProgram {
        EditorEqsProgram program{};
        uint64_t sourceTimestamp = 0;
        uint64_t fileStamp = 0;
        uint64_t lastUsedQueryGeneration = 0;
    };
    struct ReservationState {
        std::string entityGuid;
        std::string slotId;
        std::string requesterGuid;
        uint64_t token = 0;
        double expiry = 0.0;
    };

    bool ResolveEqsProgram(const EditorAssetRecord& record,
        const ResidentEqsProgram*& output, std::string* errorMessage);

    bool initialized_ = false;
    double worldTimeSeconds_ = 0.0;
    uint64_t worldGeneration_ = 0;
    uint64_t queryGeneration_ = 0;
    uint64_t nextReservationToken_ = 1;
    EditorProductionAiWorldPolicy policy_{};
    std::unordered_map<std::string, ResidentEqsProgram> programs_;
    std::unordered_map<uint64_t, ReservationState> reservations_;
    std::vector<EditorSmartObjectSlot> slots_;
    std::vector<EditorCrowdAgentSnapshot> crowd_;
    EditorProductionAiWorldStats stats_{};
    std::vector<std::string> diagnostics_;
};

const char* ToString(EditorEqsGeneratorType value) noexcept;
const char* ToString(EditorEqsTestType value) noexcept;
const char* ToString(EditorEqsQueryStatus value) noexcept;

} // namespace editor
