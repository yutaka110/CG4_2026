#include "EditorAssetFallbackIconAtlas.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>

namespace editor {
namespace {

constexpr uint32_t kIconSize = 96;

struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
};

Color FromSwatch(uint32_t rgba) {
    Color color{};
    color.r = static_cast<uint8_t>(rgba & 0xffu);
    color.g = static_cast<uint8_t>((rgba >> 8) & 0xffu);
    color.b = static_cast<uint8_t>((rgba >> 16) & 0xffu);
    color.a = static_cast<uint8_t>((rgba >> 24) & 0xffu);
    if (color.a == 0) {
        color.a = 255;
    }
    return color;
}

Color Mix(Color a, Color b, uint32_t numerator, uint32_t denominator) {
    if (denominator == 0) {
        return a;
    }
    Color result{};
    result.r = static_cast<uint8_t>((static_cast<uint32_t>(a.r) * (denominator - numerator) +
                                      static_cast<uint32_t>(b.r) * numerator) /
                                     denominator);
    result.g = static_cast<uint8_t>((static_cast<uint32_t>(a.g) * (denominator - numerator) +
                                      static_cast<uint32_t>(b.g) * numerator) /
                                     denominator);
    result.b = static_cast<uint8_t>((static_cast<uint32_t>(a.b) * (denominator - numerator) +
                                      static_cast<uint32_t>(b.b) * numerator) /
                                     denominator);
    result.a = 255;
    return result;
}

void PutPixel(EditorAssetThumbnailPixelData& pixels, int x, int y, Color color) {
    if (x < 0 || y < 0 || x >= static_cast<int>(pixels.width) || y >= static_cast<int>(pixels.height)) {
        return;
    }
    const size_t index = static_cast<size_t>(y) * pixels.rowPitch + static_cast<size_t>(x) * 4u;
    pixels.rgba8[index + 0] = color.r;
    pixels.rgba8[index + 1] = color.g;
    pixels.rgba8[index + 2] = color.b;
    pixels.rgba8[index + 3] = color.a;
}

void FillRect(EditorAssetThumbnailPixelData& pixels, int left, int top, int right, int bottom, Color color) {
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            PutPixel(pixels, x, y, color);
        }
    }
}

void StrokeRect(EditorAssetThumbnailPixelData& pixels, int left, int top, int right, int bottom, Color color) {
    FillRect(pixels, left, top, right, top + 2, color);
    FillRect(pixels, left, bottom - 2, right, bottom, color);
    FillRect(pixels, left, top, left + 2, bottom, color);
    FillRect(pixels, right - 2, top, right, bottom, color);
}

void DrawLine(EditorAssetThumbnailPixelData& pixels, int x0, int y0, int x1, int y1, Color color) {
    const int dx = (std::abs)(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -(std::abs)(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    while (true) {
        PutPixel(pixels, x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const int error2 = error * 2;
        if (error2 >= dy) {
            error += dy;
            x0 += sx;
        }
        if (error2 <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

void FillBackground(EditorAssetThumbnailPixelData& pixels, Color accent) {
    const Color dark{24, 27, 34, 255};
    const Color soft = Mix(dark, accent, 1, 4);
    for (uint32_t y = 0; y < pixels.height; ++y) {
        const Color row = Mix(dark, soft, y, pixels.height - 1);
        for (uint32_t x = 0; x < pixels.width; ++x) {
            PutPixel(pixels, static_cast<int>(x), static_cast<int>(y), row);
        }
    }
    StrokeRect(pixels, 2, 2, static_cast<int>(pixels.width) - 2, static_cast<int>(pixels.height) - 2, Mix(accent, dark, 1, 3));
}

void DrawMeshIcon(EditorAssetThumbnailPixelData& pixels, Color accent) {
    const Color edge = Mix(accent, Color{242, 247, 255, 255}, 2, 5);
    const std::array<std::pair<int, int>, 8> points = {{
        {31, 38}, {51, 27}, {70, 39}, {50, 51},
        {31, 59}, {51, 72}, {70, 58}, {50, 48},
    }};
    DrawLine(pixels, points[0].first, points[0].second, points[1].first, points[1].second, edge);
    DrawLine(pixels, points[1].first, points[1].second, points[2].first, points[2].second, edge);
    DrawLine(pixels, points[2].first, points[2].second, points[3].first, points[3].second, edge);
    DrawLine(pixels, points[3].first, points[3].second, points[0].first, points[0].second, edge);
    DrawLine(pixels, points[4].first, points[4].second, points[5].first, points[5].second, edge);
    DrawLine(pixels, points[5].first, points[5].second, points[6].first, points[6].second, edge);
    DrawLine(pixels, points[6].first, points[6].second, points[7].first, points[7].second, edge);
    DrawLine(pixels, points[7].first, points[7].second, points[4].first, points[4].second, edge);
    for (int i = 0; i < 4; ++i) {
        DrawLine(pixels, points[i].first, points[i].second, points[i + 4].first, points[i + 4].second, edge);
    }
}

void DrawTextureIcon(EditorAssetThumbnailPixelData& pixels, Color accent) {
    const Color light = Mix(accent, Color{250, 250, 250, 255}, 2, 5);
    for (int y = 28; y < 70; y += 14) {
        for (int x = 28; x < 70; x += 14) {
            const bool odd = ((x + y) / 14) % 2 != 0;
            FillRect(pixels, x, y, x + 12, y + 12, odd ? light : Mix(accent, Color{18, 20, 26, 255}, 1, 2));
        }
    }
    StrokeRect(pixels, 26, 26, 72, 72, light);
}

void DrawAudioIcon(EditorAssetThumbnailPixelData& pixels, Color accent) {
    const Color light = Mix(accent, Color{250, 250, 250, 255}, 2, 5);
    FillRect(pixels, 30, 44, 39, 58, light);
    FillRect(pixels, 39, 38, 49, 64, light);
    DrawLine(pixels, 56, 41, 64, 48, light);
    DrawLine(pixels, 64, 48, 56, 56, light);
    DrawLine(pixels, 62, 35, 73, 48, light);
    DrawLine(pixels, 73, 48, 62, 62, light);
}

void DrawEffectIcon(EditorAssetThumbnailPixelData& pixels, Color accent) {
    const Color light = Mix(accent, Color{255, 255, 255, 255}, 1, 2);
    FillRect(pixels, 44, 23, 52, 74, light);
    FillRect(pixels, 23, 44, 74, 52, light);
    DrawLine(pixels, 31, 31, 65, 65, light);
    DrawLine(pixels, 65, 31, 31, 65, light);
}

void DrawDocumentIcon(EditorAssetThumbnailPixelData& pixels, Color accent) {
    const Color page = Mix(Color{210, 220, 232, 255}, accent, 1, 4);
    FillRect(pixels, 31, 21, 65, 75, page);
    FillRect(pixels, 56, 21, 65, 31, Mix(page, Color{20, 22, 28, 255}, 1, 3));
    for (int y = 40; y < 62; y += 8) {
        FillRect(pixels, 38, y, 58, y + 3, Mix(accent, Color{20, 22, 28, 255}, 1, 2));
    }
}

} // namespace

bool BuildEditorAssetFallbackIconPixels(
    EditorAssetKind kind,
    EditorAssetPreviewKind previewKind,
    uint32_t swatchRgba,
    EditorAssetThumbnailPixelData& outPixels) {
    outPixels.width = kIconSize;
    outPixels.height = kIconSize;
    outPixels.rowPitch = kIconSize * 4u;
    outPixels.rgba8.assign(static_cast<size_t>(outPixels.rowPitch) * outPixels.height, 0);

    const Color accent = FromSwatch(swatchRgba == 0 ? 0xff8a7a52u : swatchRgba);
    FillBackground(outPixels, accent);

    switch (kind) {
    case EditorAssetKind::Mesh:
        DrawMeshIcon(outPixels, accent);
        break;
    case EditorAssetKind::Texture:
        DrawTextureIcon(outPixels, accent);
        break;
    case EditorAssetKind::Audio:
        DrawAudioIcon(outPixels, accent);
        break;
    case EditorAssetKind::Effect:
        DrawEffectIcon(outPixels, accent);
        break;
    case EditorAssetKind::Course:
    case EditorAssetKind::Prefab:
    case EditorAssetKind::MaterialGraph:
    case EditorAssetKind::MaterialInstance:
    case EditorAssetKind::VfxGraph:
    case EditorAssetKind::AnimationStateMachine:
    case EditorAssetKind::GameplayVisualScript:
        DrawDocumentIcon(outPixels, accent);
        break;
    default:
        if (previewKind == EditorAssetPreviewKind::Texture) {
            DrawTextureIcon(outPixels, accent);
        } else if (previewKind == EditorAssetPreviewKind::Audio) {
            DrawAudioIcon(outPixels, accent);
        } else {
            DrawDocumentIcon(outPixels, accent);
        }
        break;
    }
    return true;
}

} // namespace editor
