#include "EditorViewportOverlay.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace editor {
namespace {

struct OverlayRect {
    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;
};

constexpr std::array<EditorViewportOverlayLayerId, kEditorViewportOverlayLayerCount> kLayers{
    EditorViewportOverlayLayerId::GameplayHud,
    EditorViewportOverlayLayerId::AuthoringHelpers,
    EditorViewportOverlayLayerId::SelectionOutline,
    EditorViewportOverlayLayerId::ObjectLabels,
    EditorViewportOverlayLayerId::CourseNavigation,
    EditorViewportOverlayLayerId::VfxDebug,
    EditorViewportOverlayLayerId::Performance,
    EditorViewportOverlayLayerId::CameraSafeFrame};

bool Intersects(const OverlayRect& a, const OverlayRect& b) {
    return a.x0 < b.x1 && a.x1 > b.x0 && a.y0 < b.y1 && a.y1 > b.y0;
}

OverlayRect LabelRect(const EditorViewportOverlayCommand& command) {
    constexpr float kCharacterWidth = 7.25f;
    constexpr float kLineHeight = 17.0f;
    constexpr float kPaddingX = 5.0f;
    constexpr float kPaddingY = 3.0f;
    size_t maxLineLength = 0;
    size_t lineLength = 0;
    size_t lineCount = 1;
    for (const char character : command.text) {
        if (character == '\n') {
            maxLineLength = (std::max)(maxLineLength, lineLength);
            lineLength = 0;
            ++lineCount;
        } else {
            ++lineLength;
        }
    }
    maxLineLength = (std::max)(maxLineLength, lineLength);
    const float width = static_cast<float>(maxLineLength) * kCharacterWidth + kPaddingX * 2.0f;
    const float height = static_cast<float>(lineCount) * kLineHeight + kPaddingY * 2.0f;
    return OverlayRect{command.x0, command.y0, command.x0 + width, command.y0 + height};
}

bool InsideViewport(const OverlayRect& rect, float width, float height) {
    return rect.x0 >= 0.0f && rect.y0 >= 0.0f && rect.x1 <= width && rect.y1 <= height;
}

uint32_t ApplyAlpha(uint32_t color, float alpha) {
    const uint32_t sourceAlpha = (color >> 24u) & 0xffu;
    const uint32_t resultAlpha = static_cast<uint32_t>(
        std::lround(static_cast<float>(sourceAlpha) * (std::clamp)(alpha, 0.0f, 1.0f)));
    return (color & 0x00ffffffu) | ((std::min)(resultAlpha, 255u) << 24u);
}

} // namespace

const char* EditorViewportOverlayLayerStableId(EditorViewportOverlayLayerId layer) {
    switch (layer) {
    case EditorViewportOverlayLayerId::GameplayHud: return "gameplay-hud";
    case EditorViewportOverlayLayerId::AuthoringHelpers: return "authoring-helpers";
    case EditorViewportOverlayLayerId::SelectionOutline: return "selection-outline";
    case EditorViewportOverlayLayerId::ObjectLabels: return "object-labels";
    case EditorViewportOverlayLayerId::CourseNavigation: return "course-navigation";
    case EditorViewportOverlayLayerId::VfxDebug: return "vfx-debug";
    case EditorViewportOverlayLayerId::Performance: return "performance";
    case EditorViewportOverlayLayerId::CameraSafeFrame: return "camera-safe-frame";
    case EditorViewportOverlayLayerId::Count: break;
    }
    return "unknown";
}

const char* EditorViewportOverlayLayerLabel(EditorViewportOverlayLayerId layer) {
    switch (layer) {
    case EditorViewportOverlayLayerId::GameplayHud: return "Gameplay HUD";
    case EditorViewportOverlayLayerId::AuthoringHelpers: return "Authoring Helpers";
    case EditorViewportOverlayLayerId::SelectionOutline: return "Selection Outline";
    case EditorViewportOverlayLayerId::ObjectLabels: return "Object Labels";
    case EditorViewportOverlayLayerId::CourseNavigation: return "Course / Navigation";
    case EditorViewportOverlayLayerId::VfxDebug: return "VFX Debug";
    case EditorViewportOverlayLayerId::Performance: return "Performance";
    case EditorViewportOverlayLayerId::CameraSafeFrame: return "Camera Safe Frame";
    case EditorViewportOverlayLayerId::Count: break;
    }
    return "Unknown";
}

bool EditorViewportOverlayLayerIsGameplay(EditorViewportOverlayLayerId layer) {
    return layer == EditorViewportOverlayLayerId::GameplayHud;
}

EditorViewportOverlayCommandSink::EditorViewportOverlayCommandSink(
    EditorViewportOverlayService& service,
    EditorViewportOverlayLayerId layer)
    : service_(&service), layer_(layer) {}

bool EditorViewportOverlayCommandSink::Line(
    float x0, float y0, float x1, float y1, uint32_t color, float thickness,
    const EditorViewportOverlayItemOptions& options) {
    return service_ != nullptr && service_->SubmitPrimitive(
        layer_, EditorViewportOverlayCommandType::Line, x0, y0, x1, y1, 0.0f,
        color, thickness, {}, options);
}

bool EditorViewportOverlayCommandSink::Rect(
    float x0, float y0, float x1, float y1, uint32_t color, float thickness,
    const EditorViewportOverlayItemOptions& options) {
    return service_ != nullptr && service_->SubmitPrimitive(
        layer_, EditorViewportOverlayCommandType::Rect, x0, y0, x1, y1, 0.0f,
        color, thickness, {}, options);
}

bool EditorViewportOverlayCommandSink::RectFilled(
    float x0, float y0, float x1, float y1, uint32_t color,
    const EditorViewportOverlayItemOptions& options) {
    return service_ != nullptr && service_->SubmitPrimitive(
        layer_, EditorViewportOverlayCommandType::RectFilled, x0, y0, x1, y1, 0.0f,
        color, 1.0f, {}, options);
}

bool EditorViewportOverlayCommandSink::Circle(
    float x, float y, float radius, uint32_t color, float thickness,
    const EditorViewportOverlayItemOptions& options) {
    return service_ != nullptr && service_->SubmitPrimitive(
        layer_, EditorViewportOverlayCommandType::Circle, x, y, 0.0f, 0.0f, radius,
        color, thickness, {}, options);
}

bool EditorViewportOverlayCommandSink::CircleFilled(
    float x, float y, float radius, uint32_t color,
    const EditorViewportOverlayItemOptions& options) {
    return service_ != nullptr && service_->SubmitPrimitive(
        layer_, EditorViewportOverlayCommandType::CircleFilled, x, y, 0.0f, 0.0f, radius,
        color, 1.0f, {}, options);
}

bool EditorViewportOverlayCommandSink::Label(
    float x, float y, std::string text, uint32_t color,
    const EditorViewportOverlayItemOptions& options) {
    return service_ != nullptr && service_->SubmitPrimitive(
        layer_, EditorViewportOverlayCommandType::Label, x, y, 0.0f, 0.0f, 0.0f,
        color, 1.0f, std::move(text), options);
}

bool EditorViewportOverlayCommandSink::Icon(
    float x, float y, float radius, uint32_t color,
    const EditorViewportOverlayItemOptions& options) {
    return service_ != nullptr && service_->SubmitPrimitive(
        layer_, EditorViewportOverlayCommandType::Icon, x, y, 0.0f, 0.0f, radius,
        color, 1.0f, {}, options);
}

EditorViewportOverlayService::EditorViewportOverlayService() {
    for (size_t index = 0; index < kLayers.size(); ++index) {
        EditorViewportOverlayLayerSettings settings{};
        settings.drawOrder = static_cast<int>(index) * 100;
        settings.hideDuringScreenshot = !EditorViewportOverlayLayerIsGameplay(kLayers[index]);
        layerSettings_[index] = settings;
    }
    layerSettings_[LayerIndex(EditorViewportOverlayLayerId::ObjectLabels)].maxDistance = 2500.0f;
    layerSettings_[LayerIndex(EditorViewportOverlayLayerId::ObjectLabels)].maxLabels = 128;
    layerSettings_[LayerIndex(EditorViewportOverlayLayerId::VfxDebug)].maxLabels = 128;
    layerSettings_[LayerIndex(EditorViewportOverlayLayerId::VfxDebug)].visible = false;
    layerSettings_[LayerIndex(EditorViewportOverlayLayerId::Performance)].visible = false;
    layerSettings_[LayerIndex(EditorViewportOverlayLayerId::CameraSafeFrame)].visible = false;
}

bool EditorViewportOverlayService::RegisterProvider(const IEditorViewportOverlayProvider& provider) {
    if (provider.Id().empty() || provider.Layer() == EditorViewportOverlayLayerId::Count) {
        return false;
    }
    const auto duplicate = std::find_if(
        providers_.begin(), providers_.end(),
        [&provider](const IEditorViewportOverlayProvider* current) {
            return current == &provider || (current != nullptr && current->Id() == provider.Id());
        });
    if (duplicate != providers_.end()) {
        return false;
    }
    providers_.push_back(&provider);
    ++revision_;
    return true;
}

bool EditorViewportOverlayService::UnregisterProvider(std::string_view id) {
    const auto found = std::find_if(
        providers_.begin(), providers_.end(),
        [id](const IEditorViewportOverlayProvider* provider) {
            return provider != nullptr && provider->Id() == id;
        });
    if (found == providers_.end()) {
        return false;
    }
    providers_.erase(found);
    ++revision_;
    return true;
}

void EditorViewportOverlayService::BeginFrame(const EditorViewportOverlayFrameContext& context) {
    frameContext_ = context;
    submitted_.clear();
    resolved_.clear();
    labelCandidateCounts_.fill(0);
    stats_ = EditorViewportOverlayFrameStats{};
    preResolveLabelsSuppressed_ = 0;
    resolvedDirty_ = true;
    nextSequence_ = 1;

    for (const IEditorViewportOverlayProvider* provider : providers_) {
        if (provider == nullptr) continue;
        EditorViewportOverlayCommandSink sink(*this, provider->Layer());
        provider->Build(frameContext_, sink);
    }
}

void EditorViewportOverlayService::Resolve() {
    if (!resolvedDirty_) return;
    resolved_.clear();
    stats_.resolved = 0;
    stats_.filtered = 0;
    stats_.labelsDrawn = 0;
    stats_.labelsRepositioned = 0;
    stats_.labelsIconized = 0;
    stats_.labelsSuppressed = preResolveLabelsSuppressed_;

    std::vector<EditorViewportOverlayCommand> labels;
    for (EditorViewportOverlayCommand command : submitted_) {
        float alpha = 1.0f;
        if (!CommandVisible(command, alpha)) {
            ++stats_.filtered;
            continue;
        }
        command.color = ApplyAlpha(command.color, alpha);
        command.backgroundColor = ApplyAlpha(command.backgroundColor, alpha);
        if (command.type == EditorViewportOverlayCommandType::Label) {
            labels.push_back(std::move(command));
        } else {
            resolved_.push_back(std::move(command));
        }
    }

    std::stable_sort(
        labels.begin(), labels.end(),
        [this](const EditorViewportOverlayCommand& a, const EditorViewportOverlayCommand& b) {
            if (a.options.selected != b.options.selected) return a.options.selected;
            if (a.options.priority != b.options.priority) return a.options.priority > b.options.priority;
            const int aOrder = LayerSettings(a.layer).drawOrder;
            const int bOrder = LayerSettings(b.layer).drawOrder;
            return aOrder != bOrder ? aOrder < bOrder : a.sequence < b.sequence;
        });

    std::array<uint32_t, kEditorViewportOverlayLayerCount> labelCounts{};
    std::vector<OverlayRect> occupied;
    const float viewportWidth = static_cast<float>((std::max)(1u, frameContext_.viewport.renderWidth));
    const float viewportHeight = static_cast<float>((std::max)(1u, frameContext_.viewport.renderHeight));
    for (EditorViewportOverlayCommand command : labels) {
        const size_t layerIndex = LayerIndex(command.layer);
        const uint32_t maxLabels = LayerSettings(command.layer).maxLabels;
        if (!command.options.selected && labelCounts[layerIndex] >= maxLabels) {
            ++stats_.labelsSuppressed;
            continue;
        }

        if (frameContext_.zoom < command.options.minZoom && command.options.iconFallback) {
            command.type = EditorViewportOverlayCommandType::Icon;
            command.radius = command.options.selected ? 6.0f : 4.0f;
            resolved_.push_back(std::move(command));
            ++labelCounts[layerIndex];
            ++stats_.labelsIconized;
            continue;
        }

        const float originalX = command.x0;
        const float originalY = command.y0;
        const OverlayRect baseRect = LabelRect(command);
        const float horizontalStep = (baseRect.x1 - baseRect.x0) + 6.0f;
        const float verticalStep = (baseRect.y1 - baseRect.y0) + 4.0f;
        const std::array<std::array<float, 2>, 9> offsets{{
            {{0.0f, 0.0f}}, {{0.0f, -verticalStep}}, {{0.0f, verticalStep}},
            {{horizontalStep, 0.0f}}, {{-horizontalStep, 0.0f}},
            {{horizontalStep, -verticalStep}}, {{horizontalStep, verticalStep}},
            {{-horizontalStep, -verticalStep}}, {{-horizontalStep, verticalStep}}}};
        bool placed = false;
        OverlayRect placedRect{};
        for (const auto& offset : offsets) {
            command.x0 = originalX + offset[0];
            command.y0 = originalY + offset[1];
            const OverlayRect candidate = LabelRect(command);
            const bool overlap = std::any_of(
                occupied.begin(), occupied.end(),
                [&candidate](const OverlayRect& current) { return Intersects(candidate, current); });
            if (!overlap && InsideViewport(candidate, viewportWidth, viewportHeight)) {
                placed = true;
                placedRect = candidate;
                if (offset[0] != 0.0f || offset[1] != 0.0f) ++stats_.labelsRepositioned;
                break;
            }
        }

        if (!placed && command.options.selected) {
            command.x0 = (std::clamp)(originalX, 0.0f, (std::max)(0.0f, viewportWidth - 160.0f));
            command.y0 = (std::clamp)(originalY, 0.0f, (std::max)(0.0f, viewportHeight - 24.0f));
            placedRect = LabelRect(command);
            placed = true;
        }

        if (placed) {
            occupied.push_back(placedRect);
            resolved_.push_back(std::move(command));
            ++labelCounts[layerIndex];
            ++stats_.labelsDrawn;
        } else if (command.options.iconFallback) {
            command.type = EditorViewportOverlayCommandType::Icon;
            command.x0 = originalX;
            command.y0 = originalY;
            command.radius = 4.0f;
            resolved_.push_back(std::move(command));
            ++labelCounts[layerIndex];
            ++stats_.labelsIconized;
        } else {
            ++stats_.labelsSuppressed;
        }
    }

    std::stable_sort(
        resolved_.begin(), resolved_.end(),
        [this](const EditorViewportOverlayCommand& a, const EditorViewportOverlayCommand& b) {
            const int aOrder = LayerSettings(a.layer).drawOrder;
            const int bOrder = LayerSettings(b.layer).drawOrder;
            return aOrder != bOrder ? aOrder < bOrder : a.sequence < b.sequence;
        });
    stats_.resolved = static_cast<uint32_t>(resolved_.size());
    resolvedDirty_ = false;
}

void EditorViewportOverlayService::Render(ImDrawList* drawList) {
    Resolve();
    EditorViewportOverlayScope scope(
        frameContext_.viewport,
        frameContext_.fallbackWidth,
        frameContext_.fallbackHeight,
        drawList);
    if (!scope.Active()) return;

    ImDrawList* output = scope.DrawList();
    for (const EditorViewportOverlayCommand& command : resolved_) {
        const ImVec2 a = scope.ToDisplay(command.x0, command.y0);
        const ImVec2 b = scope.ToDisplay(command.x1, command.y1);
        const float thickness = (std::max)(0.5f, scope.ScaleRadius(command.thickness));
        switch (command.type) {
        case EditorViewportOverlayCommandType::Line:
            output->AddLine(a, b, command.color, thickness);
            break;
        case EditorViewportOverlayCommandType::Rect:
            output->AddRect(a, b, command.color, 0.0f, 0, thickness);
            break;
        case EditorViewportOverlayCommandType::RectFilled:
            output->AddRectFilled(a, b, command.color);
            break;
        case EditorViewportOverlayCommandType::Circle:
            output->AddCircle(a, scope.ScaleRadius(command.radius), command.color, 24, thickness);
            break;
        case EditorViewportOverlayCommandType::CircleFilled:
            output->AddCircleFilled(a, scope.ScaleRadius(command.radius), command.color, 24);
            break;
        case EditorViewportOverlayCommandType::Label: {
            if (command.options.background) {
                const ImVec2 textSize = ImGui::CalcTextSize(command.text.c_str());
                output->AddRectFilled(
                    ImVec2(a.x - 4.0f, a.y - 3.0f),
                    ImVec2(a.x + textSize.x + 4.0f, a.y + textSize.y + 3.0f),
                    command.backgroundColor,
                    3.0f);
            }
            output->AddText(a, command.color, command.text.c_str());
            break;
        }
        case EditorViewportOverlayCommandType::Icon: {
            const float radius = scope.ScaleRadius((std::max)(3.0f, command.radius));
            const ImVec2 points[4]{
                ImVec2(a.x, a.y - radius), ImVec2(a.x + radius, a.y),
                ImVec2(a.x, a.y + radius), ImVec2(a.x - radius, a.y)};
            output->AddConvexPolyFilled(points, 4, command.color);
            break;
        }
        }
    }
}

bool EditorViewportOverlayService::Submit(EditorViewportOverlayCommand command) {
    if (command.layer == EditorViewportOverlayLayerId::Count) return false;
    ++stats_.submitted;
    if (command.type == EditorViewportOverlayCommandType::Label && !command.options.selected) {
        const size_t layerIndex = LayerIndex(command.layer);
        const uint32_t maxCandidates =
            (std::max)(LayerSettings(command.layer).maxLabels, 1u) * 4u;
        if (labelCandidateCounts_[layerIndex] >= maxCandidates) {
            ++preResolveLabelsSuppressed_;
            return true;
        }
        ++labelCandidateCounts_[layerIndex];
    }
    if (submitted_.size() >= commandBudget_) {
        ++stats_.commandBudgetRejected;
        return false;
    }
    command.sequence = nextSequence_++;
    submitted_.push_back(std::move(command));
    resolvedDirty_ = true;
    return true;
}

bool EditorViewportOverlayService::SubmitWorldLabel(
    EditorViewportOverlayLayerId layer,
    const Vector3& world,
    std::string text,
    uint32_t color,
    const EditorViewportOverlayItemOptions& options) {
    if (frameContext_.coordinates == nullptr) return false;
    const EditorViewportProjectedPoint projected = frameContext_.coordinates->ProjectWorld(world);
    if (!projected.valid || projected.behind || !projected.inDepth || !projected.render.valid) return false;
    EditorViewportOverlayItemOptions resolvedOptions = options;
    if (resolvedOptions.distance < 0.0f) {
        const Vector3 delta{
            world.x - frameContext_.cameraWorldPosition.x,
            world.y - frameContext_.cameraWorldPosition.y,
            world.z - frameContext_.cameraWorldPosition.z};
        resolvedOptions.distance = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
    }
    return SubmitPrimitive(
        layer,
        EditorViewportOverlayCommandType::Label,
        projected.render.x,
        projected.render.y,
        0.0f,
        0.0f,
        0.0f,
        color,
        1.0f,
        std::move(text),
        resolvedOptions);
}

const EditorViewportOverlayLayerSettings& EditorViewportOverlayService::LayerSettings(
    EditorViewportOverlayLayerId layer) const {
    return layerSettings_[LayerIndex(layer)];
}

void EditorViewportOverlayService::SetLayerSettings(
    EditorViewportOverlayLayerId layer,
    const EditorViewportOverlayLayerSettings& settings) {
    if (layer == EditorViewportOverlayLayerId::Count) return;
    EditorViewportOverlayLayerSettings sanitized = settings;
    sanitized.maxDistance = (std::max)(0.0f, sanitized.maxDistance);
    sanitized.distanceFadeStart = (std::clamp)(sanitized.distanceFadeStart, 0.0f, 1.0f);
    sanitized.maxLabels = (std::max)(1u, sanitized.maxLabels);
    EditorViewportOverlayLayerSettings& current = layerSettings_[LayerIndex(layer)];
    if (current.visible == sanitized.visible &&
        current.selectedOnly == sanitized.selectedOnly &&
        current.hideDuringScreenshot == sanitized.hideDuringScreenshot &&
        current.maxDistance == sanitized.maxDistance &&
        current.distanceFadeStart == sanitized.distanceFadeStart &&
        current.maxLabels == sanitized.maxLabels &&
        current.drawOrder == sanitized.drawOrder) {
        return;
    }
    current = sanitized;
    ++revision_;
    resolvedDirty_ = true;
}

void EditorViewportOverlayService::SetLayerVisible(
    EditorViewportOverlayLayerId layer,
    bool visible) {
    if (layer == EditorViewportOverlayLayerId::Count) return;
    EditorViewportOverlayLayerSettings settings = LayerSettings(layer);
    if (settings.visible == visible) return;
    settings.visible = visible;
    SetLayerSettings(layer, settings);
}

bool EditorViewportOverlayService::LayerVisible(EditorViewportOverlayLayerId layer) const {
    if (layer == EditorViewportOverlayLayerId::Count) return false;
    const EditorViewportOverlayLayerSettings& settings = LayerSettings(layer);
    if (!settings.visible) return false;
    return EditorViewportOverlayLayerIsGameplay(layer) ? gameplayVisible_ : editorVisible_;
}

size_t EditorViewportOverlayService::LayerIndex(EditorViewportOverlayLayerId layer) {
    const size_t index = static_cast<size_t>(layer);
    return index < kEditorViewportOverlayLayerCount ? index : 0;
}

bool EditorViewportOverlayService::CommandVisible(
    const EditorViewportOverlayCommand& command,
    float& outAlpha) const {
    outAlpha = 1.0f;
    if (!LayerVisible(command.layer)) return false;
    const EditorViewportOverlayLayerSettings& settings = LayerSettings(command.layer);
    if (screenshotSuppression_ && settings.hideDuringScreenshot) return false;
    if (settings.selectedOnly && !command.options.selected) return false;
    if (frameContext_.zoom > command.options.maxZoom) return false;
    if (frameContext_.zoom < command.options.minZoom && !command.options.iconFallback) return false;
    if (settings.maxDistance > 0.0f && command.options.distance >= 0.0f) {
        if (command.options.distance >= settings.maxDistance) return false;
        const float fadeStart = settings.maxDistance * settings.distanceFadeStart;
        if (command.options.distance > fadeStart && fadeStart < settings.maxDistance) {
            outAlpha = 1.0f -
                (command.options.distance - fadeStart) / (settings.maxDistance - fadeStart);
        }
    }
    return true;
}

bool EditorViewportOverlayService::SubmitPrimitive(
    EditorViewportOverlayLayerId layer,
    EditorViewportOverlayCommandType type,
    float x0,
    float y0,
    float x1,
    float y1,
    float radius,
    uint32_t color,
    float thickness,
    std::string text,
    const EditorViewportOverlayItemOptions& options) {
    EditorViewportOverlayCommand command{};
    command.layer = layer;
    command.type = type;
    command.x0 = x0;
    command.y0 = y0;
    command.x1 = x1;
    command.y1 = y1;
    command.radius = radius;
    command.thickness = thickness;
    command.color = color;
    command.text = std::move(text);
    command.options = options;
    return Submit(std::move(command));
}

EditorViewportOverlayScope::EditorViewportOverlayScope(
    const EditorViewportRenderTargetState& viewport,
    uint32_t fallbackWidth,
    uint32_t fallbackHeight,
    ImDrawList* drawList) {
    drawList_ = drawList != nullptr ? drawList : ImGui::GetForegroundDrawList();
    if (drawList_ == nullptr) return;

    const EditorPanelRect displayRect =
        viewport.enabled && viewport.Valid() && viewport.displayRect.Valid()
            ? viewport.displayRect
            : EditorPanelRect{
                  0.0f, 0.0f,
                  static_cast<float>((std::max)(1u, fallbackWidth)),
                  static_cast<float>((std::max)(1u, fallbackHeight))};
    const uint32_t renderWidth = viewport.Valid()
        ? (std::max)(1u, viewport.renderWidth)
        : (std::max)(1u, fallbackWidth);
    const uint32_t renderHeight = viewport.Valid()
        ? (std::max)(1u, viewport.renderHeight)
        : (std::max)(1u, fallbackHeight);

    coordinates_.Update(EditorViewportCoordinateContext{displayRect, renderWidth, renderHeight});
    displayMin_ = ImVec2(displayRect.x, displayRect.y);
    displayMax_ = ImVec2(displayRect.x + displayRect.width, displayRect.y + displayRect.height);
    renderWidth_ = static_cast<float>(renderWidth);
    renderHeight_ = static_cast<float>(renderHeight);
    scaleX_ = coordinates_.ScaleRenderToDisplayX(1.0f);
    scaleY_ = coordinates_.ScaleRenderToDisplayY(1.0f);
    drawList_->PushClipRect(displayMin_, displayMax_, true);
    clipPushed_ = true;
}

EditorViewportOverlayScope::~EditorViewportOverlayScope() {
    if (drawList_ != nullptr && clipPushed_) drawList_->PopClipRect();
}

ImVec2 EditorViewportOverlayScope::DisplayCenter() const {
    return ImVec2((displayMin_.x + displayMax_.x) * 0.5f, (displayMin_.y + displayMax_.y) * 0.5f);
}

ImVec2 EditorViewportOverlayScope::ToDisplay(float renderX, float renderY) const {
    const EditorViewportCoordinatePoint point = coordinates_.RenderToDisplay(renderX, renderY);
    return ImVec2(point.x, point.y);
}

ImVec2 EditorViewportOverlayScope::ToDisplay(const ImVec2& renderPoint) const {
    return ToDisplay(renderPoint.x, renderPoint.y);
}

float EditorViewportOverlayScope::ScaleX(float value) const {
    return coordinates_.ScaleRenderToDisplayX(value);
}

float EditorViewportOverlayScope::ScaleY(float value) const {
    return coordinates_.ScaleRenderToDisplayY(value);
}

float EditorViewportOverlayScope::ScaleRadius(float value) const {
    return coordinates_.ScaleRenderToDisplayRadius(value);
}

bool EditorViewportOverlayScope::RenderPointVisible(float renderX, float renderY, float margin) const {
    return coordinates_.RenderPointVisible(renderX, renderY, margin);
}

} // namespace editor
