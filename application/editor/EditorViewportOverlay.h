#pragma once

#include "EditorViewportCoordinateService.h"
#include "EditorViewportRenderTarget.h"

#include "../../externals/imgui/imgui.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

enum class EditorViewportOverlayLayerId : uint8_t {
    GameplayHud = 0,
    AuthoringHelpers,
    SelectionOutline,
    ObjectLabels,
    CourseNavigation,
    VfxDebug,
    Performance,
    CameraSafeFrame,
    Count
};

constexpr size_t kEditorViewportOverlayLayerCount =
    static_cast<size_t>(EditorViewportOverlayLayerId::Count);

const char* EditorViewportOverlayLayerStableId(EditorViewportOverlayLayerId layer);
const char* EditorViewportOverlayLayerLabel(EditorViewportOverlayLayerId layer);
bool EditorViewportOverlayLayerIsGameplay(EditorViewportOverlayLayerId layer);

struct EditorViewportOverlayLayerSettings {
    bool visible = true;
    bool selectedOnly = false;
    bool hideDuringScreenshot = true;
    float maxDistance = 0.0f;
    float distanceFadeStart = 0.70f;
    uint32_t maxLabels = 256;
    int drawOrder = 0;
};

struct EditorViewportOverlayFrameContext {
    EditorViewportRenderTargetState viewport{};
    uint32_t fallbackWidth = 0;
    uint32_t fallbackHeight = 0;
    const EditorViewportCoordinateService* coordinates = nullptr;
    Vector3 cameraWorldPosition{};
    float zoom = 1.0f;
};

struct EditorViewportOverlayItemOptions {
    bool selected = false;
    bool iconFallback = true;
    bool background = false;
    float distance = -1.0f;
    float minZoom = 0.0f;
    float maxZoom = (std::numeric_limits<float>::max)();
    int priority = 0;
};

enum class EditorViewportOverlayCommandType : uint8_t {
    Line,
    Rect,
    RectFilled,
    Circle,
    CircleFilled,
    Label,
    Icon
};

struct EditorViewportOverlayCommand {
    EditorViewportOverlayLayerId layer = EditorViewportOverlayLayerId::AuthoringHelpers;
    EditorViewportOverlayCommandType type = EditorViewportOverlayCommandType::Line;
    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;
    float radius = 0.0f;
    float thickness = 1.0f;
    uint32_t color = 0xffffffffu;
    uint32_t backgroundColor = 0xb0000000u;
    std::string text;
    EditorViewportOverlayItemOptions options{};
    uint64_t sequence = 0;
};

struct EditorViewportOverlayFrameStats {
    uint32_t submitted = 0;
    uint32_t resolved = 0;
    uint32_t filtered = 0;
    uint32_t labelsDrawn = 0;
    uint32_t labelsRepositioned = 0;
    uint32_t labelsIconized = 0;
    uint32_t labelsSuppressed = 0;
    uint32_t commandBudgetRejected = 0;
};

class EditorViewportOverlayService;

class EditorViewportOverlayCommandSink {
public:
    EditorViewportOverlayCommandSink(
        EditorViewportOverlayService& service,
        EditorViewportOverlayLayerId layer);

    bool Line(
        float x0, float y0, float x1, float y1, uint32_t color, float thickness = 1.0f,
        const EditorViewportOverlayItemOptions& options = {});
    bool Rect(
        float x0, float y0, float x1, float y1, uint32_t color, float thickness = 1.0f,
        const EditorViewportOverlayItemOptions& options = {});
    bool RectFilled(
        float x0, float y0, float x1, float y1, uint32_t color,
        const EditorViewportOverlayItemOptions& options = {});
    bool Circle(
        float x, float y, float radius, uint32_t color, float thickness = 1.0f,
        const EditorViewportOverlayItemOptions& options = {});
    bool CircleFilled(
        float x, float y, float radius, uint32_t color,
        const EditorViewportOverlayItemOptions& options = {});
    bool Label(
        float x, float y, std::string text, uint32_t color,
        const EditorViewportOverlayItemOptions& options = {});
    bool Icon(
        float x, float y, float radius, uint32_t color,
        const EditorViewportOverlayItemOptions& options = {});

private:
    EditorViewportOverlayService* service_ = nullptr;
    EditorViewportOverlayLayerId layer_ = EditorViewportOverlayLayerId::AuthoringHelpers;
};

class IEditorViewportOverlayProvider {
public:
    virtual ~IEditorViewportOverlayProvider() = default;
    virtual std::string_view Id() const = 0;
    virtual EditorViewportOverlayLayerId Layer() const = 0;
    virtual void Build(
        const EditorViewportOverlayFrameContext& context,
        EditorViewportOverlayCommandSink& sink) const = 0;
};

class EditorViewportOverlayService {
public:
    EditorViewportOverlayService();

    bool RegisterProvider(const IEditorViewportOverlayProvider& provider);
    bool UnregisterProvider(std::string_view id);
    size_t ProviderCount() const { return providers_.size(); }

    void BeginFrame(const EditorViewportOverlayFrameContext& context);
    void Resolve();
    void Render(ImDrawList* drawList = nullptr);

    EditorViewportOverlayCommandSink Sink(EditorViewportOverlayLayerId layer) {
        return EditorViewportOverlayCommandSink(*this, layer);
    }

    bool Submit(EditorViewportOverlayCommand command);
    bool SubmitWorldLabel(
        EditorViewportOverlayLayerId layer,
        const Vector3& world,
        std::string text,
        uint32_t color,
        const EditorViewportOverlayItemOptions& options = {});

    const EditorViewportOverlayFrameContext& FrameContext() const { return frameContext_; }
    const std::vector<EditorViewportOverlayCommand>& ResolvedCommands() const { return resolved_; }
    const EditorViewportOverlayFrameStats& Stats() const { return stats_; }

    const EditorViewportOverlayLayerSettings& LayerSettings(EditorViewportOverlayLayerId layer) const;
    void SetLayerSettings(
        EditorViewportOverlayLayerId layer,
        const EditorViewportOverlayLayerSettings& settings);
    void SetLayerVisible(EditorViewportOverlayLayerId layer, bool visible);
    bool LayerVisible(EditorViewportOverlayLayerId layer) const;

    void SetGameplayVisible(bool visible) { gameplayVisible_ = visible; resolvedDirty_ = true; }
    bool GameplayVisible() const { return gameplayVisible_; }
    void SetEditorVisible(bool visible) { editorVisible_ = visible; resolvedDirty_ = true; }
    bool EditorVisible() const { return editorVisible_; }
    void SetScreenshotSuppression(bool active) { screenshotSuppression_ = active; resolvedDirty_ = true; }
    bool ScreenshotSuppression() const { return screenshotSuppression_; }

    void SetCommandBudget(uint32_t budget) { commandBudget_ = budget > 0 ? budget : 1; }
    uint32_t CommandBudget() const { return commandBudget_; }
    uint32_t Revision() const { return revision_; }

private:
    friend class EditorViewportOverlayCommandSink;

    static size_t LayerIndex(EditorViewportOverlayLayerId layer);
    bool CommandVisible(const EditorViewportOverlayCommand& command, float& outAlpha) const;
    bool SubmitPrimitive(
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
        const EditorViewportOverlayItemOptions& options);

    EditorViewportOverlayFrameContext frameContext_{};
    std::array<EditorViewportOverlayLayerSettings, kEditorViewportOverlayLayerCount> layerSettings_{};
    std::vector<const IEditorViewportOverlayProvider*> providers_;
    std::vector<EditorViewportOverlayCommand> submitted_;
    std::vector<EditorViewportOverlayCommand> resolved_;
    std::array<uint32_t, kEditorViewportOverlayLayerCount> labelCandidateCounts_{};
    EditorViewportOverlayFrameStats stats_{};
    bool gameplayVisible_ = true;
    bool editorVisible_ = true;
    bool screenshotSuppression_ = false;
    bool resolvedDirty_ = true;
    uint32_t commandBudget_ = 8192;
    uint32_t preResolveLabelsSuppressed_ = 0;
    uint32_t revision_ = 0;
    uint64_t nextSequence_ = 1;
};

class EditorViewportOverlayScreenshotScope {
public:
    explicit EditorViewportOverlayScreenshotScope(EditorViewportOverlayService& service)
        : service_(&service), previous_(service.ScreenshotSuppression()) {
        service_->SetScreenshotSuppression(true);
    }
    ~EditorViewportOverlayScreenshotScope() {
        if (service_ != nullptr) service_->SetScreenshotSuppression(previous_);
    }

    EditorViewportOverlayScreenshotScope(const EditorViewportOverlayScreenshotScope&) = delete;
    EditorViewportOverlayScreenshotScope& operator=(const EditorViewportOverlayScreenshotScope&) = delete;

private:
    EditorViewportOverlayService* service_ = nullptr;
    bool previous_ = false;
};

class EditorViewportOverlayScope {
public:
    EditorViewportOverlayScope(
        const EditorViewportRenderTargetState& viewport,
        uint32_t fallbackWidth,
        uint32_t fallbackHeight,
        ImDrawList* drawList = nullptr);
    ~EditorViewportOverlayScope();

    EditorViewportOverlayScope(const EditorViewportOverlayScope&) = delete;
    EditorViewportOverlayScope& operator=(const EditorViewportOverlayScope&) = delete;

    bool Active() const { return drawList_ != nullptr && renderWidth_ > 0.0f && renderHeight_ > 0.0f; }
    ImDrawList* DrawList() const { return drawList_; }

    float RenderWidth() const { return renderWidth_; }
    float RenderHeight() const { return renderHeight_; }
    ImVec2 DisplayMin() const { return displayMin_; }
    ImVec2 DisplayMax() const { return displayMax_; }
    ImVec2 DisplayCenter() const;

    ImVec2 ToDisplay(float renderX, float renderY) const;
    ImVec2 ToDisplay(const ImVec2& renderPoint) const;
    float ScaleX(float value) const;
    float ScaleY(float value) const;
    float ScaleRadius(float value) const;
    bool RenderPointVisible(float renderX, float renderY, float margin = 0.0f) const;

private:
    ImDrawList* drawList_ = nullptr;
    EditorViewportCoordinateService coordinates_{};
    ImVec2 displayMin_{};
    ImVec2 displayMax_{};
    float renderWidth_ = 0.0f;
    float renderHeight_ = 0.0f;
    float scaleX_ = 1.0f;
    float scaleY_ = 1.0f;
    bool clipPushed_ = false;
};

} // namespace editor
