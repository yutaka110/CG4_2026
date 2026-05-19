#pragma once

#include <memory>

#include "AppSceneState.h"

class AppSceneStateManager {
public:
    void Initialize(std::unique_ptr<IAppSceneState> initialState, AppSceneHost& host);
    void ChangeScene(std::unique_ptr<IAppSceneState> nextState);
    void Update(AppSceneHost& host);
    void Render(AppSceneHost& host);
    void Shutdown(AppSceneHost& host);

    const IAppSceneState* CurrentState() const { return currentState_.get(); }

private:
    void ApplyPendingScene(AppSceneHost& host);

    std::unique_ptr<IAppSceneState> currentState_;
    std::unique_ptr<IAppSceneState> pendingState_;
};
