#pragma once

class AppSceneHost {
public:
    virtual ~AppSceneHost() = default;

    virtual void EnterRailShooterScene() = 0;
    virtual void UpdateRailShooterFrame() = 0;
    virtual void RenderRailShooterFrame() = 0;
    virtual void UpdateVfxPreviewFrame() = 0;
    virtual void RenderVfxPreviewFrame() = 0;
};

class IAppSceneState {
public:
    virtual ~IAppSceneState() = default;

    virtual const char* Name() const = 0;
    virtual void OnEnter(AppSceneHost& host);
    virtual void OnExit(AppSceneHost& host);
    virtual void Update(AppSceneHost& host) = 0;
    virtual void Render(AppSceneHost& host) = 0;
};

class VfxPreviewSceneState final : public IAppSceneState {
public:
    const char* Name() const override;
    void Update(AppSceneHost& host) override;
    void Render(AppSceneHost& host) override;
};

class RailShooterSceneState final : public IAppSceneState {
public:
    const char* Name() const override;
    void OnEnter(AppSceneHost& host) override;
    void Update(AppSceneHost& host) override;
    void Render(AppSceneHost& host) override;
};
