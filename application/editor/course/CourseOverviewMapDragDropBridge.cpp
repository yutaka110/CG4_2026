#include "CourseOverviewMapDragDropBridge.h"

#include "../../course/CourseActorAsset.h"

#include <filesystem>
#include <algorithm>
#include <cctype>
#include <limits>

namespace editor {

void CourseOverviewMapDragDropBridge::Bind(
    CourseEnemyEditorController* enemies,
    EditorSelection* selection,
    const CourseOverviewMapSnapService* snapping) {
    enemies_ = enemies;
    selection_ = selection;
    snapping_ = snapping;
}

bool CourseOverviewMapDragDropBridge::CanAccept(
    const EditorAssetRecord& record) const {
    const std::filesystem::path path = record.sourcePath.empty()
        ? std::filesystem::path(record.logicalPath)
        : std::filesystem::path(record.sourcePath);
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return record.referenceable && !record.missing &&
        extension == ".actor";
}

CourseOverviewMapAssetDropResult CourseOverviewMapDragDropBridge::DropActorAsset(
    const CourseOverviewMapAssetDropRequest& request,
    const CourseOverviewMapProjection& projection) {
    lastResult_ = {};
    const auto fail = [this](std::string message, bool accepted = false) {
        lastResult_.accepted = accepted;
        lastResult_.message = std::move(message);
        return lastResult_;
    };
    if (request.registry == nullptr || enemies_ == nullptr || snapping_ == nullptr) {
        return fail("Overview Map ActorAsset drop bridge is not bound.");
    }
    const EditorAssetRecord* record = request.registry->FindByGuid(request.assetGuid);
    if (record == nullptr || !CanAccept(*record)) {
        return fail("Only a valid .actor Asset can be dropped onto Course Overview Map.");
    }
    if (!enemies_->State().bound || !enemies_->State().authoringAllowed ||
        enemies_->State().status != CourseEnemyEditorControllerStatus::Ready ||
        enemies_->Model() == nullptr) {
        return fail("Course enemy authoring is read-only or unavailable.", true);
    }

    const std::filesystem::path path = record->sourcePath.empty()
        ? std::filesystem::path(record->logicalPath)
        : std::filesystem::path(record->sourcePath);
    CourseActorAsset actor{};
    std::string loadError;
    if (!actor.LoadFromFile(path.string(), &loadError) || actor.id.empty()) {
        return fail(loadError.empty() ? "ActorAsset could not be loaded." : loadError, true);
    }
    const CourseOverviewMapSnapResult snapped = snapping_->SnapRailAnchor(
        request.mapPosition, (std::numeric_limits<float>::quiet_NaN)(),
        projection, enemies_->Model()->RailModel());
    if (!snapped.valid) {
        return fail("ActorAsset drop could not be projected onto the course rail.", true);
    }

    CourseEnemyPlacement placement{};
    placement.actorAssetId = actor.id;
    placement.railAnchor = snapped.railAnchor;
    CourseEnemyMutationRequest mutation{};
    mutation.kind = CourseEnemyMutationKind::AddPlacements;
    mutation.expectedRevision = enemies_->State().mutationRevision;
    mutation.label = "Place ActorAsset From Overview Map";
    mutation.placements.push_back(std::move(placement));
    const CourseEnemyMutationResult result = enemies_->Mutate(mutation);
    lastResult_.accepted = true;
    lastResult_.succeeded = result.succeeded;
    lastResult_.actorAssetId = actor.id;
    lastResult_.message = result.message;
    if (!result.succeeded || result.affectedPlacementGuids.empty()) return lastResult_;

    lastResult_.placementGuid = result.affectedPlacementGuids.front();
    if (selection_ != nullptr) {
        const auto index = enemies_->Model()->FindIndex(lastResult_.placementGuid);
        selection_->SetPrimary({EditorDomainId::CourseEnemyPlacement,
            "course-enemy-placement:" + lastResult_.placementGuid,
            index.has_value() ? static_cast<uint64_t>(*index) : 0,
            enemies_->State().bindingGeneration,
            actor.id});
    }
    return lastResult_;
}

} // namespace editor
