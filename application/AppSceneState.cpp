#include "AppSceneState.h"

void IAppSceneState::OnEnter(AppSceneHost&) {}

void IAppSceneState::OnExit(AppSceneHost&) {}

const char* VfxPreviewSceneState::Name() const {
    return "VfxPreview";
}

void VfxPreviewSceneState::OnEnter(AppSceneHost& host) {
    host.EnterVfxPreviewScene();
}

void VfxPreviewSceneState::Update(AppSceneHost& host) {
    host.UpdateVfxPreviewFrame();
}

void VfxPreviewSceneState::Render(AppSceneHost& host) {
    host.RenderVfxPreviewFrame();
}

const char* MultiMaterialShowcaseSceneState::Name() const {
    return "MultiMaterialShowcase";
}

void MultiMaterialShowcaseSceneState::OnEnter(AppSceneHost& host) {
    host.EnterMultiMaterialShowcaseScene();
}

void MultiMaterialShowcaseSceneState::Update(AppSceneHost& host) {
    host.UpdateMultiMaterialShowcaseFrame();
}

void MultiMaterialShowcaseSceneState::Render(AppSceneHost& host) {
    host.RenderVfxPreviewFrame();
}

const char* RailShooterSceneState::Name() const {
    return "RailShooter";
}

void RailShooterSceneState::OnEnter(AppSceneHost& host) {
    host.EnterRailShooterScene();
}

void RailShooterSceneState::Update(AppSceneHost& host) {
    host.UpdateRailShooterFrame();
}

void RailShooterSceneState::Render(AppSceneHost& host) {
    host.RenderRailShooterFrame();
}
