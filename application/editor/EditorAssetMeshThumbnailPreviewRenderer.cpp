#include "EditorAssetMeshThumbnailPreviewRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace editor {
namespace {

constexpr uint32_t kPreviewSize = 128;

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
    color.a = 255;
    return color;
}

Color Mix(Color a, Color b, uint32_t numerator, uint32_t denominator) {
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

void FillTriangle(
    EditorAssetThumbnailPixelData& pixels,
    int x0,
    int y0,
    int x1,
    int y1,
    int x2,
    int y2,
    Color color) {
    const int minX = (std::max)(0, (std::min)({x0, x1, x2}));
    const int maxX = (std::min)(static_cast<int>(pixels.width) - 1, (std::max)({x0, x1, x2}));
    const int minY = (std::max)(0, (std::min)({y0, y1, y2}));
    const int maxY = (std::min)(static_cast<int>(pixels.height) - 1, (std::max)({y0, y1, y2}));
    const int area = (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);
    if (area == 0) {
        return;
    }
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const int w0 = (x1 - x0) * (y - y0) - (y1 - y0) * (x - x0);
            const int w1 = (x2 - x1) * (y - y1) - (y2 - y1) * (x - x1);
            const int w2 = (x0 - x2) * (y - y2) - (y0 - y2) * (x - x2);
            if ((area > 0 && w0 >= 0 && w1 >= 0 && w2 >= 0) ||
                (area < 0 && w0 <= 0 && w1 <= 0 && w2 <= 0)) {
                PutPixel(pixels, x, y, color);
            }
        }
    }
}

} // namespace

bool RenderEditorAssetMeshThumbnailPreview(
    const EditorAssetGpuThumbnailAllocationRequest& request,
    EditorAssetThumbnailPixelData& outPixels,
    std::string& outError) {
    (void)outError;
    outPixels.width = kPreviewSize;
    outPixels.height = kPreviewSize;
    outPixels.rowPitch = kPreviewSize * 4u;
    outPixels.rgba8.assign(static_cast<size_t>(outPixels.rowPitch) * outPixels.height, 0);

    const Color background{18, 20, 26, 255};
    const Color accent = FromSwatch(request.swatchRgba == 0 ? 0xff88a766u : request.swatchRgba);
    for (uint32_t y = 0; y < outPixels.height; ++y) {
        const Color row = Mix(background, accent, y, outPixels.height * 4u);
        for (uint32_t x = 0; x < outPixels.width; ++x) {
            PutPixel(outPixels, static_cast<int>(x), static_cast<int>(y), row);
        }
    }

    const uint32_t complexity = (std::max)(1u, request.faceCount + request.vertexCount / 3u);
    const float radius = request.boundsRadius > 0.0f ? request.boundsRadius : 1.0f;
    const int spread = (std::clamp)(24 + static_cast<int>(radius * 7.0f) + static_cast<int>(complexity % 13u), 24, 48);
    const int cx = 64;
    const int cy = 61;
    const Color top = Mix(accent, Color{245, 248, 255, 255}, 2, 5);
    const Color left = Mix(accent, Color{20, 24, 32, 255}, 1, 3);
    const Color right = Mix(accent, Color{255, 255, 255, 255}, 1, 5);

    FillTriangle(outPixels, cx, cy - spread, cx - spread, cy, cx, cy + spread, top);
    FillTriangle(outPixels, cx, cy - spread, cx, cy + spread, cx + spread, cy, right);
    FillTriangle(outPixels, cx - spread, cy, cx, cy + spread, cx + spread, cy, left);

    const Color edge = Mix(Color{255, 255, 255, 255}, accent, 1, 3);
    DrawLine(outPixels, cx, cy - spread, cx - spread, cy, edge);
    DrawLine(outPixels, cx - spread, cy, cx, cy + spread, edge);
    DrawLine(outPixels, cx, cy + spread, cx + spread, cy, edge);
    DrawLine(outPixels, cx + spread, cy, cx, cy - spread, edge);
    DrawLine(outPixels, cx - spread, cy, cx + spread, cy, edge);

    const int wireCount = 3 + static_cast<int>(complexity % 4u);
    for (int i = 1; i <= wireCount; ++i) {
        const int offset = (spread * i) / (wireCount + 1);
        DrawLine(outPixels, cx - offset, cy - spread + offset, cx + offset, cy - spread + offset, Mix(edge, left, 1, 3));
        DrawLine(outPixels, cx - spread + offset, cy + offset, cx + spread - offset, cy + offset, Mix(edge, right, 1, 3));
    }

    const uint32_t materialSlots = request.hasMaterialBinding
        ? (std::max)(1u, request.materialSlotCount)
        : 0u;
    for (uint32_t slot = 0; slot < (std::min)(materialSlots, 5u); ++slot) {
        const int x0 = 24 + static_cast<int>(slot) * 17;
        const int y0 = 109;
        const Color swatch = Mix(accent, Color{245, 238, 218, 255}, slot + 1u, materialSlots + 2u);
        for (int y = y0; y < y0 + 8; ++y) {
            for (int x = x0; x < x0 + 12; ++x) {
                PutPixel(outPixels, x, y, swatch);
            }
        }
    }
    return true;
}

} // namespace editor
