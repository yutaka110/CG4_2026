#include "RailShooterHudRenderer.h"

#include <algorithm>
#include <string_view>

namespace {
std::string Utf8(std::u8string_view text) {
    return {
        reinterpret_cast<const char*>(text.data()),
        reinterpret_cast<const char*>(text.data() + text.size())};
}

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
        (!input.showTitleScreen &&
         (!input.definition->enabled || !input.presentation->visible)) ||
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

    if (input.showTitleScreen) {
        // A self-contained title uses the existing font atlas and geometric
        // shapes, so it adds no external artwork or texture dependencies.
        const float s = (std::min)(width / 1600.0f, height / 900.0f);
        const float cx = width * 0.5f;
        const float top = (height - 900.0f * s) * 0.5f;
        const Vector4 ink{0.018f, 0.034f, 0.048f, 1.0f};
        const Vector4 cyan{0.24f, 0.83f, 0.90f, 1.0f};
        const Vector4 white{0.91f, 0.96f, 0.97f, 1.0f};
        const Vector4 secondary{0.57f, 0.70f, 0.76f, 1.0f};
        rect(0.0f, 0.0f, width, height, ink);
        // Stepped rail silhouettes frame the title without obscuring the text.
        for (int i = 0; i < 12; ++i) {
            const float depth = static_cast<float>(i) / 11.0f;
            const float spread = (460.0f + depth * depth * 300.0f) * s;
            const float y = top + (140.0f + i * 62.0f) * s;
            const Vector4 rail{0.055f, 0.12f + depth * 0.05f, 0.16f, 1.0f};
            rect(cx - spread, y, 4.0f * s, 48.0f * s, rail);
            rect(cx + spread, y, 4.0f * s, 48.0f * s, rail);
            rect(cx - spread - 18.0f * s, y, 40.0f * s, 3.0f * s, rail);
            rect(cx + spread - 18.0f * s, y, 40.0f * s, 3.0f * s, rail);
        }
        rect(cx - 34.0f * s, top + 130.0f * s, 68.0f * s, 4.0f * s, cyan);
        text("RAIL SHOOTING", cx, top + 185.0f * s, 0.85f * s,
             secondary, RailShooterHudTextAlignment::Center);
        text(Utf8(u8"\u30ec\u30fc\u30eb\u3067"), cx, top + 300.0f * s, 4.4f * s,
             white, RailShooterHudTextAlignment::Center);
        text(Utf8(u8"\u3042\u3070\u30ec\u30fc\u30eb"), cx, top + 405.0f * s, 4.4f * s,
             cyan, RailShooterHudTextAlignment::Center);
        text("RIDE THE RAILS. BREAK THROUGH.", cx, top + 457.0f * s,
             0.85f * s, secondary, RailShooterHudTextAlignment::Center);
        rect(cx - 220.0f * s, top + 516.0f * s, 440.0f * s, 70.0f * s,
             Vector4{0.06f, 0.22f, 0.27f, 1.0f});
        rect(cx - 220.0f * s, top + 516.0f * s, 4.0f * s, 70.0f * s, cyan);
        text("ENTER  /  START GAME", cx, top + 561.0f * s, 1.18f * s,
             white, RailShooterHudTextAlignment::Center);
        text("ESC  /  EXIT", cx, top + 640.0f * s, 0.94f * s,
             secondary, RailShooterHudTextAlignment::Center);
        text("MOUSE: AIM   |   LMB: FIRE   |   RMB: HOLD LOCK / RELEASE", cx,
             top + 761.0f * s, 0.78f * s, secondary,
             RailShooterHudTextAlignment::Center);
        text("WASD: MOVE   |   SHIFT / SPACE: DODGE   |   P: PAUSE", cx,
             top + 799.0f * s, 0.78f * s, secondary,
             RailShooterHudTextAlignment::Center);
        next.visible = !next.commands.empty();
        frame_ = std::move(next);
        return;
    }

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
        text(Utf8(u8"\u6b8b\u6a5f ") + std::to_string(hud.retriesRemaining),
             x + panelWidth - 82.0f * scale,
             y + 18.0f * scale, 0.48f * scale, muted);
    }

    if (hud.showDamageNotice && hud.damageNoticeAlpha > 0.0f) {
        // Keep the combat centre clear and place the explanation by health.
        const float panelWidth = (std::min)(400.0f * scale, width - safe * 2.0f);
        const float panelHeight = 76.0f * scale;
        const float healthHeight = definition.showPlayerHealth && definition.showVehicleIntegrity
            ? 112.0f : (definition.showPlayerHealth || definition.showVehicleIntegrity ? 65.0f : 0.0f);
        const float y = safe + (healthHeight + 12.0f) * scale;
        const float alpha = opacity * hud.damageNoticeAlpha;
        rect(safe, y, panelWidth, panelHeight, {0.035f, 0.012f, 0.012f, alpha * 0.94f});
        rect(safe, y, 4.0f * scale, panelHeight, {1.0f, 0.30f, 0.16f, alpha});
        text(hud.damageNoticeText, safe + 14.0f * scale, y + 29.0f * scale,
            0.78f * scale, {1.0f, 0.68f, 0.44f, alpha});
        text(hud.damageHealthText, safe + 14.0f * scale, y + 57.0f * scale,
            0.70f * scale, {0.96f, 0.96f, 0.96f, alpha});
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
        text(Utf8(u8"\u901f\u5ea6"), x + 11.0f * scale, y + 20.0f * scale,
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

    if (hud.showObstacleWarning) {
        const float panelWidth = (std::min)(440.0f * scale, width - safe * 2.0f);
        const float x = width * 0.5f - panelWidth * 0.5f;
        const float y = safe + 74.0f * scale;
        rect(x, y, panelWidth, 68.0f * scale, {0.045f, 0.025f, 0.008f, opacity * 0.96f});
        rect(x, y, panelWidth, 3.0f * scale, warning);
        text(hud.obstacleWarningText, width * 0.5f, y + 27.0f * scale,
            0.76f * scale, warning, RailShooterHudTextAlignment::Center);
        text(hud.obstacleActionText, width * 0.5f, y + 53.0f * scale,
            0.72f * scale, textColor, RailShooterHudTextAlignment::Center);
    }

    if (definition.showThreat && hud.threatWarning && !hud.showObstacleWarning) {
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

    // Keep the first-play controls available after removing the editor panels.
    // ASCII is covered by the HUD atlas; the strip sits below the main panels.
    if (width >= 960.0f) {
        text("LMB: FIRE   RMB: HOLD LOCK / RELEASE   P: PAUSE   R: RETRY",
            width * 0.5f, height - 10.0f * responsive, 0.52f * responsive,
            textColor, RailShooterHudTextAlignment::Center);
    }
    next.visible = !next.commands.empty();
    frame_ = std::move(next);
}
