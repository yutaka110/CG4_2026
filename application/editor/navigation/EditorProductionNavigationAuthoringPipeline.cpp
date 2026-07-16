#include "EditorProductionNavigationAuthoringPipeline.h"

#include "../EditorTransactionStack.h"
#include "../documents/EditorDocumentManager.h"

#include <algorithm>
#include <memory>

namespace editor {
namespace {
void SetError(std::string* output, std::string message) {
    if (output != nullptr) *output = std::move(message);
}

class NavigationAuthoringUndoCommand final : public IEditorUndoCommand {
public:
    NavigationAuthoringUndoCommand(EditorDocumentId document,
        EditorNavigationAuthoringAsset before, EditorNavigationAuthoringAsset after)
        : document_(std::move(document)), before_(std::move(before)), after_(std::move(after)) {}

    EditorUndoResult Apply(EditorTransactionApplyMode mode,
        EditorExecutionContext& context) const override {
        auto* service = dynamic_cast<EditorProductionNavigationAuthoringPipeline*>(
            context.Find(EditorProductionNavigationAuthoringPipeline::kServiceId));
        if (service == nullptr)
            return EditorUndoResult::Failure(EditorErrorCode::MissingService,
                "Navigation authoring execution service is unavailable.");
        std::string error;
        if (!service->PublishFromCommand(document_,
                mode == EditorTransactionApplyMode::Undo ? before_ : after_, error))
            return EditorUndoResult::Failure(EditorErrorCode::ApplyFailed, std::move(error));
        return EditorUndoResult::Success();
    }

    std::size_t EstimatedBytes() const noexcept override {
        return sizeof(*this) +
            (before_.areas.size() + after_.areas.size()) * sizeof(EditorNavigationAreaDefinition) +
            (before_.agentProfiles.size() + after_.agentProfiles.size()) * sizeof(EditorNavigationAgentProfile) +
            (before_.offMeshLinks.size() + after_.offMeshLinks.size()) * sizeof(EditorNavigationOffMeshLink);
    }
    std::string_view DomainId() const noexcept override { return "navigation-authoring"; }
    std::string_view TypeId() const noexcept override {
        return "navigation-authoring.asset-snapshot";
    }

private:
    EditorDocumentId document_;
    EditorNavigationAuthoringAsset before_;
    EditorNavigationAuthoringAsset after_;
};

template <typename T>
auto FindById(std::vector<T>& values, std::string_view id) {
    return std::find_if(values.begin(), values.end(),
        [&](const T& value) { return value.id == id; });
}
} // namespace

bool EditorProductionNavigationAuthoringPipeline::Initialize(
    EditorNavigationAuthoringPolicy policy, std::string* errorMessage) {
    Shutdown();
    policy.maximumOverlayCommands = (std::max)(1u, policy.maximumOverlayCommands);
    policy_ = policy;
    initialized_ = true;
    SetError(errorMessage, {});
    return true;
}

void EditorProductionNavigationAuthoringPipeline::Shutdown() {
    initialized_ = false;
    activeDocument_ = {};
    compileResult_ = {};
    stats_ = {};
    provider_ = nullptr;
    transactions_ = nullptr;
    documents_ = nullptr;
    runtime_ = nullptr;
    mutationCallback_ = {};
}

void EditorProductionNavigationAuthoringPipeline::Bind(
    EditorNavigationDocumentProvider* provider, EditorTransactionStack* transactions,
    EditorDocumentManager* documents, EditorProductionNavigationPipeline* runtime) {
    provider_ = provider;
    transactions_ = transactions;
    documents_ = documents;
    runtime_ = runtime;
}

void EditorProductionNavigationAuthoringPipeline::SetActiveDocument(EditorDocumentId document) {
    activeDocument_ = std::move(document);
    std::string ignored;
    RefreshAndPublishRuntime(&ignored);
}

EditorNavigationAuthoringAsset* EditorProductionNavigationAuthoringPipeline::ActiveAsset() {
    return provider_ != nullptr && activeDocument_.type == EditorDocumentTypes::NavigationData
        ? provider_->Asset(activeDocument_) : nullptr;
}
const EditorNavigationAuthoringAsset* EditorProductionNavigationAuthoringPipeline::ActiveAsset() const {
    return const_cast<EditorProductionNavigationAuthoringPipeline*>(this)->ActiveAsset();
}

bool EditorProductionNavigationAuthoringPipeline::AddArea(
    EditorNavigationAreaDefinition area, std::string& errorMessage) {
    auto* asset = ActiveAsset();
    if (asset == nullptr || area.id.empty() || asset->areas.size() >= kEditorNavigationMaximumAreas ||
        FindById(asset->areas, area.id) != asset->areas.end()) {
        errorMessage = "Navigation area identity, capacity, or active document is invalid.";
        return false;
    }
    auto before = *asset, after = before;
    after.areas.push_back(std::move(area));
    return Commit("Add Navigation Area", std::move(before), std::move(after), errorMessage);
}

bool EditorProductionNavigationAuthoringPipeline::UpdateArea(
    EditorNavigationAreaDefinition area, std::string& errorMessage) {
    auto* asset = ActiveAsset();
    if (asset == nullptr || area.id.empty()) { errorMessage = "Navigation area is unavailable."; return false; }
    auto before = *asset, after = before;
    auto found = FindById(after.areas, area.id);
    if (found == after.areas.end()) { errorMessage = "Navigation area was not found."; return false; }
    *found = std::move(area);
    return Commit("Edit Navigation Area", std::move(before), std::move(after), errorMessage);
}

bool EditorProductionNavigationAuthoringPipeline::RemoveArea(
    std::string_view id, std::string& errorMessage) {
    auto* asset = ActiveAsset();
    if (asset == nullptr || id == "Default") { errorMessage = "Default navigation area cannot be removed."; return false; }
    auto before = *asset, after = before;
    const auto found = FindById(after.areas, id);
    if (found == after.areas.end()) { errorMessage = "Navigation area was not found."; return false; }
    if (std::any_of(after.offMeshLinks.begin(), after.offMeshLinks.end(),
            [&](const auto& link) { return link.areaId == id; })) {
        errorMessage = "Navigation area is referenced by an Off-Mesh Link.";
        return false;
    }
    after.areas.erase(found);
    return Commit("Remove Navigation Area", std::move(before), std::move(after), errorMessage);
}

bool EditorProductionNavigationAuthoringPipeline::AddAgentProfile(
    EditorNavigationAgentProfile profile, std::string& errorMessage) {
    auto* asset = ActiveAsset();
    if (asset == nullptr || profile.id.empty() ||
        asset->agentProfiles.size() >= kEditorNavigationMaximumAgentProfiles ||
        FindById(asset->agentProfiles, profile.id) != asset->agentProfiles.end()) {
        errorMessage = "Agent profile identity, capacity, or active document is invalid.";
        return false;
    }
    auto before = *asset, after = before;
    after.agentProfiles.push_back(std::move(profile));
    return Commit("Add Navigation Agent Profile", std::move(before), std::move(after), errorMessage);
}

bool EditorProductionNavigationAuthoringPipeline::UpdateAgentProfile(
    EditorNavigationAgentProfile profile, std::string& errorMessage) {
    auto* asset = ActiveAsset();
    if (asset == nullptr || profile.id.empty()) { errorMessage = "Agent profile is unavailable."; return false; }
    auto before = *asset, after = before;
    auto found = FindById(after.agentProfiles, profile.id);
    if (found == after.agentProfiles.end()) { errorMessage = "Agent profile was not found."; return false; }
    *found = std::move(profile);
    return Commit("Edit Navigation Agent Profile", std::move(before), std::move(after), errorMessage);
}

bool EditorProductionNavigationAuthoringPipeline::RemoveAgentProfile(
    std::string_view id, std::string& errorMessage) {
    auto* asset = ActiveAsset();
    if (asset == nullptr || id == "Default") { errorMessage = "Default agent profile cannot be removed."; return false; }
    auto before = *asset, after = before;
    const auto found = FindById(after.agentProfiles, id);
    if (found == after.agentProfiles.end()) { errorMessage = "Agent profile was not found."; return false; }
    if (std::any_of(after.offMeshLinks.begin(), after.offMeshLinks.end(),
            [&](const auto& link) { return link.agentProfileId == id; })) {
        errorMessage = "Agent profile is referenced by an Off-Mesh Link.";
        return false;
    }
    after.agentProfiles.erase(found);
    return Commit("Remove Navigation Agent Profile", std::move(before), std::move(after), errorMessage);
}

bool EditorProductionNavigationAuthoringPipeline::AddOffMeshLink(
    EditorNavigationOffMeshLink link, std::string& errorMessage) {
    auto* asset = ActiveAsset();
    if (asset == nullptr || link.id.empty() ||
        asset->offMeshLinks.size() >= kEditorNavigationMaximumOffMeshLinks ||
        FindById(asset->offMeshLinks, link.id) != asset->offMeshLinks.end()) {
        errorMessage = "Off-Mesh Link identity, capacity, or active document is invalid.";
        return false;
    }
    auto before = *asset, after = before;
    after.offMeshLinks.push_back(std::move(link));
    return Commit("Add Off-Mesh Link", std::move(before), std::move(after), errorMessage);
}

bool EditorProductionNavigationAuthoringPipeline::UpdateOffMeshLink(
    EditorNavigationOffMeshLink link, std::string& errorMessage) {
    auto* asset = ActiveAsset();
    if (asset == nullptr || link.id.empty()) { errorMessage = "Off-Mesh Link is unavailable."; return false; }
    auto before = *asset, after = before;
    auto found = FindById(after.offMeshLinks, link.id);
    if (found == after.offMeshLinks.end()) { errorMessage = "Off-Mesh Link was not found."; return false; }
    *found = std::move(link);
    return Commit("Edit Off-Mesh Link", std::move(before), std::move(after), errorMessage);
}

bool EditorProductionNavigationAuthoringPipeline::RemoveOffMeshLink(
    std::string_view id, std::string& errorMessage) {
    auto* asset = ActiveAsset();
    if (asset == nullptr) { errorMessage = "Navigation Data document is unavailable."; return false; }
    auto before = *asset, after = before;
    const auto found = FindById(after.offMeshLinks, id);
    if (found == after.offMeshLinks.end()) { errorMessage = "Off-Mesh Link was not found."; return false; }
    after.offMeshLinks.erase(found);
    return Commit("Remove Off-Mesh Link", std::move(before), std::move(after), errorMessage);
}

bool EditorProductionNavigationAuthoringPipeline::Commit(std::string_view label,
    EditorNavigationAuthoringAsset before, EditorNavigationAuthoringAsset after,
    std::string& errorMessage) {
    const auto compiled = CompileEditorNavigationAuthoring(after);
    if (!compiled.succeeded) {
        ++stats_.compileFailures;
        errorMessage = compiled.diagnostics.empty() ? "Navigation Data compile failed."
            : compiled.diagnostics.front().message;
        return false;
    }
    if (provider_ == nullptr || transactions_ == nullptr || !activeDocument_.IsValid() ||
        !provider_->Publish(activeDocument_, after)) {
        errorMessage = "Navigation authoring mutation services are unavailable.";
        return false;
    }
    if (runtime_ != nullptr && !runtime_->ApplyAuthoringProgram(compiled.program, &errorMessage)) {
        provider_->Publish(activeDocument_, before);
        return false;
    }
    EditorObjectHandle target{EditorDomainId::NavigationAuthoring, activeDocument_.assetGuid,
        0, 0, after.name};
    EditorError error;
    if (!transactions_->PushCommand(std::string(label), std::move(target),
            std::make_shared<NavigationAuthoringUndoCommand>(activeDocument_, before, after), &error)) {
        provider_->Publish(activeDocument_, before);
        const auto rollback = CompileEditorNavigationAuthoring(before);
        if (runtime_ != nullptr && rollback.succeeded)
            runtime_->ApplyAuthoringProgram(rollback.program, nullptr);
        errorMessage = error.message;
        return false;
    }
    compileResult_ = compiled;
    ++stats_.mutations;
    ++stats_.runtimePublishes;
    if (documents_ != nullptr) documents_->MarkDirty(activeDocument_, label);
    if (mutationCallback_) mutationCallback_(activeDocument_, label);
    return true;
}

bool EditorProductionNavigationAuthoringPipeline::PublishFromCommand(
    const EditorDocumentId& document, const EditorNavigationAuthoringAsset& asset,
    std::string& errorMessage) {
    const auto compiled = CompileEditorNavigationAuthoring(asset);
    if (!compiled.succeeded) {
        errorMessage = compiled.diagnostics.empty() ? "Navigation Data transaction compile failed."
            : compiled.diagnostics.front().message;
        return false;
    }
    const EditorNavigationAuthoringAsset* previousAsset =
        provider_ != nullptr ? provider_->Asset(document) : nullptr;
    const EditorNavigationAuthoringAsset previous =
        previousAsset != nullptr ? *previousAsset : EditorNavigationAuthoringAsset{};
    if (provider_ == nullptr || previousAsset == nullptr || !provider_->Publish(document, asset)) {
        errorMessage = "Navigation Data transaction snapshot could not be published.";
        return false;
    }
    if (document == activeDocument_) {
        compileResult_ = compiled;
        if (runtime_ != nullptr && !runtime_->ApplyAuthoringProgram(compiled.program, &errorMessage)) {
            provider_->Publish(document, previous);
            return false;
        }
        ++stats_.runtimePublishes;
    }
    if (documents_ != nullptr) documents_->MarkDirty(document, "Navigation Authoring Undo/Redo");
    if (mutationCallback_) mutationCallback_(document, "Navigation Authoring Undo/Redo");
    return true;
}

bool EditorProductionNavigationAuthoringPipeline::RefreshAndPublishRuntime(std::string* errorMessage) {
    compileResult_ = {};
    const auto* asset = ActiveAsset();
    if (asset == nullptr) { SetError(errorMessage, {}); return true; }
    compileResult_ = CompileEditorNavigationAuthoring(*asset);
    if (!compileResult_.succeeded) {
        ++stats_.compileFailures;
        SetError(errorMessage, compileResult_.diagnostics.empty() ? "Navigation Data compile failed."
            : compileResult_.diagnostics.front().message);
        return false;
    }
    if (runtime_ != nullptr && !runtime_->ApplyAuthoringProgram(compileResult_.program, errorMessage))
        return false;
    ++stats_.runtimePublishes;
    return true;
}

void EditorProductionNavigationAuthoringPipeline::Build(
    const EditorViewportOverlayFrameContext& context,
    EditorViewportOverlayCommandSink& sink) const {
    if (!initialized_ || runtime_ == nullptr || context.coordinates == nullptr) return;
    const auto snapshot = runtime_->Snapshot();
    if (!snapshot) return;
    uint32_t submitted = 0;
    const auto permit = [&]() {
        if (submitted >= policy_.maximumOverlayCommands) {
            ++stats_.overlayBudgetRejected;
            return false;
        }
        ++submitted;
        return true;
    };
    EditorViewportOverlayItemOptions options{};
    options.priority = 170;
    for (const auto& tile : snapshot->tiles) {
        for (const auto& polygon : tile.polygons) {
            uint32_t color = 0x9fffbd55u;
            const auto area = std::find_if(snapshot->areas.begin(), snapshot->areas.end(),
                [&](const auto& value) { return value.id == polygon.areaId; });
            if (area != snapshot->areas.end()) {
                const auto byte = [](float value) { return static_cast<uint32_t>(
                    (std::clamp)(value, 0.0f, 1.0f) * 255.0f); };
                color = 0x9f000000u | byte(area->debugColor.x) |
                    (byte(area->debugColor.y) << 8u) | (byte(area->debugColor.z) << 16u);
            }
            for (std::size_t index = 0; index < polygon.vertices.size(); ++index) {
                const auto a = context.coordinates->ProjectWorld(polygon.vertices[index]);
                const auto b = context.coordinates->ProjectWorld(
                    polygon.vertices[(index + 1) % polygon.vertices.size()]);
                if (a.valid && b.valid && !a.behind && !b.behind && permit())
                    sink.Line(a.render.x, a.render.y, b.render.x, b.render.y, color, 1.0f, options);
            }
        }
    }
    options.priority = 190;
    options.background = true;
    for (const auto& link : snapshot->offMeshLinks) {
        const auto a = context.coordinates->ProjectWorld(link.start);
        const auto b = context.coordinates->ProjectWorld(link.end);
        if (!a.valid || !b.valid || a.behind || b.behind) continue;
        if (permit()) sink.Line(a.render.x, a.render.y, b.render.x, b.render.y,
            link.bidirectional ? 0xff70e5ffu : 0xffffc65cu, 3.0f, options);
        if (permit()) sink.Circle(a.render.x, a.render.y, 5.0f, 0xffffffffu, 2.0f, options);
        if (permit()) sink.Circle(b.render.x, b.render.y, 5.0f, 0xffffffffu, 2.0f, options);
        if (permit()) sink.Label((a.render.x + b.render.x) * 0.5f,
            (a.render.y + b.render.y) * 0.5f, link.id, 0xffffffffu, options);
    }
    stats_.overlayCommands = submitted;
}

} // namespace editor
