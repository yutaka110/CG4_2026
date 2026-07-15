#pragma once

#include "EditorGeometryEditCommand.h"
#include "../EditorSelection.h"
#include "../EditorViewportCoordinateService.h"
#include "../documents/EditorDocumentId.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

class SceneWorldObjectProvider;

enum class EditorGeometryElementMode : uint32_t {
    Vertex = 0,
    Edge = 1,
    Face = 2,
};

struct EditorGeometryTransform {
    Vector3 translation{};
    Vector3 rotation{};
    Vector3 scale{1.0f, 1.0f, 1.0f};

    Vector3 TransformPoint(const Vector3& local) const;
};

struct EditorGeometryFaceHit {
    std::string faceGuid;
    Vector3 position{};
    float rayDistance = 0.0f;
    bool valid = false;
};

class EditorGeometryWorkspace {
public:
    void Bind(
        SceneWorldObjectProvider* provider,
        const EditorSelection* selection,
        const EditorDocumentId& activeDocument);
    void Clear();
    void RefreshFromScene();

    bool CanEdit() const noexcept;
    bool HasGeometry() const noexcept { return authored_.has_value(); }
    const EditorGeometryMesh* AuthoredMesh() const noexcept;
    const EditorGeometryMesh* DisplayMesh() const noexcept;
    const EditorGeometryTransform& Transform() const noexcept { return transform_; }
    const EditorObjectHandle& Target() const noexcept { return target_; }
    const EditorDocumentId& Document() const noexcept { return document_; }
    const std::string& EntityGuid() const noexcept { return entityGuid_; }

    EditorGeometryPropertyState AuthoredState() const;
    bool SetPreview(EditorGeometryMesh mesh, std::string* errorMessage = nullptr);
    void SetCollisionPreview(EditorGeneratedCollision collision);
    void ClearPreview();
    bool HasPreview() const noexcept { return preview_.has_value() || collisionPreview_.has_value(); }
    EditorGeometryPropertyState PreviewState(std::string* errorMessage = nullptr) const;

    EditorGeometryElementMode ElementMode() const noexcept { return elementMode_; }
    void SetElementMode(EditorGeometryElementMode mode);
    const std::vector<std::string>& SelectedFaces() const noexcept { return selectedFaces_; }
    void SelectFace(std::string guid, bool additive);
    void ClearElementSelection();
    void PruneElementSelection();

    EditorGeometryFaceHit PickFace(
        const EditorViewportCoordinateService& coordinates,
        float displayX,
        float displayY) const;

private:
    SceneWorldObjectProvider* provider_ = nullptr;
    EditorDocumentId document_{};
    EditorObjectHandle target_{};
    std::string entityGuid_;
    std::optional<EditorGeometryMesh> authored_;
    std::optional<EditorGeometryMesh> preview_;
    std::optional<EditorGeneratedCollision> collisionPreview_;
    std::optional<std::string> authoredGeometryText_;
    std::optional<std::string> authoredCollisionText_;
    EditorGeometryTransform transform_{};
    EditorGeometryElementMode elementMode_ = EditorGeometryElementMode::Face;
    std::vector<std::string> selectedFaces_;
};

} // namespace editor
