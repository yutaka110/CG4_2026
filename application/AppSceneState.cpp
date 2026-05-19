#include "AppSceneState.h"

void IAppSceneState::OnEnter(AppSceneHost&) {}

void IAppSceneState::OnExit(AppSceneHost&) {}

const char* VfxPreviewSceneState::Name() const {
    return "VfxPreview";
}

void VfxPreviewSceneState::Update(AppSceneHost& host) {
    host.UpdateVfxPreviewFrame();
}

void VfxPreviewSceneState::Render(AppSceneHost& host) {
    host.RenderVfxPreviewFrame();
}
