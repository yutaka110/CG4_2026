#include "RailShooterDefensePromptRenderer.h"

#include <algorithm>
#include <string>

void RailShooterDefensePromptRenderer::Reset() {
    frame_ = {};
    revision_ = 0;
}

void RailShooterDefensePromptRenderer::Update(
    const RailShooterDefensePromptRenderInput& input) {
    RailShooterDefensePromptRenderFrame next{};
    next.revision = ++revision_;
    const bool hasPrompt = input.presentation != nullptr &&
        !input.presentation->cues.empty();
    const bool hasOutcome = input.outcome != nullptr && input.outcome->visible;
    if (!input.settings.enabled || (!hasPrompt && !hasOutcome) ||
        input.viewportWidth < 320 ||
        input.viewportHeight < 180) {
        frame_ = std::move(next);
        return;
    }
    next.sourceRevision = hasOutcome
        ? input.outcome->revision : input.presentation->revision;
    const float width = static_cast<float>(input.viewportWidth);
    const float height = static_cast<float>(input.viewportHeight);
    const float scale = (std::clamp)(
        (std::min)(width / 1600.0f, height / 900.0f), 0.72f, 1.35f);
    const uint32_t budget = (std::max)(4u,
        input.settings.maximumDrawCommands);
    next.commands.reserve((std::min)(budget, 32u));
    const auto push = [&](RailShooterHudDrawCommand command) {
        if (next.commands.size() < budget) {
            next.commands.push_back(std::move(command));
        } else {
            ++next.droppedCommands;
        }
    };
    const auto rect = [&](float x, float y, float w, float h, Vector4 color) {
        RailShooterHudDrawCommand command{};
        command.x = x; command.y = y; command.width = w; command.height = h;
        command.color = color;
        push(std::move(command));
    };
    const auto text = [&](std::string value, float x, float y,
                          float fontScale, Vector4 color) {
        RailShooterHudDrawCommand command{};
        command.kind = RailShooterHudDrawCommandKind::Text;
        command.textAlignment = RailShooterHudTextAlignment::Center;
        command.x = x; command.y = y; command.fontScale = fontScale;
        command.color = color; command.text = std::move(value);
        push(std::move(command));
    };

    if (hasOutcome) {
        const float alpha = input.settings.opacity * input.outcome->alpha;
        const float panelWidth = 350.0f * scale;
        const float panelHeight = 68.0f * scale;
        const float panelX = width * 0.5f - panelWidth * 0.5f;
        const float panelY = height * 0.66f;
        Vector4 color = input.outcome->color;
        color.w = alpha;
        rect(panelX, panelY, panelWidth, panelHeight,
             Vector4{0.008f, 0.018f, 0.028f, alpha * 0.90f});
        rect(panelX, panelY, panelWidth, 5.0f * scale, color);
        text(input.outcome->headline, width * 0.5f,
             panelY + 33.0f * scale, 0.88f * scale, color);
        text(input.outcome->detail, width * 0.5f,
             panelY + 56.0f * scale, 0.48f * scale,
             Vector4{0.88f, 0.95f, 0.96f, alpha});
        next.visiblePrompts = 1;
        next.visible = !next.commands.empty();
        frame_ = std::move(next);
        return;
    }

    const EnemyAttackDefensePresentationCue& primary =
        input.presentation->cues.front();
    const float pulse = 0.72f + 0.28f *
        (std::clamp)(primary.pulse, 0.0f, 1.0f);
    const float alpha = input.settings.opacity * pulse;
    const float panelWidth = 310.0f * scale;
    const float panelHeight = 62.0f * scale;
    const float panelX = width * 0.5f - panelWidth * 0.5f;
    const float panelY = height * 0.68f;
    const Vector4 actionColor = primary.actionSatisfied
        ? Vector4{0.30f, 1.0f, 0.55f, alpha}
        : Vector4{primary.color.x, primary.color.y, primary.color.z, alpha};
    rect(panelX, panelY, panelWidth, panelHeight,
         Vector4{0.008f, 0.018f, 0.028f, alpha * 0.88f});
    rect(panelX, panelY, panelWidth, 4.0f * scale, actionColor);
    const char* direction = primary.directionFromCenter.x < -0.12f
        ? "  <<" : (primary.directionFromCenter.x > 0.12f ? ">>  " : "");
    std::string headline;
    if (primary.directionFromCenter.x > 0.12f) headline += direction;
    headline += primary.actionSatisfied ? "DEFENSE OK" :
        ToString(primary.primaryAction);
    if (primary.directionFromCenter.x < -0.12f) headline += direction;
    text(headline, width * 0.5f, panelY + 31.0f * scale,
         0.88f * scale, actionColor);

    std::string detail = primary.projectileInFlight
        ? "PROJECTILE INBOUND"
        : (primary.primaryAction == EnemyAttackDefensePromptAction::Interrupt
            ? "BREAK THE ATTACK BEFORE LAUNCH"
            : "RESPOND BEFORE IMPACT");
    if (input.presentation->cues.size() > 1) {
        detail += "  +" + std::to_string(input.presentation->cues.size() - 1) +
            " THREAT";
    }
    text(detail, width * 0.5f, panelY + 51.0f * scale,
         0.47f * scale, Vector4{0.82f, 0.91f, 0.95f, alpha * 0.90f});
    next.visiblePrompts = static_cast<uint32_t>(
        input.presentation->cues.size());
    next.visible = !next.commands.empty();
    frame_ = std::move(next);
}
