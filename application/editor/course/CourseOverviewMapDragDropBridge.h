#pragma once

#include <string>

#include "CourseEnemyEditorController.h"
#include "CourseOverviewMapSnapService.h"
#include "../EditorAssetRegistry.h"
#include "../EditorSelection.h"

namespace editor {

struct CourseOverviewMapAssetDropRequest final {
    const EditorAssetRegistry* registry = nullptr;
    std::string assetGuid;
    Vector2 mapPosition{};
};

struct CourseOverviewMapAssetDropResult final {
    bool accepted = false;
    bool succeeded = false;
    std::string placementGuid;
    std::string actorAssetId;
    std::string message;
};

// Validates Content Browser payloads as ActorAssets, maps the drop through the
// shared projection/snap path, and commits one AddPlacements transaction.
class CourseOverviewMapDragDropBridge final {
public:
    void Bind(
        CourseEnemyEditorController* enemies,
        EditorSelection* selection,
        const CourseOverviewMapSnapService* snapping);
    CourseOverviewMapAssetDropResult DropActorAsset(
        const CourseOverviewMapAssetDropRequest& request,
        const CourseOverviewMapProjection& projection);

    bool CanAccept(const EditorAssetRecord& record) const;
    const CourseOverviewMapAssetDropResult& LastResult() const noexcept {
        return lastResult_;
    }

private:
    CourseEnemyEditorController* enemies_ = nullptr;
    EditorSelection* selection_ = nullptr;
    const CourseOverviewMapSnapService* snapping_ = nullptr;
    CourseOverviewMapAssetDropResult lastResult_{};
};

} // namespace editor
