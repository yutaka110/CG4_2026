#include "EditorSplineRouteTool.h"

#include "EditorPlacementQueryService.h"
#include "../EditorSelection.h"
#include "../EditorViewportOverlay.h"
#include "../scene/EditorScene.h"
#include "../scene/EditorSplineRouteComponent.h"
#include "../scene/EditorSplineRouteEvaluationService.h"
#include "../world/EditorWorldModel.h"
#include "../world/EditorWorldMutationService.h"
#include "../world/SceneWorldObjectProvider.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace editor {
namespace {

enum class SplineEditOperation {
    Move,
    Add,
    Delete,
};

struct AffineTransform {
    double basis[3][3]{
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {0.0, 0.0, 1.0},
    };
    double translation[3]{0.0, 0.0, 0.0};
};

const EditorSceneProperty* FindProperty(
    const EditorSceneComponent& component,
    std::string_view name) {
    const auto found = std::find_if(
        component.properties.begin(), component.properties.end(),
        [&](const EditorSceneProperty& property) {
            return property.name == name;
        });
    return found == component.properties.end() ? nullptr : &*found;
}

bool ParseVector3(std::string_view text, Vector3& value) {
    std::istringstream input{std::string(text)};
    input.imbue(std::locale::classic());
    if (!(input >> value.x >> value.y >> value.z) ||
        !std::isfinite(value.x) ||
        !std::isfinite(value.y) ||
        !std::isfinite(value.z)) {
        return false;
    }
    input >> std::ws;
    return input.eof();
}

AffineTransform Multiply(
    const AffineTransform& parent,
    const AffineTransform& local) {
    AffineTransform result{};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            result.basis[row][column] = 0.0;
            for (std::size_t index = 0; index < 3; ++index) {
                result.basis[row][column] +=
                    parent.basis[row][index] *
                    local.basis[index][column];
            }
        }
        result.translation[row] = parent.translation[row];
        for (std::size_t index = 0; index < 3; ++index) {
            result.translation[row] +=
                parent.basis[row][index] *
                local.translation[index];
        }
    }
    return result;
}

AffineTransform MakeLocalTransform(
    const Vector3& translation,
    const Vector3& rotation,
    const Vector3& scale) {
    const double cx = std::cos(rotation.x);
    const double sx = std::sin(rotation.x);
    const double cy = std::cos(rotation.y);
    const double sy = std::sin(rotation.y);
    const double cz = std::cos(rotation.z);
    const double sz = std::sin(rotation.z);
    const double rotationMatrix[3][3]{
        {cz * cy, cz * sy * sx - sz * cx,
            cz * sy * cx + sz * sx},
        {sz * cy, sz * sy * sx + cz * cx,
            sz * sy * cx - cz * sx},
        {-sy, cy * sx, cy * cx},
    };
    const double scales[3]{scale.x, scale.y, scale.z};
    AffineTransform result{};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            result.basis[row][column] =
                rotationMatrix[row][column] * scales[column];
        }
    }
    result.translation[0] = translation.x;
    result.translation[1] = translation.y;
    result.translation[2] = translation.z;
    return result;
}

bool LocalTransform(
    const EditorScene& scene,
    const EditorSceneEntity& entity,
    AffineTransform& output,
    std::string& error) {
    const EditorSceneComponent* transform =
        scene.FindComponent(entity, kEditorTransformComponentType);
    if (transform == nullptr || !transform->enabled) {
        output = {};
        return true;
    }
    Vector3 translation{};
    Vector3 rotation{};
    Vector3 scale{1.0f, 1.0f, 1.0f};
    const std::array<std::pair<std::string_view, Vector3*>, 3> fields{{
        {"translation", &translation},
        {"rotation", &rotation},
        {"scale", &scale},
    }};
    for (const auto& [name, target] : fields) {
        const EditorSceneProperty* property =
            FindProperty(*transform, name);
        if (property == nullptr ||
            !ParseVector3(property->value, *target)) {
            error = "Spline Route Entity has an invalid Transform.";
            return false;
        }
    }
    output = MakeLocalTransform(translation, rotation, scale);
    return true;
}

bool WorldTransform(
    const EditorScene& scene,
    const EditorSceneEntity& entity,
    std::unordered_map<std::string, AffineTransform>& cache,
    std::unordered_set<std::string>& resolving,
    AffineTransform& output,
    std::string& error) {
    const auto cached = cache.find(entity.guid);
    if (cached != cache.end()) {
        output = cached->second;
        return true;
    }
    if (!resolving.insert(entity.guid).second) {
        error = "Scene hierarchy contains a Transform cycle.";
        return false;
    }
    AffineTransform local{};
    if (!LocalTransform(scene, entity, local, error)) {
        resolving.erase(entity.guid);
        return false;
    }
    output = local;
    if (!entity.parentGuid.empty()) {
        const EditorSceneEntity* parent =
            scene.FindEntity(entity.parentGuid);
        if (parent == nullptr) {
            error = "Spline Route Entity has a missing parent.";
            resolving.erase(entity.guid);
            return false;
        }
        AffineTransform parentWorld{};
        if (!WorldTransform(
                scene, *parent, cache, resolving,
                parentWorld, error)) {
            resolving.erase(entity.guid);
            return false;
        }
        output = Multiply(parentWorld, local);
    }
    resolving.erase(entity.guid);
    cache.emplace(entity.guid, output);
    return true;
}

Vector3 TransformPoint(
    const AffineTransform& transform,
    const Vector3& point) {
    return {
        static_cast<float>(
            transform.translation[0] +
            transform.basis[0][0] * point.x +
            transform.basis[0][1] * point.y +
            transform.basis[0][2] * point.z),
        static_cast<float>(
            transform.translation[1] +
            transform.basis[1][0] * point.x +
            transform.basis[1][1] * point.y +
            transform.basis[1][2] * point.z),
        static_cast<float>(
            transform.translation[2] +
            transform.basis[2][0] * point.x +
            transform.basis[2][1] * point.y +
            transform.basis[2][2] * point.z),
    };
}

bool InverseTransformPoint(
    const AffineTransform& transform,
    const Vector3& world,
    Vector3& local) {
    const double a = transform.basis[0][0];
    const double b = transform.basis[0][1];
    const double c = transform.basis[0][2];
    const double d = transform.basis[1][0];
    const double e = transform.basis[1][1];
    const double f = transform.basis[1][2];
    const double g = transform.basis[2][0];
    const double h = transform.basis[2][1];
    const double i = transform.basis[2][2];
    const double determinant =
        a * (e * i - f * h) -
        b * (d * i - f * g) +
        c * (d * h - e * g);
    if (!std::isfinite(determinant) ||
        std::abs(determinant) <= 1.0e-10) {
        return false;
    }
    const double inverse[3][3]{
        {(e * i - f * h) / determinant,
         (c * h - b * i) / determinant,
         (b * f - c * e) / determinant},
        {(f * g - d * i) / determinant,
         (a * i - c * g) / determinant,
         (c * d - a * f) / determinant},
        {(d * h - e * g) / determinant,
         (b * g - a * h) / determinant,
         (a * e - b * d) / determinant},
    };
    const double delta[3]{
        world.x - transform.translation[0],
        world.y - transform.translation[1],
        world.z - transform.translation[2],
    };
    local = {
        static_cast<float>(
            inverse[0][0] * delta[0] +
            inverse[0][1] * delta[1] +
            inverse[0][2] * delta[2]),
        static_cast<float>(
            inverse[1][0] * delta[0] +
            inverse[1][1] * delta[1] +
            inverse[1][2] * delta[2]),
        static_cast<float>(
            inverse[2][0] * delta[0] +
            inverse[2][1] * delta[1] +
            inverse[2][2] * delta[2]),
    };
    return std::isfinite(local.x) && std::isfinite(local.y) &&
        std::isfinite(local.z);
}

std::string FormatFloat(float value) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(3) << value;
    return output.str();
}

bool ParseFloat(std::string_view text, float& value) {
    try {
        std::size_t consumed = 0;
        const float parsed = std::stof(std::string(text), &consumed);
        if (consumed != text.size() || !std::isfinite(parsed)) {
            return false;
        }
        value = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

class EditorSplineRouteControlPointTool final
    : public IEditorInteractiveTool {
public:
    explicit EditorSplineRouteControlPointTool(
        EditorSplineRouteToolServices services)
        : services_(std::move(services)) {}

    bool Activate(
        const EditorInteractiveToolEnvironment& environment,
        std::string& outError) override {
        if (services_.mutations == nullptr ||
            services_.world == nullptr ||
            services_.scene == nullptr ||
            services_.selection == nullptr ||
            environment.selection == nullptr ||
            environment.selection->Count() != 1) {
            outError =
                "Spline Control Points requires exactly one selected Scene Entity.";
            return false;
        }
        target_ = *environment.selection->Primary();
        entity_ = services_.scene->ResolveEntity(target_);
        EditorScene* scene = services_.scene->BoundScene();
        if (entity_ == nullptr || scene == nullptr ||
            !services_.scene->Document().IsValid() ||
            environment.activeDocumentKey !=
                services_.scene->Document().Key()) {
            outError =
                "Spline Control Points requires the active Scene document.";
            return false;
        }
        const EditorSceneComponent* component =
            scene->FindComponent(
                *entity_, kEditorSplineRouteComponentType);
        if (component == nullptr || !component->enabled ||
            !EditorSplineRouteComponent::FromSceneComponent(
                *component, route_, &outError)) {
            if (outError.empty()) {
                outError =
                    "Selected Entity has no enabled Spline Route Component.";
            }
            return false;
        }
        std::unordered_map<std::string, AffineTransform> cache;
        std::unordered_set<std::string> resolving;
        if (!WorldTransform(
                *scene, *entity_, cache, resolving,
                worldTransform_, outError)) {
            return false;
        }
        coordinates_ = environment.coordinates;
        RebuildPreview();
        return true;
    }

    void Tick(
        const EditorInteractiveToolEnvironment& environment,
        const EditorInteractiveToolFrameInput& input) override {
        coordinates_ = environment.coordinates;
        if (coordinates_ == nullptr) return;
        hovered_ = PickControlPoint(input.mouseX, input.mouseY);

        if (operation_ == SplineEditOperation::Move) {
            TickMove(input);
        } else if (
            operation_ == SplineEditOperation::Add &&
            input.viewportPrimaryPressed) {
            AddPoint(input.mouseX, input.mouseY);
        } else if (
            operation_ == SplineEditOperation::Delete &&
            input.viewportPrimaryPressed) {
            DeletePoint(hovered_);
        }
    }

    EditorInteractiveToolAcceptResult BuildAccept(
        const EditorInteractiveToolEnvironment&) override {
        if (!changed_) {
            return EditorInteractiveToolAcceptResult::Failure(
                "Spline edit has no control point change to commit.");
        }
        std::string error;
        if (!route_.Validate(&error)) {
            return EditorInteractiveToolAcceptResult::Failure(
                "Spline edit is invalid: " + error);
        }
        EditorWorldMutationRequest request{};
        request.kind =
            EditorWorldMutationKind::SetComponentProperty;
        request.targets = {target_};
        request.componentType =
            std::string(kEditorSplineRouteComponentType);
        request.property = "controlPoints";
        request.propertyValue =
            SerializeEditorSplineRouteControlPoints(
                route_.controlPoints);
        prepared_ = {};
        if (!services_.mutations->Prepare(
                request, true, prepared_, &error)) {
            return EditorInteractiveToolAcceptResult::Failure(
                error.empty()
                ? "Spline control point mutation planning failed."
                : error);
        }
        return EditorInteractiveToolAcceptResult::Commit(
            EditorInteractiveToolCommit{
                prepared_.label,
                prepared_.transactionTarget,
                prepared_.command},
            "Spline control points committed.");
    }

    void Cancel(EditorInteractiveToolEndReason) override {
        dragging_ = false;
        acceptRequested_ = false;
        changed_ = false;
        prepared_ = {};
    }

    void OnAccepted() override {
        const EditorWorldMutationResult result =
            services_.mutations->ResolveCommitted(prepared_);
        if (result.succeeded && services_.onCommitted) {
            services_.onCommitted(result);
        }
        dragging_ = false;
        acceptRequested_ = false;
    }

    bool WantsAccept() const override { return acceptRequested_; }

    bool SetProperty(
        std::string_view name,
        std::string_view value,
        std::string& outError) override {
        if (name == "Operation") {
            if (value == "MOVE") {
                operation_ = SplineEditOperation::Move;
            } else if (value == "ADD") {
                operation_ = SplineEditOperation::Add;
            } else if (value == "DELETE") {
                operation_ = SplineEditOperation::Delete;
            } else {
                outError = "Operation must be MOVE, ADD, or DELETE.";
                return false;
            }
            return true;
        }
        if (name == "Edit Plane") {
            if (value == "XZ") settings_.plane = EditorPlacementPlane::XZ;
            else if (value == "XY") settings_.plane = EditorPlacementPlane::XY;
            else if (value == "YZ") settings_.plane = EditorPlacementPlane::YZ;
            else {
                outError = "Edit Plane must be XZ, XY, or YZ.";
                return false;
            }
            return true;
        }
        if (name == "Grid Snap") {
            settings_.gridSnapEnabled =
                value == "true" || value == "1";
            return true;
        }
        if (name == "Grid Size") {
            float parsed = 0.0f;
            if (!ParseFloat(value, parsed) || parsed <= 0.0f) {
                outError =
                    "Grid Size must be a positive finite number.";
                return false;
            }
            settings_.gridSize =
                (std::clamp)(parsed, 0.001f, 1000.0f);
            return true;
        }
        outError = "Unknown Spline Control Point property.";
        return false;
    }

    void BuildViewportOverlay(
        EditorViewportOverlayService& overlay) const override {
        if (coordinates_ == nullptr) return;
        auto sink =
            overlay.Sink(EditorViewportOverlayLayerId::AuthoringHelpers);
        constexpr uint32_t kLineColor = 0xff4ed8ffu;
        constexpr uint32_t kPointColor = 0xfff4cf58u;
        constexpr uint32_t kHoverColor = 0xffff8a55u;
        constexpr uint32_t kSelectedColor = 0xff68ff8cu;

        for (std::size_t index = 1;
             index < polyline_.size(); ++index) {
            const auto left =
                coordinates_->ProjectWorld(polyline_[index - 1]);
            const auto right =
                coordinates_->ProjectWorld(polyline_[index]);
            if (left.valid && left.inDepth &&
                right.valid && right.inDepth) {
                sink.Line(
                    left.render.x, left.render.y,
                    right.render.x, right.render.y,
                    kLineColor, 2.0f);
            }
        }
        for (std::size_t index = 0;
             index < route_.controlPoints.size(); ++index) {
            const Vector3 world = TransformPoint(
                worldTransform_,
                route_.controlPoints[index].position);
            const auto projected =
                coordinates_->ProjectWorld(world);
            if (!projected.valid || !projected.inDepth) continue;
            const uint32_t color =
                static_cast<int>(index) == selected_
                ? kSelectedColor
                : static_cast<int>(index) == hovered_
                    ? kHoverColor : kPointColor;
            sink.CircleFilled(
                projected.render.x, projected.render.y,
                static_cast<int>(index) == selected_ ? 7.0f : 5.0f,
                color);
            sink.Label(
                projected.render.x + 8.0f,
                projected.render.y - 8.0f,
                route_.controlPoints[index].id,
                color);
        }
    }

    std::string ViewportHint() const override {
        switch (operation_) {
        case SplineEditOperation::Move:
            return "Spline: drag a control point; release commits one Undoable Transaction";
        case SplineEditOperation::Add:
            return "Spline: click the edit plane to append one stable control point";
        case SplineEditOperation::Delete:
            return "Spline: click a control point to delete it within route minimums";
        }
        return {};
    }

    std::vector<EditorInteractiveToolProperty>
    Properties() const override {
        const char* operation =
            operation_ == SplineEditOperation::Move ? "MOVE" :
            operation_ == SplineEditOperation::Add ? "ADD" :
            "DELETE";
        return {
            {"Operation", operation,
             "Choose point interaction mode.",
             EditorInteractiveToolPropertyEditKind::Choice,
             0.0f, 0.0f, {"MOVE", "ADD", "DELETE"}},
            {"Edit Plane", ToString(settings_.plane),
             "World-space drag and placement plane.",
             EditorInteractiveToolPropertyEditKind::Choice,
             0.0f, 0.0f, {"XZ", "XY", "YZ"}},
            {"Grid Snap",
             settings_.gridSnapEnabled ? "true" : "false",
             "Snap authored world positions before conversion to local space.",
             EditorInteractiveToolPropertyEditKind::Boolean},
            {"Grid Size", FormatFloat(settings_.gridSize),
             "World-space snap interval.",
             EditorInteractiveToolPropertyEditKind::Float,
             0.001f, 1000.0f},
            {"Control Points",
             std::to_string(route_.controlPoints.size()),
             "Stable-ID control point count."},
            {"Hovered",
             hovered_ >= 0
                ? route_.controlPoints[
                    static_cast<std::size_t>(hovered_)].id
                : "None",
             "Nearest control point under the cursor."},
        };
    }

private:
    int PickControlPoint(float x, float y) const {
        if (coordinates_ == nullptr) return -1;
        constexpr float kPickRadiusSquared = 14.0f * 14.0f;
        int best = -1;
        float bestSquared = kPickRadiusSquared;
        for (std::size_t index = 0;
             index < route_.controlPoints.size(); ++index) {
            const auto point = coordinates_->ProjectWorld(
                TransformPoint(
                    worldTransform_,
                    route_.controlPoints[index].position));
            if (!point.valid || !point.inDepth) continue;
            const float dx = point.display.x - x;
            const float dy = point.display.y - y;
            const float squared = dx * dx + dy * dy;
            if (squared <= bestSquared) {
                bestSquared = squared;
                best = static_cast<int>(index);
            }
        }
        return best;
    }

    void TickMove(const EditorInteractiveToolFrameInput& input) {
        if (input.viewportPrimaryPressed && hovered_ >= 0) {
            selected_ = hovered_;
            dragging_ = true;
            const Vector3 world = TransformPoint(
                worldTransform_,
                route_.controlPoints[
                    static_cast<std::size_t>(selected_)].position);
            switch (settings_.plane) {
            case EditorPlacementPlane::XZ:
                settings_.planeOffset = world.y;
                break;
            case EditorPlacementPlane::XY:
                settings_.planeOffset = world.z;
                break;
            case EditorPlacementPlane::YZ:
                settings_.planeOffset = world.x;
                break;
            }
        }
        if (dragging_ && input.viewportPrimaryDown) {
            const EditorPlacementQueryResult query =
                query_.QueryDisplay(
                    *coordinates_, input.mouseX, input.mouseY,
                    settings_);
            Vector3 local{};
            if (query.valid &&
                InverseTransformPoint(
                    worldTransform_, query.position, local)) {
                auto& position =
                    route_.controlPoints[
                        static_cast<std::size_t>(selected_)].position;
                const float delta =
                    std::abs(position.x - local.x) +
                    std::abs(position.y - local.y) +
                    std::abs(position.z - local.z);
                if (delta > 0.00001f) {
                    position = local;
                    changed_ = true;
                    RebuildPreview();
                }
            }
        }
        if (dragging_ && input.viewportPrimaryReleased) {
            dragging_ = false;
            acceptRequested_ = changed_;
        }
    }

    void AddPoint(float x, float y) {
        const EditorPlacementQueryResult query =
            query_.QueryDisplay(*coordinates_, x, y, settings_);
        Vector3 local{};
        if (!query.valid ||
            !InverseTransformPoint(
                worldTransform_, query.position, local)) {
            return;
        }
        std::unordered_set<std::string> ids;
        for (const auto& point : route_.controlPoints) {
            ids.insert(point.id);
        }
        uint64_t suffix = route_.controlPoints.size();
        std::string id;
        do {
            id = "p" + std::to_string(suffix++);
        } while (ids.contains(id));
        route_.controlPoints.push_back({std::move(id), local});
        selected_ =
            static_cast<int>(route_.controlPoints.size() - 1);
        changed_ = true;
        acceptRequested_ = true;
        RebuildPreview();
    }

    void DeletePoint(int index) {
        if (index < 0) return;
        const std::size_t minimum = route_.closedLoop ? 3u : 2u;
        if (route_.controlPoints.size() <= minimum) return;
        route_.controlPoints.erase(
            route_.controlPoints.begin() + index);
        selected_ = -1;
        hovered_ = -1;
        changed_ = true;
        acceptRequested_ = true;
        RebuildPreview();
    }

    void RebuildPreview() {
        polyline_.clear();
        EditorSplineRouteEvaluationService evaluator;
        if (!evaluator.Build(route_, nullptr)) return;
        const float spacing =
            (std::max)(0.1f, evaluator.TotalLength() / 128.0f);
        std::vector<Vector3> local =
            evaluator.SamplePolyline(spacing);
        polyline_.reserve(local.size());
        for (const Vector3& point : local) {
            polyline_.push_back(
                TransformPoint(worldTransform_, point));
        }
    }

    EditorSplineRouteToolServices services_{};
    EditorPlacementQueryService query_{};
    EditorPlacementQuerySettings settings_{};
    EditorObjectHandle target_{};
    EditorSceneEntity* entity_ = nullptr;
    EditorSplineRouteComponent route_{};
    AffineTransform worldTransform_{};
    const EditorViewportCoordinateService* coordinates_ = nullptr;
    std::vector<Vector3> polyline_;
    EditorPreparedWorldMutation prepared_{};
    SplineEditOperation operation_ = SplineEditOperation::Move;
    int hovered_ = -1;
    int selected_ = -1;
    bool dragging_ = false;
    bool changed_ = false;
    bool acceptRequested_ = false;
};

} // namespace

void RegisterSplineRouteTools(
    EditorModeRegistry& registry,
    EditorSplineRouteToolServices services) {
    registry.RegisterMode(EditorModeDescriptor{
        "editor.mode.paths",
        "Paths",
        "Author stable spline control points in the Scene viewport.",
        "Shift+6",
        175});
    EditorInteractiveToolDescriptor descriptor{};
    descriptor.id = "editor.tool.splineControlPoints";
    descriptor.modeId = "editor.mode.paths";
    descriptor.label = "Spline Control Points";
    descriptor.category = "Paths";
    descriptor.description =
        "Move, append, and delete stable spline control points with "
        "one Undo/Redo transaction per gesture.";
    descriptor.shortcut = "S";
    descriptor.sortOrder = 100;
    descriptor.requiresSelection = true;
    descriptor.requiresViewport = true;
    descriptor.requiresAuthoring = true;
    descriptor.cancelOnSelectionChange = true;
    descriptor.transactionPolicy =
        EditorInteractiveToolTransactionPolicy::SingleCommandOnAccept;
    descriptor.build = [services = std::move(services)]() mutable {
        return std::make_unique<
            EditorSplineRouteControlPointTool>(services);
    };
    registry.RegisterTool(std::move(descriptor));
}

} // namespace editor
