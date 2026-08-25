#include "RailShooterHudRenderer.h"

#include <algorithm>

namespace {
Vector4 WithOpacity(Vector4 color, float opacity) {
    color.w *= (std::clamp)(opacity, 0.0f, 1.0f);
    return color;
}
} // namespace

void RailShooterHudRenderer::Reset() {
    frame_ = {};
    revision_ = 0;
}

void RailShooterHudRenderer::Update(const RailShooterHudRenderInput& input) {
    RailShooterHudRenderFrame next{};
    next.viewportWidth = input.viewportWidth;
    next.viewportHeight = input.viewportHeight;
    next.revision = ++revision_;
    if (input.definition == nullptr || input.presentation == nullptr ||
        !input.definition->enabled || !input.presentation->visible ||
        input.viewportWidth < 320 || input.viewportHeight < 180) {
        frame_ = std::move(next);
        return;
    }

    const RailShooterHudDefinitionAsset& definition = *input.definition;
    const RailShooterHudPresentationFrame& hud = *input.presentation;
    const float width = static_cast<float>(input.viewportWidth);
    const float height = static_cast<float>(input.viewportHeight);
    const float responsive = (std::clamp)(
        (std::min)(width / 1600.0f, height / 900.0f), 0.72f, 1.35f);
    const float scale = responsive * definition.scale;
    const float safe = definition.safeAreaPixels * responsive;
    const float opacity = definition.opacity;
    const uint32_t budget = definition.maximumDrawCommands;
    next.commands.reserve((std::min)(budget, 96u));

    const auto push = [&next, budget](RailShooterHudDrawCommand command) {
        if (next.commands.size() < budget) {
            next.commands.push_back(std::move(command));
        }
    };
    const auto rect = [&push](float x, float y, float w, float h, Vector4 color) {
        if (w <= 0.0f || h <= 0.0f || color.w <= 0.0f) return;
        RailShooterHudDrawCommand command{};
        command.x = x; command.y = y; command.width = w; command.height = h;
        command.color = color;
        push(std::move(command));
    };
    const auto text = [&push](
        std::string value,
        float x,
        float y,
        float fontScale,
        Vector4 color,
        RailShooterHudTextAlignment alignment = RailShooterHudTextAlignment::Left) {
        if (value.empty() || color.w <= 0.0f) return;
        RailShooterHudDrawCommand command{};
        command.kind = RailShooterHudDrawCommandKind::Text;
        command.textAlignment = alignment;
        command.x = x; command.y = y; command.fontScale = fontScale;
        command.color = color; command.text = std::move(value);
        push(std::move(command));
    };
    const auto bar = [&rect](
        float x, float y, float w, float h, float value, Vector4 color,
        Vector4 background) {
        rect(x, y, w, h, background);
        rect(x, y, w * (std::clamp)(value, 0.0f, 1.0f), h, color);
    };

    const Vector4 panel = WithOpacity(definition.panelColor, opacity);
    const Vector4 textColor = WithOpacity(definition.textColor, opacity);
    const Vector4 muted = WithOpacity(definition.mutedColor, opacity);
    const Vector4 primary = WithOpacity(definition.primaryColor, opacity);
    const Vector4 healthy = WithOpacity(definition.healthyColor, opacity);
    const Vector4 warning = WithOpacity(definition.warningColor, opacity);
    const float criticalOpacity = opacity * (0.65f + 0.35f * hud.warningPulse);
    const Vector4 critical = WithOpacity(definition.criticalColor, criticalOpacity);
    const Vector4 barBackground{0.04f, 0.075f, 0.09f, opacity * 0.92f};
    const float barHeight = definition.barHeight * scale;

    if (definition.showPlayerHealth || definition.showVehicleIntegrity) {
        const float x = safe;
        const float y = safe;
        const float panelWidth = definition.leftPanelWidth * scale;
        const float panelHeight = (definition.showPlayerHealth &&
            definition.showVehicleIntegrity ? 112.0f : 65.0f) * scale;
        rect(x, y, panelWidth, panelHeight, panel);
        rect(x, y, panelWidth, 3.0f * scale, primary);
        float rowY = y + 24.0f * scale;
        if (definition.showPlayerHealth) {
            text(hud.healthText, x + 11.0f * scale, rowY,
                 0.66f * scale, textColor);
            bar(x + 11.0f * scale, rowY + 9.0f * scale,
                panelWidth - 22.0f * scale, barHeight,
                hud.playerHealthNormalized,
                hud.playerHealthCritical ? critical : healthy,
                barBackground);
            rowY += 45.0f * scale;
        }
        if (definition.showVehicleIntegrity) {
            text(hud.vehicleText, x + 11.0f * scale, rowY,
                 0.62f * scale, textColor);
            bar(x + 11.0f * scale, rowY + 9.0f * scale,
                panelWidth - 22.0f * scale, barHeight,
                hud.vehicleIntegrityNormalized,
                hud.vehicleIntegrityCritical ? critical : warning,
                barBackground);
        }
        text("RETRY " + std::to_string(hud.retriesRemaining),
             x + panelWidth - 82.0f * scale,
             y + 18.0f * scale, 0.48f * scale, muted);
    }

    if (definition.showWaveObjective) {
        const float panelWidth = definition.topCenterWidth * scale;
        const float x = width * 0.5f - panelWidth * 0.5f;
        const float y = safe;
        rect(x, y, panelWidth, 62.0f * scale, panel);
        text(hud.waveText, width * 0.5f, y + 22.0f * scale,
             0.66f * scale, primary, RailShooterHudTextAlignment::Center);
        bar(x + 12.0f * scale, y + 30.0f * scale,
            panelWidth - 24.0f * scale, 7.0f * scale,
            hud.courseProgressNormalized, primary, barBackground);
        text(hud.enemyText, width * 0.5f, y + 52.0f * scale,
             0.52f * scale, hud.activeEnemies > 0 ? warning : healthy,
             RailShooterHudTextAlignment::Center);
    }

    if (definition.showScore) {
        const float panelWidth = definition.rightPanelWidth * scale;
        const float x = width - safe - panelWidth;
        const float y = safe;
        rect(x, y, panelWidth, 86.0f * scale, panel);
        rect(x, y, panelWidth, 3.0f * scale, primary);
        text(hud.scoreText, x + 12.0f * scale, y + 28.0f * scale,
             0.72f * scale, textColor);
        text(hud.comboText, x + 12.0f * scale, y + 53.0f * scale,
             0.60f * scale, warning);
        text(hud.grazeText, x + 12.0f * scale, y + 76.0f * scale,
             0.48f * scale, hud.grazeChain > 0 ? primary : muted);
    }

    if (definition.showSpeed) {
        const float panelWidth = 190.0f * scale;
        const float x = safe;
        const float y = height - safe - 61.0f * scale;
        rect(x, y, panelWidth, 61.0f * scale, panel);
        text("SPEED", x + 11.0f * scale, y + 20.0f * scale,
             0.48f * scale, muted);
        text(hud.speedText, x + 11.0f * scale, y + 47.0f * scale,
             0.82f * scale, primary);
        bar(x + 11.0f * scale, y + 52.0f * scale,
            panelWidth - 22.0f * scale, 4.0f * scale,
            hud.speedNormalized, primary, barBackground);
    }

    if (definition.showWeapon) {
        const float panelWidth = 235.0f * scale;
        const float x = width - safe - panelWidth;
        const float y = height - safe - 70.0f * scale;
        rect(x, y, panelWidth, 70.0f * scale, panel);
        text(hud.weaponText, x + 12.0f * scale, y + 27.0f * scale,
             0.65f * scale, textColor);
        const Vector4 weaponStatusColor = hud.primaryWeapon.overheated
            ? critical
            : (hud.primaryWeapon.reloading ? warning : healthy);
        text(hud.weaponStatusText, x + 12.0f * scale, y + 52.0f * scale,
             0.52f * scale, weaponStatusColor);
        bar(x + 12.0f * scale, y + 59.0f * scale,
            panelWidth - 24.0f * scale, 5.0f * scale,
            hud.primaryWeapon.heatNormalized,
            hud.primaryWeapon.overheated ? critical : warning,
            barBackground);
    }

    if (definition.showThreat && hud.threatWarning) {
        const float threatWidth = 320.0f * scale;
        const float x = width * 0.5f - threatWidth * 0.5f;
        const float y = safe + 77.0f * scale;
        rect(x, y, threatWidth, 38.0f * scale,
             Vector4{definition.panelColor.x, definition.panelColor.y,
                     definition.panelColor.z, opacity * 0.74f});
        text(hud.threatText, width * 0.5f, y + 25.0f * scale,
             0.72f * scale, critical, RailShooterHudTextAlignment::Center);
    }

    if (definition.showSessionBanner && hud.showBanner &&
        hud.bannerAlpha > 0.0f) {
        const float bannerWidth = (std::min)(620.0f * scale, width - safe * 2.0f);
        const float bannerHeight = hud.bannerDetail.empty()
            ? 82.0f * scale : 112.0f * scale;
        const float x = width * 0.5f - bannerWidth * 0.5f;
        const float y = height * 0.28f - bannerHeight * 0.5f;
        const float alpha = opacity * hud.bannerAlpha;
        rect(x, y, bannerWidth, bannerHeight,
             Vector4{0.008f, 0.015f, 0.022f, alpha * 0.88f});
        rect(x, y, bannerWidth, 4.0f * scale,
             Vector4{hud.bannerColor.x, hud.bannerColor.y,
                     hud.bannerColor.z, alpha});
        text(hud.bannerHeadline, width * 0.5f, y + 49.0f * scale,
             1.24f * scale,
             Vector4{hud.bannerColor.x, hud.bannerColor.y,
                     hud.bannerColor.z, alpha},
             RailShooterHudTextAlignment::Center);
        text(hud.bannerDetail, width * 0.5f, y + 84.0f * scale,
             0.72f * scale, Vector4{0.82f, 0.92f, 0.96f, alpha},
             RailShooterHudTextAlignment::Center);
    }

    next.visible = !next.commands.empty();
    frame_ = std::move(next);
}
