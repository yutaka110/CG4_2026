#include "AppSceneStateManager.h"

void AppSceneStateManager::Initialize(
    std::unique_ptr<IAppSceneState> initialState,
    AppSceneHost& host) {
    currentState_ = std::move(initialState);
    if (currentState_ != nullptr) {
        currentState_->OnEnter(host);
    }
}

void AppSceneStateManager::ChangeScene(std::unique_ptr<IAppSceneState> nextState) {
    pendingState_ = std::move(nextState);
}

void AppSceneStateManager::Update(AppSceneHost& host) {
    ApplyPendingScene(host);
    if (currentState_ != nullptr) {
        currentState_->Update(host);
    }
}

void AppSceneStateManager::Render(AppSceneHost& host) {
    ApplyPendingScene(host);
    if (currentState_ != nullptr) {
        currentState_->Render(host);
    }
}

void AppSceneStateManager::Shutdown(AppSceneHost& host) {
    if (currentState_ != nullptr) {
        currentState_->OnExit(host);
        currentState_.reset();
    }
    pendingState_.reset();
}

void AppSceneStateManager::ApplyPendingScene(AppSceneHost& host) {
    if (pendingState_ == nullptr) {
        return;
    }

    if (currentState_ != nullptr) {
        currentState_->OnExit(host);
    }
    currentState_ = std::move(pendingState_);
    currentState_->OnEnter(host);
}
