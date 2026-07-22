#include "AppSceneResources.h"

#include <cassert>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cwctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <numbers>
#include <utility>
#include <vector>

#include "AppRenderResources.h"
#include "AppRuntimeConfig.h"
#include "AppRuntimeUtils.h"
#include "ModelLoaderAssimp.h"

using Microsoft::WRL::ComPtr;

namespace {

    std::string NormalizeResourcePathKey(const std::string& pathText) {
        if (pathText.empty()) {
            return {};
        }
        std::string key = std::filesystem::path(pathText)
            .lexically_normal()
            .generic_string();
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char value) {
            return static_cast<char>(std::tolower(value));
        });
        return key;
    }

    struct SphereVertex {
        float position[4];
        float texcoord[2];
        float normal[3];
    };

    struct SkyboxVertex {
        Vector4 position;
    };

    struct RingVertex {
        Vector4 position;
        Vector2 texcoord;
        Vector3 normal;
    };

    struct CylinderVertex {
        Vector4 position;
        Vector2 texcoord;
        Vector3 normal;
    };

    struct SpearVertex {
        Vector4 position;
        Vector2 texcoord;
        Vector3 normal;
    };

    struct OrbitRibbonVertex {
        Vector4 position;
        Vector2 texcoord;
        Vector3 normal;
    };

    constexpr uint32_t kSkinningDescriptorBaseIndex = 20;
    constexpr uint32_t kSkinningDescriptorStride = 4;

    uint32_t GetSkinningDescriptorBaseIndex(uint32_t modelIndex) {
        return kSkinningDescriptorBaseIndex + modelIndex * kSkinningDescriptorStride;
    }

    ComPtr<ID3D12Resource> CreateDepthShadowTexture(
        ID3D12Device* device,
        uint32_t width,
        uint32_t height,
        D3D12_RESOURCE_STATES initialState) {
        D3D12_HEAP_PROPERTIES heapProperties{};
        heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC resourceDesc{};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resourceDesc.Width = width;
        resourceDesc.Height = height;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE clearValue{};
        clearValue.Format = DXGI_FORMAT_D32_FLOAT;
        clearValue.DepthStencil.Depth = 1.0f;
        clearValue.DepthStencil.Stencil = 0;

        ComPtr<ID3D12Resource> resource;
        const HRESULT hr = device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            initialState,
            &clearValue,
            IID_PPV_ARGS(&resource));
        assert(SUCCEEDED(hr));
        return resource;
    }

    DirectX::ScratchImage CreateSolidColorTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        DirectX::ScratchImage image;
        const HRESULT hr = image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1);
        assert(SUCCEEDED(hr));
        uint8_t* pixels = image.GetPixels();
        assert(pixels != nullptr);
        pixels[0] = r;
        pixels[1] = g;
        pixels[2] = b;
        pixels[3] = a;
        return image;
    }

    float Hash2D(int32_t x, int32_t y, uint32_t seed) {
        uint32_t h = static_cast<uint32_t>(x) * 0x8da6b343u;
        h ^= static_cast<uint32_t>(y) * 0xd8163841u;
        h ^= seed * 0xcb1ab31fu;
        h ^= h >> 16;
        h *= 0x7feb352du;
        h ^= h >> 15;
        h *= 0x846ca68bu;
        h ^= h >> 16;
        return static_cast<float>(h & 0x00ffffffu) / static_cast<float>(0x01000000u);
    }

    float SmoothNoise2D(float x, float y, uint32_t seed) {
        const float ix = std::floor(x);
        const float iy = std::floor(y);
        float fx = x - ix;
        float fy = y - iy;
        fx = fx * fx * (3.0f - 2.0f * fx);
        fy = fy * fy * (3.0f - 2.0f * fy);
        const int32_t x0 = static_cast<int32_t>(ix);
        const int32_t y0 = static_cast<int32_t>(iy);
        const float n00 = Hash2D(x0, y0, seed);
        const float n10 = Hash2D(x0 + 1, y0, seed);
        const float n01 = Hash2D(x0, y0 + 1, seed);
        const float n11 = Hash2D(x0 + 1, y0 + 1, seed);
        const float nx0 = std::lerp(n00, n10, fx);
        const float nx1 = std::lerp(n01, n11, fx);
        return std::lerp(nx0, nx1, fy);
    }

    uint8_t ToByte(float value) {
        return static_cast<uint8_t>((std::clamp)(value, 0.0f, 1.0f) * 255.0f + 0.5f);
    }

    bool IsDdsFile(const std::filesystem::path& path) {
        std::wstring extension = path.extension().wstring();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](wchar_t c) {
            return static_cast<wchar_t>(std::towlower(c));
        });
        return extension == L".dds";
    }

    bool LoadLinearTextureFile(const std::filesystem::path& path, DirectX::ScratchImage& output) {
        DirectX::ScratchImage image;
        const std::string pathString = path.string();
        const std::wstring pathWide = ConvertString(pathString);

        HRESULT hr = S_OK;
        if (IsDdsFile(path)) {
            hr = DirectX::LoadFromDDSFile(pathWide.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
        } else {
            hr = DirectX::LoadFromWICFile(pathWide.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, image);
        }
        if (FAILED(hr)) {
            return false;
        }

        const DirectX::TexMetadata& metadata = image.GetMetadata();
        if (metadata.IsCubemap() || DirectX::IsCompressed(metadata.format) || metadata.mipLevels > 1) {
            output = std::move(image);
            return true;
        }

        DirectX::ScratchImage mipImages;
        hr = DirectX::GenerateMipMaps(
            image.GetImages(),
            image.GetImageCount(),
            metadata,
            DirectX::TEX_FILTER_DEFAULT,
            0,
            mipImages);
        if (FAILED(hr)) {
            output = std::move(image);
            return true;
        }

        output = std::move(mipImages);
        return true;
    }

    bool CopyScratchImageToArray(
        const DirectX::ScratchImage& source,
        uint32_t arraySize,
        DirectX::ScratchImage& output) {
        const DirectX::TexMetadata& metadata = source.GetMetadata();
        if (metadata.dimension != DirectX::TEX_DIMENSION_TEXTURE2D || metadata.IsCubemap() || metadata.arraySize == 0) {
            return false;
        }

        const HRESULT hr = output.Initialize2D(
            metadata.format,
            metadata.width,
            metadata.height,
            arraySize,
            metadata.mipLevels);
        if (FAILED(hr)) {
            return false;
        }

        for (uint32_t layerIndex = 0; layerIndex < arraySize; ++layerIndex) {
            const size_t sourceLayer = static_cast<size_t>(layerIndex) % metadata.arraySize;
            for (size_t mipLevel = 0; mipLevel < metadata.mipLevels; ++mipLevel) {
                const DirectX::Image* src = source.GetImage(mipLevel, sourceLayer, 0);
                const DirectX::Image* dst = output.GetImage(mipLevel, layerIndex, 0);
                if (src == nullptr || dst == nullptr || src->pixels == nullptr || dst->pixels == nullptr) {
                    return false;
                }
                std::memcpy(dst->pixels, src->pixels, (std::min)(dst->slicePitch, src->slicePitch));
            }
        }

        return true;
    }

    bool TryLoadTerrainDetailNormalMapAsset(DirectX::ScratchImage& output, std::string& sourcePath) {
        static constexpr uint32_t kLayerCount = 4;
        const std::filesystem::path candidates[] = {
            "Resources/terrain/terrain_detail_normal.dds",
            "Resources/terrain/terrain_detail_normal.png",
            "Resources/terrain/terrain_detail_normal.jpg",
            "Resources/terrain/terrain_detail_normal.jpeg",
        };

        for (const std::filesystem::path& candidate : candidates) {
            if (!std::filesystem::exists(candidate)) {
                continue;
            }

            DirectX::ScratchImage loaded;
            if (!LoadLinearTextureFile(candidate, loaded)) {
                continue;
            }

            DirectX::ScratchImage asArray;
            if (!CopyScratchImageToArray(loaded, kLayerCount, asArray)) {
                continue;
            }

            output = std::move(asArray);
            sourcePath = candidate.string();
            return true;
        }

        return false;
    }

    struct TerrainDetailPattern {
        float grain = 0.0f;
        float verticalCrack = 0.0f;
        float chipped = 0.0f;
        float cavity = 0.0f;
    };

    TerrainDetailPattern EvaluateTerrainDetailPattern(float u, float v, uint32_t layerIndex) {
        const float layerOffset = static_cast<float>(layerIndex) * 19.37f;
        const float grainScale = 1.0f + static_cast<float>(layerIndex) * 0.17f;
        const float strataScale = 1.0f + static_cast<float>((layerIndex + 1u) % 3u) * 0.12f;
        const float crackBias = 0.68f + static_cast<float>(layerIndex) * 0.025f;
        const uint32_t seedOffset = layerIndex * 8191u;

        TerrainDetailPattern pattern{};
        pattern.grain =
            SmoothNoise2D(u * 38.0f * grainScale + layerOffset, v * 38.0f, 1337u + seedOffset) * 0.40f +
            SmoothNoise2D(u * 91.0f + 17.0f, v * 91.0f * grainScale + layerOffset, 1447u + seedOffset) * 0.37f +
            SmoothNoise2D(u * 177.0f * grainScale, v * 177.0f + 31.0f, 1559u + seedOffset) * 0.23f;
        const float column =
            SmoothNoise2D(u * 18.0f + layerOffset, v * 2.4f, 2311u + seedOffset) * 0.72f +
            SmoothNoise2D(u * 54.0f + 8.0f, v * 4.8f + layerOffset, 2333u + seedOffset) * 0.28f;
        const float fineSplit =
            SmoothNoise2D(u * 112.0f, v * 12.0f + 23.0f + layerOffset, 2473u + seedOffset);
        pattern.verticalCrack =
            (std::clamp)((column - crackBias) / 0.25f, 0.0f, 1.0f) *
            (std::clamp)((fineSplit - 0.32f) / 0.54f, 0.0f, 1.0f);
        const float layerLine = std::abs(std::fmod(
            v * 21.0f * strataScale + SmoothNoise2D(u * 9.0f + layerOffset, v * 9.0f, 3011u + seedOffset) * 0.56f,
            1.0f) - 0.5f);
        pattern.chipped =
            (std::clamp)((0.085f - layerLine) / 0.085f, 0.0f, 1.0f) *
            (std::clamp)((SmoothNoise2D(u * 63.0f, v * 16.0f + 11.0f + layerOffset, 3251u + seedOffset) - 0.36f) / 0.52f, 0.0f, 1.0f);
        pattern.cavity =
            SmoothNoise2D(u * 24.0f + 5.0f + layerOffset, v * 31.0f, 4001u + seedOffset) * 0.32f +
            pattern.verticalCrack * 0.40f +
            pattern.chipped * 0.28f;
        return pattern;
    }

    float TerrainDetailHeightFromPattern(const TerrainDetailPattern& pattern) {
        return
            (pattern.grain - 0.5f) * 0.52f +
            pattern.verticalCrack * 0.36f +
            pattern.chipped * 0.24f +
            pattern.cavity * 0.28f;
    }

    DirectX::ScratchImage CreateTerrainDetailCacheTexture() {
        constexpr uint32_t kSize = 512;
        constexpr uint32_t kLayerCount = 4;
        DirectX::ScratchImage image;
        const HRESULT hr = image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, kSize, kSize, kLayerCount, 1);
        assert(SUCCEEDED(hr));

        for (uint32_t layerIndex = 0; layerIndex < kLayerCount; ++layerIndex) {
            const DirectX::Image* img = image.GetImage(0, layerIndex, 0);
            assert(img != nullptr);
            uint8_t* pixels = img->pixels;
            assert(pixels != nullptr);

            for (uint32_t y = 0; y < kSize; ++y) {
                for (uint32_t x = 0; x < kSize; ++x) {
                    const float u = static_cast<float>(x) / static_cast<float>(kSize);
                    const float v = static_cast<float>(y) / static_cast<float>(kSize);
                    const TerrainDetailPattern pattern = EvaluateTerrainDetailPattern(u, v, layerIndex);

                    uint8_t* dst = pixels + y * img->rowPitch + x * 4u;
                    dst[0] = ToByte(pattern.grain);
                    dst[1] = ToByte(pattern.verticalCrack);
                    dst[2] = ToByte(pattern.chipped);
                    dst[3] = ToByte(pattern.cavity);
                }
            }
        }

        return image;
    }

    DirectX::ScratchImage CreateTerrainDetailNormalMapTexture() {
        constexpr uint32_t kSize = 512;
        constexpr uint32_t kLayerCount = 4;
        DirectX::ScratchImage image;
        const HRESULT hr = image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, kSize, kSize, kLayerCount, 1);
        assert(SUCCEEDED(hr));

        const float invSize = 1.0f / static_cast<float>(kSize);
        for (uint32_t layerIndex = 0; layerIndex < kLayerCount; ++layerIndex) {
            const DirectX::Image* img = image.GetImage(0, layerIndex, 0);
            assert(img != nullptr);
            uint8_t* pixels = img->pixels;
            assert(pixels != nullptr);

            for (uint32_t y = 0; y < kSize; ++y) {
                for (uint32_t x = 0; x < kSize; ++x) {
                    const float u = static_cast<float>(x) * invSize;
                    const float v = static_cast<float>(y) * invSize;
                    const float hL = TerrainDetailHeightFromPattern(EvaluateTerrainDetailPattern(u - invSize, v, layerIndex));
                    const float hR = TerrainDetailHeightFromPattern(EvaluateTerrainDetailPattern(u + invSize, v, layerIndex));
                    const float hD = TerrainDetailHeightFromPattern(EvaluateTerrainDetailPattern(u, v - invSize, layerIndex));
                    const float hU = TerrainDetailHeightFromPattern(EvaluateTerrainDetailPattern(u, v + invSize, layerIndex));
                    const float dx = (hR - hL) * 2.65f;
                    const float dy = (hU - hD) * 2.65f;
                    const float invLen = 1.0f / std::sqrt(dx * dx + dy * dy + 1.0f);

                    uint8_t* dst = pixels + y * img->rowPitch + x * 4u;
                    dst[0] = ToByte(-dx * invLen * 0.5f + 0.5f);
                    dst[1] = ToByte(-dy * invLen * 0.5f + 0.5f);
                    dst[2] = ToByte(invLen * 0.5f + 0.5f);
                    dst[3] = 255;
                }
            }
        }

        return image;
    }

    std::vector<SphereVertex> BuildSphereVertices(uint32_t stackCount, uint32_t sliceCount) {
        std::vector<SphereVertex> v;
        v.reserve(stackCount * sliceCount * 6);

        for (uint32_t y = 0; y < stackCount; ++y) {
            float v0 = (float)y / (float)stackCount;
            float v1 = (float)(y + 1) / (float)stackCount;
            float phi0 = v0 * std::numbers::pi_v<float>;
            float phi1 = v1 * std::numbers::pi_v<float>;

            for (uint32_t x = 0; x < sliceCount; ++x) {
                float u0 = (float)x / (float)sliceCount;
                float u1 = (float)(x + 1) / (float)sliceCount;
                float theta0 = u0 * (std::numbers::pi_v<float> *2.0f);
                float theta1 = u1 * (std::numbers::pi_v<float> *2.0f);

                auto MakeV = [](float phi, float theta, float u, float vTex) {
                    SphereVertex sv{};
                    float sx = std::sin(phi) * std::cos(theta);
                    float sy = std::cos(phi);
                    float sz = std::sin(phi) * std::sin(theta);

                    sv.position[0] = sx;
                    sv.position[1] = sy;
                    sv.position[2] = sz;
                    sv.position[3] = 1.0f;

                    sv.texcoord[0] = u;
                    sv.texcoord[1] = vTex;

                    sv.normal[0] = sx;
                    sv.normal[1] = sy;
                    sv.normal[2] = sz;
                    return sv;
                    };

                SphereVertex a = MakeV(phi0, theta0, u0, v0);
                SphereVertex b = MakeV(phi0, theta1, u1, v0);
                SphereVertex c = MakeV(phi1, theta0, u0, v1);
                SphereVertex d = MakeV(phi1, theta1, u1, v1);

                v.push_back(a);
                v.push_back(c);
                v.push_back(b);
                v.push_back(b);
                v.push_back(c);
                v.push_back(d);
            }
        }

        return v;
    }

    std::vector<SkyboxVertex> BuildSkyboxVertices() {
        constexpr float k = 1.0f;
        const Vector4 p000{-k, -k, -k, 1.0f};
        const Vector4 p001{-k, -k,  k, 1.0f};
        const Vector4 p010{-k,  k, -k, 1.0f};
        const Vector4 p011{-k,  k,  k, 1.0f};
        const Vector4 p100{ k, -k, -k, 1.0f};
        const Vector4 p101{ k, -k,  k, 1.0f};
        const Vector4 p110{ k,  k, -k, 1.0f};
        const Vector4 p111{ k,  k,  k, 1.0f};

        return {
            {p100}, {p110}, {p111}, {p100}, {p111}, {p101},
            {p000}, {p001}, {p011}, {p000}, {p011}, {p010},
            {p010}, {p011}, {p111}, {p010}, {p111}, {p110},
            {p000}, {p100}, {p101}, {p000}, {p101}, {p001},
            {p001}, {p101}, {p111}, {p001}, {p111}, {p011},
            {p000}, {p010}, {p110}, {p000}, {p110}, {p100},
        };
    }

    std::vector<RingVertex> BuildRingVertices(uint32_t divide) {
        std::vector<RingVertex> vertices;
        divide = (std::max)(uint32_t{3}, divide);
        vertices.reserve(static_cast<size_t>(divide) * 6);
        const float radianPerDivide = 2.0f * std::numbers::pi_v<float> / static_cast<float>(divide);

        auto makeVertex = [](float sinValue, float cosValue, float v, float u) {
            RingVertex vertex{};
            vertex.position = {sinValue, cosValue, 0.0f, 1.0f};
            vertex.texcoord = {u, v};
            vertex.normal = {0.0f, 0.0f, -1.0f};
            return vertex;
        };

        for (uint32_t index = 0; index < divide; ++index) {
            const float angle = static_cast<float>(index) * radianPerDivide;
            const float nextAngle = static_cast<float>(index + 1) * radianPerDivide;
            const float sin0 = std::sin(angle);
            const float cos0 = std::cos(angle);
            const float sin1 = std::sin(nextAngle);
            const float cos1 = std::cos(nextAngle);
            const float u0 = static_cast<float>(index) / static_cast<float>(divide);
            const float u1 = static_cast<float>(index + 1) / static_cast<float>(divide);

            const RingVertex outer0 = makeVertex(sin0, cos0, 0.0f, u0);
            const RingVertex outer1 = makeVertex(sin1, cos1, 0.0f, u1);
            const RingVertex inner0 = makeVertex(sin0, cos0, 1.0f, u0);
            const RingVertex inner1 = makeVertex(sin1, cos1, 1.0f, u1);

            vertices.push_back(outer0);
            vertices.push_back(outer1);
            vertices.push_back(inner0);
            vertices.push_back(inner0);
            vertices.push_back(outer1);
            vertices.push_back(inner1);
        }
        return vertices;
    }

    std::vector<CylinderVertex> BuildCylinderVertices(uint32_t divide) {
        std::vector<CylinderVertex> vertices;
        divide = (std::max)(uint32_t{3}, divide);
        vertices.reserve(static_cast<size_t>(divide) * 6);
        const float radianPerDivide = 2.0f * std::numbers::pi_v<float> / static_cast<float>(divide);

        auto makeVertex = [](float sinValue, float cosValue, float y, float u, float v) {
            CylinderVertex vertex{};
            vertex.position = {-sinValue, y, cosValue, 1.0f};
            vertex.texcoord = {u, v};
            vertex.normal = {-sinValue, 0.0f, cosValue};
            return vertex;
        };

        for (uint32_t index = 0; index < divide; ++index) {
            const float angle = static_cast<float>(index) * radianPerDivide;
            const float nextAngle = static_cast<float>(index + 1) * radianPerDivide;
            const float sin0 = std::sin(angle);
            const float cos0 = std::cos(angle);
            const float sin1 = std::sin(nextAngle);
            const float cos1 = std::cos(nextAngle);
            const float u0 = static_cast<float>(index) / static_cast<float>(divide);
            const float u1 = static_cast<float>(index + 1) / static_cast<float>(divide);

            const CylinderVertex top0 = makeVertex(sin0, cos0, 1.0f, u0, 0.0f);
            const CylinderVertex top1 = makeVertex(sin1, cos1, 1.0f, u1, 0.0f);
            const CylinderVertex bottom0 = makeVertex(sin0, cos0, 0.0f, u0, 1.0f);
            const CylinderVertex bottom1 = makeVertex(sin1, cos1, 0.0f, u1, 1.0f);

            vertices.push_back(top0);
            vertices.push_back(top1);
            vertices.push_back(bottom0);
            vertices.push_back(bottom0);
            vertices.push_back(top1);
            vertices.push_back(bottom1);
        }
        return vertices;
    }

    std::vector<SpearVertex> BuildSpearVertices() {
        auto makeVertex = [](float x, float y, float u, float edge) {
            SpearVertex vertex{};
            vertex.position = {x, y, 0.0f, 1.0f};
            vertex.texcoord = {u, edge};
            vertex.normal = {0.0f, 0.0f, -1.0f};
            return vertex;
        };

        auto smoothStep = [](float edge0, float edge1, float x) {
            const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        };

        auto radiusAt = [smoothStep](float t) {
            constexpr float kMaxRadius = 0.56f;
            constexpr float kBulbCenter = 0.72f;
            if (t <= kBulbCenter) {
                const float tailGrow = smoothStep(0.0f, 0.28f, t);
                const float shoulder = 0.72f + 0.28f * smoothStep(0.24f, kBulbCenter, t);
                return kMaxRadius * tailGrow * shoulder;
            }

            const float capT = (t - kBulbCenter) / (1.0f - kBulbCenter);
            return kMaxRadius * std::sqrt((std::max)(0.0f, 1.0f - capT * capT));
        };

        constexpr int kLengthSegments = 32;
        constexpr int kWidthSegments = 10;
        constexpr float kMinX = -0.96f;
        constexpr float kMaxX = 0.62f;

        std::vector<SpearVertex> vertices;
        vertices.reserve(static_cast<size_t>(kLengthSegments) * kWidthSegments * 6);

        auto makeGridVertex = [&](int xIndex, int yIndex) {
            const float u = static_cast<float>(xIndex) / static_cast<float>(kLengthSegments);
            const float v = -1.0f + 2.0f * static_cast<float>(yIndex) / static_cast<float>(kWidthSegments);
            const float radius = radiusAt(u);
            const float x = kMinX + (kMaxX - kMinX) * u;
            const float y = radius * v;
            return makeVertex(x, y, u, std::abs(v));
        };

        for (int x = 0; x < kLengthSegments; ++x) {
            for (int y = 0; y < kWidthSegments; ++y) {
                const SpearVertex a = makeGridVertex(x, y);
                const SpearVertex b = makeGridVertex(x + 1, y);
                const SpearVertex c = makeGridVertex(x, y + 1);
                const SpearVertex d = makeGridVertex(x + 1, y + 1);

                vertices.push_back(a);
                vertices.push_back(c);
                vertices.push_back(b);
                vertices.push_back(b);
                vertices.push_back(c);
                vertices.push_back(d);
            }
        }

        return vertices;
    }

    void AppendBox(
        ModelData& model,
        const Vector3& minimum,
        const Vector3& maximum) {
        const Vector3 corners[8] = {
            {minimum.x, minimum.y, minimum.z},
            {minimum.x, maximum.y, minimum.z},
            {maximum.x, maximum.y, minimum.z},
            {maximum.x, minimum.y, minimum.z},
            {minimum.x, minimum.y, maximum.z},
            {minimum.x, maximum.y, maximum.z},
            {maximum.x, maximum.y, maximum.z},
            {maximum.x, minimum.y, maximum.z},
        };
        struct Face {
            uint32_t corners[4];
            Vector3 normal;
        };
        const Face faces[6] = {
            {{4, 5, 6, 7}, {0.0f, 0.0f, 1.0f}},
            {{3, 2, 1, 0}, {0.0f, 0.0f, -1.0f}},
            {{7, 6, 2, 3}, {1.0f, 0.0f, 0.0f}},
            {{0, 1, 5, 4}, {-1.0f, 0.0f, 0.0f}},
            {{5, 1, 2, 6}, {0.0f, 1.0f, 0.0f}},
            {{0, 4, 7, 3}, {0.0f, -1.0f, 0.0f}},
        };
        const Vector2 uvs[4] = {
            {0.0f, 1.0f},
            {0.0f, 0.0f},
            {1.0f, 0.0f},
            {1.0f, 1.0f},
        };

        for (const Face& face : faces) {
            const uint32_t base = static_cast<uint32_t>(model.vertices.size());
            for (uint32_t corner = 0; corner < 4; ++corner) {
                const Vector3& position = corners[face.corners[corner]];
                model.vertices.push_back({
                    {position.x, position.y, position.z, 1.0f},
                    uvs[corner],
                    face.normal,
                });
            }
            model.indices.insert(model.indices.end(), {
                base + 0, base + 1, base + 2,
                base + 0, base + 2, base + 3,
            });
        }
    }

    ModelData BuildTrainingSwordModelData() {
        ModelData model;
        // The socket origin sits inside the grip and the blade extends along
        // local +Y. A white albedo preserves the authored material colours;
        // using the dark VFX gradation texture made the weapon effectively
        // disappear under the fixed submission lighting.
        constexpr const char* kWhiteAlbedo = "Resources/human/white.png";
        model.materials = {
            {"blade", kWhiteAlbedo, {}, {0.55f, 0.78f, 1.00f, 1.0f}},
            {"guard", kWhiteAlbedo, {}, {1.00f, 0.58f, 0.10f, 1.0f}},
            {"grip", kWhiteAlbedo, {}, {0.16f, 0.045f, 0.025f, 1.0f}},
        };
        model.material = model.materials.front();

        auto appendPart = [&](const char* name, uint32_t materialIndex, auto appendGeometry) {
            const uint32_t indexStart = static_cast<uint32_t>(model.indices.size());
            appendGeometry();
            model.subMeshes.push_back({
                name,
                indexStart,
                static_cast<uint32_t>(model.indices.size()) - indexStart,
                materialIndex,
            });
        };
        appendPart("grip_and_pommel", 2, [&]() {
            AppendBox(model, {-0.045f, -0.22f, -0.045f}, {0.045f, 0.08f, 0.045f});
            AppendBox(model, {-0.085f, -0.29f, -0.065f}, {0.085f, -0.22f, 0.065f});
        });
        appendPart("guard", 1, [&]() {
            AppendBox(model, {-0.30f, 0.08f, -0.06f}, {0.30f, 0.14f, 0.06f});
        });
        appendPart("blade_and_tip", 0, [&]() {
            AppendBox(model, {-0.065f, 0.14f, -0.025f}, {0.065f, 1.02f, 0.025f});
            AppendBox(model, {-0.045f, 1.02f, -0.018f}, {0.045f, 1.14f, 0.018f});
        });
        model.rootNode.name = "training_sword_root";
        model.rootNode.transform = {
            {1.0f, 1.0f, 1.0f},
            {0.0f, 0.0f, 0.0f, 1.0f},
            {0.0f, 0.0f, 0.0f},
        };
        model.rootNode.localMatrix = MakeIdentity4x4();
        EnsureModelDataMaterialLayout(model);
        const ModelGeometryOrientationStats orientation =
            RepairModelGeometryOrientation(model);
        if (!ValidateModelDataMaterialLayout(model) ||
            !ValidateModelGeometryOrientation(model) ||
            orientation.degenerateTriangleCount != 0) {
            OutputDebugStringA(
                "[AppSceneResources] Training sword CPU geometry validation failed.\n");
        }
        return model;
    }

    std::vector<OrbitRibbonVertex> BuildOrbitRibbonVertices() {
        auto makeVertex = [](float t, float side, float ribbonIndex) {
            OrbitRibbonVertex vertex{};
            vertex.position = {t, side, ribbonIndex, 1.0f};
            vertex.texcoord = {t, side * 0.5f + 0.5f};
            vertex.normal = {0.0f, 0.0f, 1.0f};
            return vertex;
        };

        constexpr int kRibbonCount = 4;
        constexpr int kSegments = 44;
        std::vector<OrbitRibbonVertex> vertices;
        vertices.reserve(static_cast<size_t>(kRibbonCount) * kSegments * 6);

        for (int ribbon = 0; ribbon < kRibbonCount; ++ribbon) {
            const float ribbonIndex = static_cast<float>(ribbon);
            for (int segment = 0; segment < kSegments; ++segment) {
                const float t0 = static_cast<float>(segment) / static_cast<float>(kSegments);
                const float t1 = static_cast<float>(segment + 1) / static_cast<float>(kSegments);

                const OrbitRibbonVertex a = makeVertex(t0, -1.0f, ribbonIndex);
                const OrbitRibbonVertex b = makeVertex(t1, -1.0f, ribbonIndex);
                const OrbitRibbonVertex c = makeVertex(t0, 1.0f, ribbonIndex);
                const OrbitRibbonVertex d = makeVertex(t1, 1.0f, ribbonIndex);

                vertices.push_back(a);
                vertices.push_back(c);
                vertices.push_back(b);
                vertices.push_back(b);
                vertices.push_back(c);
                vertices.push_back(d);
            }
        }

        return vertices;
    }

    const NodeAnimation* FindNodeAnimationRecursive(
        const AnimationClip& animation,
        const Node& node) {
        auto found = animation.nodeAnimations.find(node.name);
        if (found != animation.nodeAnimations.end()) {
            return &found->second;
        }

        for (const Node& child : node.children) {
            if (const NodeAnimation* childAnimation =
                    FindNodeAnimationRecursive(animation, child)) {
                return childAnimation;
            }
        }
        return nullptr;
    }

    const NodeAnimation* FindAnimatedCubeNodeAnimation(
        const AnimationClip& animation,
        const Node& rootNode) {
        if (const NodeAnimation* nodeAnimation =
                FindNodeAnimationRecursive(animation, rootNode)) {
            return nodeAnimation;
        }
        if (!animation.nodeAnimations.empty()) {
            return &animation.nodeAnimations.begin()->second;
        }
        return nullptr;
    }

    Vector3 ExtractTranslation(const Matrix4x4& matrix) {
        return { matrix.m[3][0], matrix.m[3][1], matrix.m[3][2] };
    }

    Matrix4x4 InverseCopy(Matrix4x4 matrix) {
        return Inverse(matrix);
    }

    ComPtr<ID3D12Resource> CreateUavBufferResource(
        ComPtr<ID3D12Device> device,
        size_t sizeInBytes,
        D3D12_RESOURCE_STATES initialState) {
        (void)initialState;
        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC resourceDesc{};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Width = (sizeInBytes + 0xFF) & ~static_cast<size_t>(0xFF);
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        ComPtr<ID3D12Resource> resource;
        const HRESULT hr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&resource));
        assert(SUCCEEDED(hr));
        return resource;
    }

    ComPtr<ID3D12Resource> CreateDefaultBufferResource(
        ComPtr<ID3D12Device> device,
        size_t sizeInBytes,
        D3D12_RESOURCE_STATES initialState) {
        (void)initialState;
        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC resourceDesc{};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Width = (sizeInBytes + 0xFF) & ~static_cast<size_t>(0xFF);
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ComPtr<ID3D12Resource> resource;
        const HRESULT hr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&resource));
        assert(SUCCEEDED(hr));
        return resource;
    }

    void UploadStaticBufferData(
        ComPtr<ID3D12Device> device,
        ID3D12GraphicsCommandList* commandList,
        ID3D12Resource* destination,
        const void* source,
        size_t sizeInBytes,
        D3D12_RESOURCE_STATES finalState,
        std::vector<ComPtr<ID3D12Resource>>& retainedUploadResources) {
        assert(commandList != nullptr);
        assert(destination != nullptr);
        assert(source != nullptr);
        assert(sizeInBytes > 0);

        ComPtr<ID3D12Resource> uploadResource = CreateBufferResource(device, sizeInBytes);
        void* mappedData = nullptr;
        uploadResource->Map(0, nullptr, &mappedData);
        std::memcpy(mappedData, source, sizeInBytes);
        uploadResource->Unmap(0, nullptr);

        D3D12_RESOURCE_BARRIER copyBarrier{};
        copyBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        copyBarrier.Transition.pResource = destination;
        copyBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
        copyBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        copyBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &copyBarrier);

        commandList->CopyBufferRegion(
            destination,
            0,
            uploadResource.Get(),
            0,
            sizeInBytes);

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = destination;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = finalState;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &barrier);

        retainedUploadResources.push_back(uploadResource);
    }

    GpuMeshResource CreateGpuMeshResource(
        ComPtr<ID3D12Device> device,
        ID3D12GraphicsCommandList* uploadCommandList,
        const ModelData& modelData,
        std::vector<ComPtr<ID3D12Resource>>& retainedUploadResources) {
        GpuMeshResource mesh{};
        mesh.vertexCount = UINT(modelData.vertices.size());
        mesh.indexCount = UINT(modelData.indices.size());
        if (mesh.vertexCount == 0 || mesh.indexCount == 0) {
            return mesh;
        }

        constexpr D3D12_RESOURCE_STATES kStaticVertexReadState =
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER |
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

        mesh.vertexResource =
            CreateDefaultBufferResource(
                device,
                sizeof(VertexData) * modelData.vertices.size(),
                D3D12_RESOURCE_STATE_COMMON);
        UploadStaticBufferData(
            device,
            uploadCommandList,
            mesh.vertexResource.Get(),
            modelData.vertices.data(),
            sizeof(VertexData) * modelData.vertices.size(),
            kStaticVertexReadState,
            retainedUploadResources);

        mesh.vbv.BufferLocation = mesh.vertexResource->GetGPUVirtualAddress();
        mesh.vbv.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
        mesh.vbv.StrideInBytes = sizeof(VertexData);

        mesh.indexResource =
            CreateDefaultBufferResource(
                device,
                sizeof(uint32_t) * modelData.indices.size(),
                D3D12_RESOURCE_STATE_COMMON);
        UploadStaticBufferData(
            device,
            uploadCommandList,
            mesh.indexResource.Get(),
            modelData.indices.data(),
            sizeof(uint32_t) * modelData.indices.size(),
            D3D12_RESOURCE_STATE_INDEX_BUFFER,
            retainedUploadResources);

        mesh.ibv.BufferLocation = mesh.indexResource->GetGPUVirtualAddress();
        mesh.ibv.SizeInBytes = UINT(sizeof(uint32_t) * modelData.indices.size());
        mesh.ibv.Format = DXGI_FORMAT_R32_UINT;
        return mesh;
    }

    SkinCluster CreateSkinCluster(
        ComPtr<ID3D12Device> device,
        const Skeleton& skeleton,
        const ModelData& modelData,
        const GpuMeshResource& mesh,
        ID3D12GraphicsCommandList* uploadCommandList,
        ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap,
        uint32_t descriptorSizeSRV,
        uint32_t skinningDescriptorBaseIndex,
        std::vector<ComPtr<ID3D12Resource>>& retainedUploadResources) {
        SkinCluster skinCluster{};
        const size_t jointCount = skeleton.joints.size();
        const size_t vertexCount = modelData.vertices.size();
        if (jointCount == 0 || vertexCount == 0 || !mesh.vertexResource) {
            return skinCluster;
        }

        const uint32_t vertexSrvIndex = skinningDescriptorBaseIndex + 0;
        const uint32_t influenceSrvIndex = skinningDescriptorBaseIndex + 1;
        const uint32_t paletteSrvIndex = skinningDescriptorBaseIndex + 2;
        const uint32_t skinnedVertexUavIndex = skinningDescriptorBaseIndex + 3;

        skinCluster.inverseBindPoseMatrices.resize(jointCount);
        std::fill(
            skinCluster.inverseBindPoseMatrices.begin(),
            skinCluster.inverseBindPoseMatrices.end(),
            MakeIdentity4x4());
        skinCluster.meshRootInverseMatrix = MakeIdentity4x4();
        Matrix4x4 meshRootBindMatrix = MakeIdentity4x4();
        if (TryBuildMeshRootBindMatrix(
                skeleton,
                modelData,
                meshRootBindMatrix)) {
            skinCluster.meshRootInverseMatrix = Inverse(meshRootBindMatrix);
        }

        skinCluster.paletteEntries.resize(jointCount);
        skinCluster.paletteResource =
            CreateDefaultBufferResource(
                device,
                sizeof(JointPaletteEntry) * jointCount,
                D3D12_RESOURCE_STATE_COMMON);
        skinCluster.paletteState = D3D12_RESOURCE_STATE_COMMON;
        skinCluster.paletteUploadResource =
            CreateBufferResource(device, sizeof(JointPaletteEntry) * jointCount);
        skinCluster.paletteUploadResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&skinCluster.mappedPaletteUpload));

        skinCluster.paletteSrvCpu =
            AppRenderResources::GetCPUDescriptorHandle(
                srvDescriptorHeap,
                descriptorSizeSRV,
                paletteSrvIndex);
        skinCluster.paletteSrvGpu =
            AppRenderResources::GetGPUDescriptorHandle(
                srvDescriptorHeap,
                descriptorSizeSRV,
                paletteSrvIndex);

        D3D12_SHADER_RESOURCE_VIEW_DESC paletteSrvDesc{};
        paletteSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
        paletteSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        paletteSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        paletteSrvDesc.Buffer.FirstElement = 0;
        paletteSrvDesc.Buffer.NumElements = UINT(jointCount);
        paletteSrvDesc.Buffer.StructureByteStride = sizeof(JointPaletteEntry);
        paletteSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        device->CreateShaderResourceView(
            skinCluster.paletteResource.Get(),
            &paletteSrvDesc,
            skinCluster.paletteSrvCpu);

        skinCluster.vertexSrvCpu =
            AppRenderResources::GetCPUDescriptorHandle(
                srvDescriptorHeap,
                descriptorSizeSRV,
                vertexSrvIndex);
        skinCluster.vertexSrvGpu =
            AppRenderResources::GetGPUDescriptorHandle(
                srvDescriptorHeap,
                descriptorSizeSRV,
                vertexSrvIndex);

        D3D12_SHADER_RESOURCE_VIEW_DESC vertexSrvDesc{};
        vertexSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
        vertexSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        vertexSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        vertexSrvDesc.Buffer.FirstElement = 0;
        vertexSrvDesc.Buffer.NumElements = UINT(vertexCount);
        vertexSrvDesc.Buffer.StructureByteStride = sizeof(VertexData);
        vertexSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        device->CreateShaderResourceView(
            mesh.vertexResource.Get(),
            &vertexSrvDesc,
            skinCluster.vertexSrvCpu);

        std::vector<VertexInfluence> influences(vertexCount);

        constexpr D3D12_RESOURCE_STATES kStaticInfluenceReadState =
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER |
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        skinCluster.influenceResource =
            CreateDefaultBufferResource(
                device,
                sizeof(VertexInfluence) * vertexCount,
                D3D12_RESOURCE_STATE_COMMON);
        skinCluster.influenceBufferView.BufferLocation =
            skinCluster.influenceResource->GetGPUVirtualAddress();
        skinCluster.influenceBufferView.SizeInBytes =
            UINT(sizeof(VertexInfluence) * vertexCount);
        skinCluster.influenceBufferView.StrideInBytes = sizeof(VertexInfluence);

        skinCluster.influenceSrvCpu =
            AppRenderResources::GetCPUDescriptorHandle(
                srvDescriptorHeap,
                descriptorSizeSRV,
                influenceSrvIndex);
        skinCluster.influenceSrvGpu =
            AppRenderResources::GetGPUDescriptorHandle(
                srvDescriptorHeap,
                descriptorSizeSRV,
                influenceSrvIndex);

        D3D12_SHADER_RESOURCE_VIEW_DESC influenceSrvDesc{};
        influenceSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
        influenceSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        influenceSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        influenceSrvDesc.Buffer.FirstElement = 0;
        influenceSrvDesc.Buffer.NumElements = UINT(vertexCount);
        influenceSrvDesc.Buffer.StructureByteStride = sizeof(VertexInfluence);
        influenceSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        device->CreateShaderResourceView(
            skinCluster.influenceResource.Get(),
            &influenceSrvDesc,
            skinCluster.influenceSrvCpu);

        skinCluster.skinnedVertexResource = CreateUavBufferResource(
            device,
            sizeof(VertexData) * vertexCount,
            D3D12_RESOURCE_STATE_COMMON);
        skinCluster.skinnedVertexState = D3D12_RESOURCE_STATE_COMMON;
        skinCluster.skinnedVertexBufferView.BufferLocation =
            skinCluster.skinnedVertexResource->GetGPUVirtualAddress();
        skinCluster.skinnedVertexBufferView.SizeInBytes =
            UINT(sizeof(VertexData) * vertexCount);
        skinCluster.skinnedVertexBufferView.StrideInBytes = sizeof(VertexData);

        skinCluster.skinnedVertexUavCpu =
            AppRenderResources::GetCPUDescriptorHandle(
                srvDescriptorHeap,
                descriptorSizeSRV,
                skinnedVertexUavIndex);
        skinCluster.skinnedVertexUavGpu =
            AppRenderResources::GetGPUDescriptorHandle(
                srvDescriptorHeap,
                descriptorSizeSRV,
                skinnedVertexUavIndex);

        D3D12_UNORDERED_ACCESS_VIEW_DESC skinnedVertexUavDesc{};
        skinnedVertexUavDesc.Format = DXGI_FORMAT_UNKNOWN;
        skinnedVertexUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        skinnedVertexUavDesc.Buffer.FirstElement = 0;
        skinnedVertexUavDesc.Buffer.NumElements = UINT(vertexCount);
        skinnedVertexUavDesc.Buffer.StructureByteStride = sizeof(VertexData);
        skinnedVertexUavDesc.Buffer.CounterOffsetInBytes = 0;
        skinnedVertexUavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
        device->CreateUnorderedAccessView(
            skinCluster.skinnedVertexResource.Get(),
            nullptr,
            &skinnedVertexUavDesc,
            skinCluster.skinnedVertexUavCpu);

        skinCluster.skinningInfoResource =
            CreateBufferResource(device, sizeof(SkinningInformation));
        skinCluster.skinningInfoResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&skinCluster.mappedSkinningInfo));
        skinCluster.mappedSkinningInfo->numVertices = UINT(vertexCount);

        for (const auto& [jointName, jointWeight] : modelData.skinClusterData) {
            const auto jointIt = skeleton.jointMap.find(jointName);
            if (jointIt == skeleton.jointMap.end()) {
                continue;
            }

            const uint32_t jointIndex = static_cast<uint32_t>(jointIt->second);
            if (jointIndex >= skinCluster.inverseBindPoseMatrices.size()) {
                continue;
            }
            skinCluster.inverseBindPoseMatrices[jointIndex] =
                jointWeight.inverseBindPoseMatrix;

            for (const VertexWeightData& vertexWeight : jointWeight.vertexWeights) {
                if (vertexWeight.vertexIndex >= influences.size()) {
                    continue;
                }

                VertexInfluence& influence = influences[vertexWeight.vertexIndex];
                for (uint32_t influenceIndex = 0;
                     influenceIndex < kNumMaxInfluence;
                     ++influenceIndex) {
                    if (influence.weights[influenceIndex] == 0.0f) {
                        influence.weights[influenceIndex] = vertexWeight.weight;
                        influence.jointIndices[influenceIndex] =
                            static_cast<int32_t>(jointIndex);
                        break;
                    }
                }
            }
        }

        for (VertexInfluence& influence : influences) {
            float sum = 0.0f;
            for (float weight : influence.weights) {
                sum += weight;
            }
            if (sum <= 0.0f) {
                influence.weights[0] = 1.0f;
                influence.jointIndices[0] = 0;
                continue;
            }
            for (float& weight : influence.weights) {
                weight /= sum;
            }
        }

        UploadStaticBufferData(
            device,
            uploadCommandList,
            skinCluster.influenceResource.Get(),
            influences.data(),
            sizeof(VertexInfluence) * influences.size(),
            kStaticInfluenceReadState,
            retainedUploadResources);

        return skinCluster;
    }

    void UpdateSkinCluster(SkinCluster& skinCluster, const Skeleton& skeleton) {
        if (skinCluster.paletteEntries.empty()) {
            return;
        }

        for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex) {
            if (jointIndex >= skinCluster.inverseBindPoseMatrices.size() ||
                jointIndex >= skinCluster.paletteEntries.size()) {
                break;
            }

            Matrix4x4 skinMatrix = BuildMeshSpaceSkinningMatrix(
                skinCluster.inverseBindPoseMatrices[jointIndex],
                skeleton.joints[jointIndex].skeletonSpaceMatrix,
                skinCluster.meshRootInverseMatrix);
            skinCluster.paletteEntries[jointIndex].skeletonSpaceMatrix = skinMatrix;
            skinCluster.paletteEntries[jointIndex].skeletonSpaceInverseTransposeMatrix =
                Transpose(InverseCopy(skinMatrix));
        }
        skinCluster.paletteDirty = true;
    }

    bool LoadSkinnedModelInstance(
        SkinnedModelInstance& instance,
        ComPtr<ID3D12Device> device,
        ID3D12GraphicsCommandList* uploadCommandList,
        ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap,
        uint32_t descriptorSizeSRV,
        uint32_t skinningDescriptorBaseIndex,
        std::vector<ComPtr<ID3D12Resource>>& retainedUploadResources,
        const std::string& name,
        const std::string& directory,
        const std::string& filename,
        const Transform& transform) {
        instance = {};
        instance.name = name;
        instance.directory = directory;
        instance.filename = filename;
        instance.transform = transform;
        instance.animator.loop = true;
        instance.animator.playing = true;
        instance.animator.speed = 1.0f;

        instance.model = LoadObjFile_Assimp(directory, filename);
        instance.animation = LoadAnimationFile(directory, filename);
        if (instance.model.vertices.empty() ||
            instance.model.indices.empty() ||
            instance.model.rootNode.name.empty()) {
            OutputDebugStringA(("[AppSceneResources] Skinned model could not be loaded: " +
                directory + "/" + filename + "\n").c_str());
            return false;
        }

        instance.mesh = CreateGpuMeshResource(
            device,
            uploadCommandList,
            instance.model,
            retainedUploadResources);
        instance.skeleton = CreateSkeleton(instance.model.rootNode);
        ApplyAnimation(instance.skeleton, instance.animation, 0.0f);
        UpdateSkeleton(instance.skeleton);
        instance.skinCluster = CreateSkinCluster(
            device,
            instance.skeleton,
            instance.model,
            instance.mesh,
            uploadCommandList,
            srvDescriptorHeap,
            descriptorSizeSRV,
            skinningDescriptorBaseIndex,
            retainedUploadResources);
        UpdateSkinCluster(instance.skinCluster, instance.skeleton);

        instance.transformResource = CreateBufferResource(device, sizeof(TransformationMatrix));
        instance.transformResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&instance.transformData));
        instance.transformData->WVP = MakeIdentity4x4();
        instance.transformData->World = MakeIdentity4x4();
        instance.transformData->WorldInverseTranspose = MakeIdentity4x4();
        instance.loaded = instance.mesh.indexCount > 0 &&
            instance.skinCluster.paletteSrvGpu.ptr != 0 &&
            instance.transformData != nullptr;
        return instance.loaded;
    }

    void UpdateSkinnedModelInstance(
        SkinnedModelInstance& instance,
        float deltaTime,
        bool play,
        bool loop,
        float speed,
        float& animationTime) {
        if (!instance.loaded) {
            return;
        }

        const bool animationTimeChanged =
            std::fabs(instance.animator.time - animationTime) > 0.00001f;
        if (!play && !animationTimeChanged && !instance.skinCluster.paletteDirty) {
            instance.animator.playing = false;
            instance.animator.loop = loop;
            instance.animator.speed = speed;
            return;
        }

        instance.animator.time = animationTime;
        instance.animator.playing = play;
        instance.animator.loop = loop;
        instance.animator.speed = speed;
        if (play) {
            instance.animator.Update(deltaTime, instance.animation.duration);
            animationTime = instance.animator.time;
        }

        ApplyAnimation(instance.skeleton, instance.animation, animationTime);
        UpdateSkeleton(instance.skeleton);
        UpdateSkinCluster(instance.skinCluster, instance.skeleton);
    }

    void UpdateSkinnedModelInstanceTransform(
        SkinnedModelInstance& instance,
        const Matrix4x4& viewMatrix,
        const Matrix4x4& projMatrix) {
        if (!instance.loaded || instance.transformData == nullptr) {
            return;
        }

        Matrix4x4 baseWorld = MakeAffineMatrix(
            instance.transform.scale,
            instance.transform.rotate,
            instance.transform.translate);
        instance.transformData->World = baseWorld;
        instance.transformData->WVP = Multiply(baseWorld, Multiply(viewMatrix, projMatrix));
        instance.transformData->WorldInverseTranspose = Transpose(Inverse(baseWorld));
    }

} // namespace

ModelData BuildTrainingSwordModelDataForSubmission() {
    return BuildTrainingSwordModelData();
}

bool AppSceneResources::Initialize(
    ComPtr<ID3D12Device> device,
    ID3D12GraphicsCommandList* uploadCommandList,
    ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap,
    uint32_t descriptorSizeSRV) {
    assert(uploadCommandList != nullptr);

    // =========================================================
    // Sprite geometry
    // =========================================================
    indexResourceSprite = CreateBufferResource(device, sizeof(uint32_t) * 6);
    vertexResourceSprite = CreateBufferResource(device, sizeof(VertexData) * 6);

    transformationMatrixResourceSprite =
        CreateBufferResource(device, sizeof(TransformationMatrix));
    transformationMatrixResourceSprite->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&transformationMatrixDataSprite));
    transformationMatrixDataSprite->WVP = MakeIdentity4x4();
    transformationMatrixDataSprite->World = MakeIdentity4x4();
    transformationMatrixDataSprite->WorldInverseTranspose = MakeIdentity4x4();

    indexBufferViewSprite.BufferLocation =
        indexResourceSprite->GetGPUVirtualAddress();
    indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;
    indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;

    vertexBufferViewSprite.BufferLocation =
        vertexResourceSprite->GetGPUVirtualAddress();
    vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 6;
    vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);

    uint32_t* indexDataSprite = nullptr;
    indexResourceSprite->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&indexDataSprite));
    indexDataSprite[0] = 0;
    indexDataSprite[1] = 1;
    indexDataSprite[2] = 2;
    indexDataSprite[3] = 3;
    indexDataSprite[4] = 4;
    indexDataSprite[5] = 5;

    VertexData* vertexDataSprite = nullptr;
    vertexResourceSprite->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&vertexDataSprite));

    vertexDataSprite[0].position = { -0.5f, -0.5f, 0.0f, 1.0f };
    vertexDataSprite[0].texcoord = { 0.0f, 1.0f };

    vertexDataSprite[1].position = { -0.5f,  0.5f, 0.0f, 1.0f };
    vertexDataSprite[1].texcoord = { 0.0f, 0.0f };

    vertexDataSprite[2].position = { 0.5f, -0.5f, 0.0f, 1.0f };
    vertexDataSprite[2].texcoord = { 1.0f, 1.0f };

    vertexDataSprite[3].position = { -0.5f,  0.5f, 0.0f, 1.0f };
    vertexDataSprite[3].texcoord = { 0.0f, 0.0f };

    vertexDataSprite[4].position = { 0.5f,  0.5f, 0.0f, 1.0f };
    vertexDataSprite[4].texcoord = { 1.0f, 0.0f };

    vertexDataSprite[5].position = { 0.5f, -0.5f, 0.0f, 1.0f };
    vertexDataSprite[5].texcoord = { 1.0f, 1.0f };

    // =========================================================
    // Materials
    // =========================================================
    materialResource = CreateBufferResource(device, sizeof(Material));
    materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
    materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    materialData->enableLighting = true;
    materialData->shininess = 3.0f;
    materialData->environmentCoefficient = 0.3f;
    materialData->specularMode = 1;
    materialData->uvTransform = MakeIdentity4x4();

    terrainMaterialResource = CreateBufferResource(device, sizeof(Material));
    terrainMaterialResource->Map(0, nullptr, reinterpret_cast<void**>(&terrainMaterialData));
    terrainMaterialData->color = Vector4(1.02f, 0.96f, 0.90f, 1.0f);
    terrainMaterialData->enableLighting = true;
    terrainMaterialData->shininess = 2.0f;
    terrainMaterialData->environmentCoefficient = 0.0f;
    terrainMaterialData->specularMode = 1;
    terrainMaterialData->uvTransform = MakeIdentity4x4();

    materialResourceSprite = CreateBufferResource(device, sizeof(Material));
    materialResourceSprite->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&materialDataSprite));
    materialDataSprite->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    materialDataSprite->enableLighting = false;
    materialDataSprite->shininess = 1.0f;
    materialDataSprite->environmentCoefficient = 0.0f;
    materialDataSprite->specularMode = 1;
    materialDataSprite->uvTransform = MakeIdentity4x4();

    // =========================================================
    // Lights
    // =========================================================
    directionalLightData.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    Vector3 dir = { 0.0f, -1.0f, 0.0f };
    dir = Normalize(dir);
    directionalLightData.direction = dir;
    directionalLightData.intensity = 1.5f;

    directionalLightResource = CreateBufferResource(device, sizeof(DirectionalLight));
    directionalLightResource->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&mappedLight));
    *mappedLight = directionalLightData;

    pointLightData.color = { 1,1,1,1 };
    pointLightData.position = { 0.0f, 2.0f, 0.0f };
    pointLightData.intensity = 1.0f;
    pointLightData.radius = 10.0f;
    pointLightData.decay = 2.0f;

    pointLightResource = CreateBufferResource(device, sizeof(PointLight));
    pointLightResource->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&mappedPointLight));
    *mappedPointLight = pointLightData;

    spotLight.color = { 1,1,1,1 };
    spotLight.position = { 2.0f, 1.25f, 0.0f };
    spotLight.direction = Normalize(Vector3{ -1.0f, -1.0f, 0.0f });
    spotLight.distance = 7.0f;
    spotLight.intensity = 5.0f;
    spotLight.decay = 2.0f;
    spotLight.cosAngle = std::cos(std::numbers::pi_v<float> / 3.0f);

    spotLightResource = CreateBufferResource(device, sizeof(SpotLight));
    spotLightResource->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&mappedSpotLight));
    *mappedSpotLight = spotLight;

    // =========================================================
    // Camera
    // =========================================================
    cameraResource = CreateBufferResource(device, sizeof(CameraForGPU));
    cameraResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedCamera));
    mappedCamera->worldPosition = Vector3{ 0.0f, 0.0f, -5.0f };
    mappedCamera->padding = 0.0f;

    // =========================================================
    // Cascaded shadow maps
    // =========================================================
    cascadeShadowResource = CreateBufferResource(device, sizeof(CascadeShadowData));
    cascadeShadowResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedCascadeShadow));
    mappedCascadeShadow->cascadeSplits = Vector4(120.0f, 260.0f, 520.0f, 960.0f);
    mappedCascadeShadow->parameters = Vector4(
        0.0018f,
        0.62f,
        1.0f,
        1.0f / static_cast<float>(kCascadeShadowMapSize));

    for (uint32_t cascade = 0; cascade < kCascadeShadowCount; ++cascade) {
        cascadeShadowDrawResources[cascade] = CreateBufferResource(device, sizeof(CascadeShadowData));
        cascadeShadowDrawResources[cascade]->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&mappedCascadeShadowDraw[cascade]));
        *mappedCascadeShadowDraw[cascade] = *mappedCascadeShadow;
        mappedCascadeShadowDraw[cascade]->parameters.w = static_cast<float>(cascade);
    }

    D3D12_DESCRIPTOR_HEAP_DESC shadowDsvHeapDesc{};
    shadowDsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    shadowDsvHeapDesc.NumDescriptors = kCascadeShadowCount;
    shadowDsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    HRESULT shadowHeapHr = device->CreateDescriptorHeap(
        &shadowDsvHeapDesc,
        IID_PPV_ARGS(&cascadeShadowDsvHeap));
    assert(SUCCEEDED(shadowHeapHr));

    const uint32_t shadowDsvIncrement =
        device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    D3D12_CPU_DESCRIPTOR_HANDLE shadowDsvStart =
        cascadeShadowDsvHeap->GetCPUDescriptorHandleForHeapStart();

    for (uint32_t cascade = 0; cascade < kCascadeShadowCount; ++cascade) {
        cascadeShadowMaps[cascade] = CreateDepthShadowTexture(
            device.Get(),
            kCascadeShadowMapSize,
            kCascadeShadowMapSize,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        cascadeShadowStates[cascade] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

        cascadeShadowDsvCpu[cascade] = shadowDsvStart;
        cascadeShadowDsvCpu[cascade].ptr +=
            static_cast<SIZE_T>(shadowDsvIncrement) * cascade;

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
        device->CreateDepthStencilView(
            cascadeShadowMaps[cascade].Get(),
            &dsvDesc,
            cascadeShadowDsvCpu[cascade]);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.PlaneSlice = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

        D3D12_CPU_DESCRIPTOR_HANDLE srvCpu = AppRenderResources::GetCPUDescriptorHandle(
            srvDescriptorHeap,
            descriptorSizeSRV,
            kCascadeShadowSrvBaseIndex + cascade);
        cascadeShadowSrvGpuHandles[cascade] = AppRenderResources::GetGPUDescriptorHandle(
            srvDescriptorHeap,
            descriptorSizeSRV,
            kCascadeShadowSrvBaseIndex + cascade);
        device->CreateShaderResourceView(
            cascadeShadowMaps[cascade].Get(),
            &srvDesc,
            srvCpu);
    }
    cascadeShadowSrvGpu = AppRenderResources::GetGPUDescriptorHandle(
        srvDescriptorHeap,
        descriptorSizeSRV,
        kCascadeShadowSrvBaseIndex);
    cascadeShadowSrvTableGpu = cascadeShadowSrvGpu;

    // =========================================================
    // Texture 2譫・
    // slot 1, 2 繧剃ｽｿ逕ｨ・・lot 0 縺ｯ ImGui 逕ｨ縺ｮ蜑肴署・・
    // =========================================================
    DirectX::ScratchImage mipImages = AppRenderResources::LoadTexture("resources/monsterBall.png");
    const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
    textureResource = AppRenderResources::CreateTextureResource(device, metadata);
    AppRenderResources::UploadTextureData(
        device,
        uploadCommandList,
        textureResource,
        mipImages,
        initialUploadResources_);

    textureResource2 = textureResource;
    const DirectX::TexMetadata& metadata2 = metadata;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = metadata.format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

    textureSrvHandleCPU = AppRenderResources::GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 1);
    textureSrvHandleGPU = AppRenderResources::GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 1);
    device->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHandleCPU);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
    srvDesc2.Format = metadata2.format;
    srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

    textureSrvHandleCPU2 = AppRenderResources::GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);
    textureSrvHandleGPU2 = AppRenderResources::GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);
    device->CreateShaderResourceView(textureResource2.Get(), &srvDesc2, textureSrvHandleCPU2);

    DirectX::ScratchImage terrainAlbedoImages = CreateSolidColorTexture(184, 137, 88, 255);
    const DirectX::TexMetadata& terrainAlbedoMetadata = terrainAlbedoImages.GetMetadata();
    terrainAlbedoTextureResource = AppRenderResources::CreateTextureResource(device, terrainAlbedoMetadata);
    AppRenderResources::UploadTextureData(
        device,
        uploadCommandList,
        terrainAlbedoTextureResource,
        terrainAlbedoImages,
        initialUploadResources_);

    D3D12_SHADER_RESOURCE_VIEW_DESC terrainAlbedoSrvDesc{};
    terrainAlbedoSrvDesc.Format = terrainAlbedoMetadata.format;
    terrainAlbedoSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    terrainAlbedoSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    terrainAlbedoSrvDesc.Texture2D.MipLevels = UINT(terrainAlbedoMetadata.mipLevels);

    terrainAlbedoTextureSrvHandleCPU =
        AppRenderResources::GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 8);
    terrainAlbedoTextureSrvHandleGPU =
        AppRenderResources::GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 8);
    device->CreateShaderResourceView(
        terrainAlbedoTextureResource.Get(),
        &terrainAlbedoSrvDesc,
        terrainAlbedoTextureSrvHandleCPU);

    DirectX::ScratchImage terrainDetailCacheImages = CreateTerrainDetailCacheTexture();
    const DirectX::TexMetadata& terrainDetailCacheMetadata = terrainDetailCacheImages.GetMetadata();
    terrainDetailCacheTextureResource = AppRenderResources::CreateTextureResource(device, terrainDetailCacheMetadata);
    AppRenderResources::UploadTextureData(
        device,
        uploadCommandList,
        terrainDetailCacheTextureResource,
        terrainDetailCacheImages,
        initialUploadResources_);

    D3D12_SHADER_RESOURCE_VIEW_DESC terrainDetailCacheSrvDesc{};
    terrainDetailCacheSrvDesc.Format = terrainDetailCacheMetadata.format;
    terrainDetailCacheSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    terrainDetailCacheSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    terrainDetailCacheSrvDesc.Texture2DArray.MipLevels = UINT(terrainDetailCacheMetadata.mipLevels);
    terrainDetailCacheSrvDesc.Texture2DArray.ArraySize = UINT(terrainDetailCacheMetadata.arraySize);

    terrainDetailCacheTextureSrvHandleCPU =
        AppRenderResources::GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 9);
    terrainDetailCacheTextureSrvHandleGPU =
        AppRenderResources::GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 9);
    device->CreateShaderResourceView(
        terrainDetailCacheTextureResource.Get(),
        &terrainDetailCacheSrvDesc,
        terrainDetailCacheTextureSrvHandleCPU);

    DirectX::ScratchImage terrainDetailNormalMapImages;
    std::string terrainDetailNormalMapSourcePath;
    const bool loadedExternalTerrainDetailNormalMap =
        TryLoadTerrainDetailNormalMapAsset(terrainDetailNormalMapImages, terrainDetailNormalMapSourcePath);
    if (!loadedExternalTerrainDetailNormalMap) {
        terrainDetailNormalMapImages = CreateTerrainDetailNormalMapTexture();
        terrainDetailNormalMapSourcePath = "generated://terrain-detail-normal-map";
    }
    {
        const std::string message =
            std::string("[AppSceneResources] Terrain detail normal map: ") +
            terrainDetailNormalMapSourcePath +
            (loadedExternalTerrainDetailNormalMap ? "\n" : " (procedural fallback)\n");
        OutputDebugStringA(message.c_str());
    }
    const DirectX::TexMetadata& terrainDetailNormalMapMetadata = terrainDetailNormalMapImages.GetMetadata();
    terrainDetailNormalMapTextureResource = AppRenderResources::CreateTextureResource(device, terrainDetailNormalMapMetadata);
    AppRenderResources::UploadTextureData(
        device,
        uploadCommandList,
        terrainDetailNormalMapTextureResource,
        terrainDetailNormalMapImages,
        initialUploadResources_);

    D3D12_SHADER_RESOURCE_VIEW_DESC terrainDetailNormalMapSrvDesc{};
    terrainDetailNormalMapSrvDesc.Format = terrainDetailNormalMapMetadata.format;
    terrainDetailNormalMapSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    terrainDetailNormalMapSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    terrainDetailNormalMapSrvDesc.Texture2DArray.MipLevels = UINT(terrainDetailNormalMapMetadata.mipLevels);
    terrainDetailNormalMapSrvDesc.Texture2DArray.ArraySize = UINT(terrainDetailNormalMapMetadata.arraySize);

    terrainDetailNormalMapTextureSrvHandleCPU =
        AppRenderResources::GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 10);
    terrainDetailNormalMapTextureSrvHandleGPU =
        AppRenderResources::GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 10);
    device->CreateShaderResourceView(
        terrainDetailNormalMapTextureResource.Get(),
        &terrainDetailNormalMapSrvDesc,
        terrainDetailNormalMapTextureSrvHandleCPU);

    DirectX::ScratchImage flatNormalImages =
        CreateSolidColorTexture(128, 128, 255, 255);
    const DirectX::TexMetadata& flatNormalMetadata = flatNormalImages.GetMetadata();
    flatNormalTextureResource =
        AppRenderResources::CreateTextureResource(device, flatNormalMetadata);
    AppRenderResources::UploadTextureData(
        device,
        uploadCommandList,
        flatNormalTextureResource,
        flatNormalImages,
        initialUploadResources_);
    D3D12_SHADER_RESOURCE_VIEW_DESC flatNormalSrvDesc{};
    flatNormalSrvDesc.Format = flatNormalMetadata.format;
    flatNormalSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    flatNormalSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    flatNormalSrvDesc.Texture2D.MipLevels = UINT(flatNormalMetadata.mipLevels);
    flatNormalTextureSrvHandleCPU =
        AppRenderResources::GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 11);
    flatNormalTextureSrvHandleGPU =
        AppRenderResources::GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 11);
    device->CreateShaderResourceView(
        flatNormalTextureResource.Get(),
        &flatNormalSrvDesc,
        flatNormalTextureSrvHandleCPU);

    const std::string circle2TexturePath =
        std::filesystem::exists("Resources/circle2.png") ? "Resources/circle2.png" : "resources/monsterBall.png";
    DirectX::ScratchImage circle2Images = AppRenderResources::LoadTexture(circle2TexturePath);
    const DirectX::TexMetadata& circle2Metadata = circle2Images.GetMetadata();
    circle2TextureResource = AppRenderResources::CreateTextureResource(device, circle2Metadata);
    AppRenderResources::UploadTextureData(
        device,
        uploadCommandList,
        circle2TextureResource,
        circle2Images,
        initialUploadResources_);

    D3D12_SHADER_RESOURCE_VIEW_DESC circle2SrvDesc{};
    circle2SrvDesc.Format = circle2Metadata.format;
    circle2SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    circle2SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    circle2SrvDesc.Texture2D.MipLevels = UINT(circle2Metadata.mipLevels);

    circle2TextureSrvHandleCPU = AppRenderResources::GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 5);
    circle2TextureSrvHandleGPU = AppRenderResources::GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 5);
    device->CreateShaderResourceView(circle2TextureResource.Get(), &circle2SrvDesc, circle2TextureSrvHandleCPU);

    const std::string gradationLineTexturePath =
        std::filesystem::exists("Resources/gradationLine.png") ? "Resources/gradationLine.png" : circle2TexturePath;
    DirectX::ScratchImage gradationLineImages = AppRenderResources::LoadTexture(gradationLineTexturePath);
    const DirectX::TexMetadata& gradationLineMetadata = gradationLineImages.GetMetadata();
    gradationLineTextureResource = AppRenderResources::CreateTextureResource(device, gradationLineMetadata);
    AppRenderResources::UploadTextureData(
        device,
        uploadCommandList,
        gradationLineTextureResource,
        gradationLineImages,
        initialUploadResources_);

    D3D12_SHADER_RESOURCE_VIEW_DESC gradationLineSrvDesc{};
    gradationLineSrvDesc.Format = gradationLineMetadata.format;
    gradationLineSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    gradationLineSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    gradationLineSrvDesc.Texture2D.MipLevels = UINT(gradationLineMetadata.mipLevels);

    gradationLineTextureSrvHandleCPU = AppRenderResources::GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 6);
    gradationLineTextureSrvHandleGPU = AppRenderResources::GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 6);
    device->CreateShaderResourceView(
        gradationLineTextureResource.Get(),
        &gradationLineSrvDesc,
        gradationLineTextureSrvHandleCPU);

    const std::string skyboxTexturePath = "Resources/rostock_laage_airport_4k.dds";
    if (std::filesystem::exists(skyboxTexturePath)) {
        DirectX::ScratchImage skyboxImages = AppRenderResources::LoadTexture(skyboxTexturePath);
        const DirectX::TexMetadata& skyboxMetadata = skyboxImages.GetMetadata();
        if (skyboxMetadata.IsCubemap()) {
            skyboxTextureResource = AppRenderResources::CreateTextureResource(device, skyboxMetadata);
            AppRenderResources::UploadTextureData(
                device,
                uploadCommandList,
                skyboxTextureResource,
                skyboxImages,
                initialUploadResources_);

            D3D12_SHADER_RESOURCE_VIEW_DESC skyboxSrvDesc{};
            skyboxSrvDesc.Format = skyboxMetadata.format;
            skyboxSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            skyboxSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            skyboxSrvDesc.TextureCube.MostDetailedMip = 0;
            skyboxSrvDesc.TextureCube.MipLevels = UINT(skyboxMetadata.mipLevels);
            skyboxSrvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

            skyboxTextureSrvHandleCPU = AppRenderResources::GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 3);
            skyboxTextureSrvHandleGPU = AppRenderResources::GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 3);
            device->CreateShaderResourceView(skyboxTextureResource.Get(), &skyboxSrvDesc, skyboxTextureSrvHandleCPU);
        } else {
            OutputDebugStringA("[AppSceneResources] Skybox texture exists but is not a cubemap DDS.\n");
        }
    } else {
        OutputDebugStringA("[AppSceneResources] Skybox DDS not found. Skybox pass will be skipped.\n");
    }

    const std::string animatedCubeTexturePath =
        std::filesystem::exists("Resources/AnimatedCube/AnimatedCube_BaseColor.png")
            ? "Resources/AnimatedCube/AnimatedCube_BaseColor.png"
            : "resources/monsterBall.png";
    DirectX::ScratchImage animatedCubeImages =
        AppRenderResources::LoadTexture(animatedCubeTexturePath);
    const DirectX::TexMetadata& animatedCubeMetadata = animatedCubeImages.GetMetadata();
    animatedCubeTextureResource =
        AppRenderResources::CreateTextureResource(device, animatedCubeMetadata);
    AppRenderResources::UploadTextureData(
        device,
        uploadCommandList,
        animatedCubeTextureResource,
        animatedCubeImages,
        initialUploadResources_);

    D3D12_SHADER_RESOURCE_VIEW_DESC animatedCubeSrvDesc{};
    animatedCubeSrvDesc.Format = animatedCubeMetadata.format;
    animatedCubeSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    animatedCubeSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    animatedCubeSrvDesc.Texture2D.MipLevels = UINT(animatedCubeMetadata.mipLevels);

    animatedCubeTextureSrvHandleCPU =
        AppRenderResources::GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 7);
    animatedCubeTextureSrvHandleGPU =
        AppRenderResources::GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 7);
    device->CreateShaderResourceView(
        animatedCubeTextureResource.Get(),
        &animatedCubeSrvDesc,
        animatedCubeTextureSrvHandleCPU);

    vfxTextureLibrary.clear();
    auto registerExistingVfxTexture = [this](
        std::string name,
        std::string path,
        ComPtr<ID3D12Resource> resource,
        D3D12_CPU_DESCRIPTOR_HANDLE cpu,
        D3D12_GPU_DESCRIPTOR_HANDLE gpu,
        uint32_t descriptorIndex,
        const DirectX::TexMetadata& textureMetadata) {
        if (resource == nullptr || gpu.ptr == 0) {
            return;
        }
        vfxTextureLibrary.push_back({
            std::move(name),
            std::move(path),
            resource,
            cpu,
            gpu,
            descriptorIndex,
            static_cast<uint32_t>(textureMetadata.width),
            static_cast<uint32_t>(textureMetadata.height)
        });
    };

    registerExistingVfxTexture(
        "default",
        "Resources/monsterBall.png",
        textureResource,
        textureSrvHandleCPU,
        textureSrvHandleGPU,
        1,
        metadata);
    registerExistingVfxTexture(
        "monsterBall",
        "Resources/monsterBall.png",
        textureResource2,
        textureSrvHandleCPU2,
        textureSrvHandleGPU2,
        2,
        metadata2);
    registerExistingVfxTexture(
        "circle2",
        circle2TexturePath,
        circle2TextureResource,
        circle2TextureSrvHandleCPU,
        circle2TextureSrvHandleGPU,
        5,
        circle2Metadata);
    registerExistingVfxTexture(
        "gradationLine",
        gradationLineTexturePath,
        gradationLineTextureResource,
        gradationLineTextureSrvHandleCPU,
        gradationLineTextureSrvHandleGPU,
        6,
        gradationLineMetadata);
    registerExistingVfxTexture(
        "terrainFlatRock",
        "generated://terrain-flat-rock",
        terrainAlbedoTextureResource,
        terrainAlbedoTextureSrvHandleCPU,
        terrainAlbedoTextureSrvHandleGPU,
        8,
        terrainAlbedoMetadata);
    registerExistingVfxTexture(
        "terrainDetailCache",
        "generated://terrain-detail-cache",
        terrainDetailCacheTextureResource,
        terrainDetailCacheTextureSrvHandleCPU,
        terrainDetailCacheTextureSrvHandleGPU,
        9,
        terrainDetailCacheMetadata);
    registerExistingVfxTexture(
        "terrainDetailNormalMap",
        terrainDetailNormalMapSourcePath,
        terrainDetailNormalMapTextureResource,
        terrainDetailNormalMapTextureSrvHandleCPU,
        terrainDetailNormalMapTextureSrvHandleGPU,
        10,
        terrainDetailNormalMapMetadata);
    registerExistingVfxTexture(
        "flatNormal",
        "generated://flat-normal",
        flatNormalTextureResource,
        flatNormalTextureSrvHandleCPU,
        flatNormalTextureSrvHandleGPU,
        11,
        flatNormalMetadata);

    struct VfxTextureLoadSpec {
        const char* name;
        const char* path;
    };
    constexpr uint32_t kVfxTextureDescriptorBaseIndex = 128;
    const VfxTextureLoadSpec vfxTextureLoadSpecs[] = {
        {"streakNoise", "Resources/streakNoise.png"},
        {"circle", "Resources/circle.png"},
        {"beamRamp_lightning", "Resources/beamRamp_lightning.png"},
        {"uvChecker", "Resources/uvChecker.png"},
        {"fence", "Resources/fence/fence.png"},
        {"iceShard", "Resources/iceShard.png"},
        {"courseOrganicRock", "Resources/course_meshes/materials/organic_rock_albedo.bmp"},
        {"courseRibRock", "Resources/course_meshes/materials/rib_rock_albedo.bmp"},
        {"courseRootRock", "Resources/course_meshes/materials/root_rock_albedo.bmp"},
        {"courseVistaRock", "Resources/course_meshes/materials/vista_rock_albedo.bmp"},
    };

    for (uint32_t index = 0; index < _countof(vfxTextureLoadSpecs); ++index) {
        const VfxTextureLoadSpec& spec = vfxTextureLoadSpecs[index];
        if (!std::filesystem::exists(spec.path)) {
            continue;
        }

        DirectX::ScratchImage vfxTextureImages = AppRenderResources::LoadTexture(spec.path);
        const DirectX::TexMetadata& vfxTextureMetadata = vfxTextureImages.GetMetadata();
        ComPtr<ID3D12Resource> vfxTextureResource =
            AppRenderResources::CreateTextureResource(device, vfxTextureMetadata);
        AppRenderResources::UploadTextureData(
            device,
            uploadCommandList,
            vfxTextureResource,
            vfxTextureImages,
            initialUploadResources_);

        D3D12_SHADER_RESOURCE_VIEW_DESC vfxTextureSrvDesc{};
        vfxTextureSrvDesc.Format = vfxTextureMetadata.format;
        vfxTextureSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        vfxTextureSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        vfxTextureSrvDesc.Texture2D.MipLevels = UINT(vfxTextureMetadata.mipLevels);

        const uint32_t descriptorIndex = kVfxTextureDescriptorBaseIndex + index;
        D3D12_CPU_DESCRIPTOR_HANDLE cpu =
            AppRenderResources::GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, descriptorIndex);
        D3D12_GPU_DESCRIPTOR_HANDLE gpu =
            AppRenderResources::GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, descriptorIndex);
        device->CreateShaderResourceView(vfxTextureResource.Get(), &vfxTextureSrvDesc, cpu);

        vfxTextureLibrary.push_back({
            spec.name,
            spec.path,
            vfxTextureResource,
            cpu,
            gpu,
            descriptorIndex,
            static_cast<uint32_t>(vfxTextureMetadata.width),
            static_cast<uint32_t>(vfxTextureMetadata.height)
        });
    }

    uint32_t nextMaterialTextureDescriptorIndex =
        kMaterialTextureSrvBaseIndex;
    auto findTextureByPath = [&](const std::string& path)
        -> const AppManagedTextureResource* {
        const std::string key = NormalizeResourcePathKey(path);
        if (key.empty()) {
            return nullptr;
        }
        for (const AppManagedTextureResource& texture : vfxTextureLibrary) {
            if (NormalizeResourcePathKey(texture.path) == key) {
                return &texture;
            }
        }
        return nullptr;
    };
    auto resolveMaterialTexture = [&](
        const std::string& path,
        D3D12_GPU_DESCRIPTOR_HANDLE fallback,
        bool& usedFallback) {
        usedFallback = true;
        if (path.empty()) {
            return fallback;
        }
        if (const AppManagedTextureResource* existing = findTextureByPath(path)) {
            usedFallback = false;
            return existing->gpu;
        }
        if (!std::filesystem::exists(path) ||
            nextMaterialTextureDescriptorIndex >=
                kMaterialTextureSrvBaseIndex + kMaterialTextureSrvCount) {
            return fallback;
        }

        DirectX::ScratchImage images = AppRenderResources::LoadTexture(path);
        const DirectX::TexMetadata& metadata = images.GetMetadata();
        ComPtr<ID3D12Resource> resource =
            AppRenderResources::CreateTextureResource(device, metadata);
        AppRenderResources::UploadTextureData(
            device,
            uploadCommandList,
            resource,
            images,
            initialUploadResources_);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDescription{};
        srvDescription.Format = metadata.format;
        srvDescription.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDescription.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDescription.Texture2D.MipLevels = UINT(metadata.mipLevels);
        const uint32_t descriptorIndex = nextMaterialTextureDescriptorIndex++;
        const D3D12_CPU_DESCRIPTOR_HANDLE cpu =
            AppRenderResources::GetCPUDescriptorHandle(
                srvDescriptorHeap,
                descriptorSizeSRV,
                descriptorIndex);
        const D3D12_GPU_DESCRIPTOR_HANDLE gpu =
            AppRenderResources::GetGPUDescriptorHandle(
                srvDescriptorHeap,
                descriptorSizeSRV,
                descriptorIndex);
        device->CreateShaderResourceView(resource.Get(), &srvDescription, cpu);
        vfxTextureLibrary.push_back({
            "material_texture_" + std::to_string(descriptorIndex),
            path,
            resource,
            cpu,
            gpu,
            descriptorIndex,
            static_cast<uint32_t>(metadata.width),
            static_cast<uint32_t>(metadata.height),
        });
        usedFallback = false;
        return gpu;
    };
    auto buildGpuMaterials = [&](
        ModelData& sourceModel,
        D3D12_GPU_DESCRIPTOR_HANDLE fallbackAlbedo) {
        EnsureModelDataMaterialLayout(sourceModel);
        std::vector<AppGpuMaterialResource> result;
        result.reserve(sourceModel.materials.size());
        for (const MaterialData& sourceMaterial : sourceModel.materials) {
            AppGpuMaterialResource gpuMaterial;
            gpuMaterial.source = sourceMaterial;
            gpuMaterial.constantBuffer = CreateBufferResource(device, sizeof(Material));
            gpuMaterial.constantBuffer->Map(
                0,
                nullptr,
                reinterpret_cast<void**>(&gpuMaterial.mappedConstants));
            *gpuMaterial.mappedConstants = *materialData;
            gpuMaterial.mappedConstants->color = {
                materialData->color.x * sourceMaterial.baseColorFactor.x,
                materialData->color.y * sourceMaterial.baseColorFactor.y,
                materialData->color.z * sourceMaterial.baseColorFactor.z,
                materialData->color.w * sourceMaterial.baseColorFactor.w,
            };
            gpuMaterial.albedoTextureGpu = resolveMaterialTexture(
                sourceMaterial.textureFilePath,
                fallbackAlbedo,
                gpuMaterial.albedoFallback);
            gpuMaterial.normalTextureGpu = resolveMaterialTexture(
                sourceMaterial.normalTextureFilePath,
                flatNormalTextureSrvHandleGPU,
                gpuMaterial.normalFallback);
            result.push_back(std::move(gpuMaterial));
        }
        return result;
    };

    // =========================================================
    // Sphere mesh
    // =========================================================
    {
        const uint32_t sphereStacks = 32;
        const uint32_t sphereSlices = 64;
        std::vector<SphereVertex> sphereVerts =
            BuildSphereVertices(sphereStacks, sphereSlices);

        sphere.vertexCount = (UINT)sphereVerts.size();
        sphere.vertexResource =
            CreateBufferResource(device, sizeof(SphereVertex) * sphere.vertexCount);

        SphereVertex* mappedVB = nullptr;
        sphere.vertexResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&mappedVB));
        memcpy(mappedVB, sphereVerts.data(), sizeof(SphereVertex) * sphere.vertexCount);

        sphere.vbv.BufferLocation = sphere.vertexResource->GetGPUVirtualAddress();
        sphere.vbv.SizeInBytes = (UINT)(sizeof(SphereVertex) * sphere.vertexCount);
        sphere.vbv.StrideInBytes = sizeof(SphereVertex);

        sphere.cbvResource = CreateBufferResource(device, sizeof(TransformationMatrix));
        sphere.cbvResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&sphere.mappedCBV));
        sphere.mappedCBV->WVP = MakeIdentity4x4();
        sphere.mappedCBV->World = MakeIdentity4x4();
        sphere.mappedCBV->WorldInverseTranspose = MakeIdentity4x4();
    }

    // =========================================================
    // Skybox mesh
    // =========================================================
    {
        const std::vector<SkyboxVertex> skyboxVerts = BuildSkyboxVertices();
        skybox.vertexCount = UINT(skyboxVerts.size());
        skybox.vertexResource =
            CreateBufferResource(device, sizeof(SkyboxVertex) * skybox.vertexCount);

        SkyboxVertex* mappedVB = nullptr;
        skybox.vertexResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&mappedVB));
        memcpy(mappedVB, skyboxVerts.data(), sizeof(SkyboxVertex) * skybox.vertexCount);

        skybox.vbv.BufferLocation = skybox.vertexResource->GetGPUVirtualAddress();
        skybox.vbv.SizeInBytes = UINT(sizeof(SkyboxVertex) * skybox.vertexCount);
        skybox.vbv.StrideInBytes = sizeof(SkyboxVertex);

        skybox.cbvResource = CreateBufferResource(device, sizeof(TransformationMatrix));
        skybox.cbvResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&skybox.mappedCBV));
        skybox.mappedCBV->WVP = MakeIdentity4x4();
        skybox.mappedCBV->World = MakeIdentity4x4();
        skybox.mappedCBV->WorldInverseTranspose = MakeIdentity4x4();
    }

    // =========================================================
    // VFX Ring mesh
    // =========================================================
    {
        const std::vector<RingVertex> ringVerts = BuildRingVertices(128);
        ring.vertexCount = UINT(ringVerts.size());
        ring.vertexResource =
            CreateBufferResource(device, sizeof(RingVertex) * ring.vertexCount);

        RingVertex* mappedVB = nullptr;
        ring.vertexResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&mappedVB));
        memcpy(mappedVB, ringVerts.data(), sizeof(RingVertex) * ring.vertexCount);

        ring.vbv.BufferLocation = ring.vertexResource->GetGPUVirtualAddress();
        ring.vbv.SizeInBytes = UINT(sizeof(RingVertex) * ring.vertexCount);
        ring.vbv.StrideInBytes = sizeof(RingVertex);
    }

    // =========================================================
    // VFX Cylinder mesh
    // =========================================================
    {
        const std::vector<CylinderVertex> cylinderVerts = BuildCylinderVertices(128);
        cylinder.vertexCount = UINT(cylinderVerts.size());
        cylinder.vertexResource =
            CreateBufferResource(device, sizeof(CylinderVertex) * cylinder.vertexCount);

        CylinderVertex* mappedVB = nullptr;
        cylinder.vertexResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&mappedVB));
        memcpy(mappedVB, cylinderVerts.data(), sizeof(CylinderVertex) * cylinder.vertexCount);

        cylinder.vbv.BufferLocation = cylinder.vertexResource->GetGPUVirtualAddress();
        cylinder.vbv.SizeInBytes = UINT(sizeof(CylinderVertex) * cylinder.vertexCount);
        cylinder.vbv.StrideInBytes = sizeof(CylinderVertex);
    }

    // =========================================================
    // VFX Spear mesh
    // =========================================================
    {
        const std::vector<SpearVertex> spearVerts = BuildSpearVertices();
        spear.vertexCount = UINT(spearVerts.size());
        spear.vertexResource =
            CreateBufferResource(device, sizeof(SpearVertex) * spear.vertexCount);

        SpearVertex* mappedVB = nullptr;
        spear.vertexResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&mappedVB));
        memcpy(mappedVB, spearVerts.data(), sizeof(SpearVertex) * spear.vertexCount);

        spear.vbv.BufferLocation = spear.vertexResource->GetGPUVirtualAddress();
        spear.vbv.SizeInBytes = UINT(sizeof(SpearVertex) * spear.vertexCount);
        spear.vbv.StrideInBytes = sizeof(SpearVertex);
    }

    // =========================================================
    // VFX Orbit ribbon mesh
    // =========================================================
    {
        const std::vector<OrbitRibbonVertex> orbitRibbonVerts = BuildOrbitRibbonVertices();
        orbitRibbon.vertexCount = UINT(orbitRibbonVerts.size());
        orbitRibbon.vertexResource =
            CreateBufferResource(device, sizeof(OrbitRibbonVertex) * orbitRibbon.vertexCount);

        OrbitRibbonVertex* mappedVB = nullptr;
        orbitRibbon.vertexResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&mappedVB));
        memcpy(mappedVB, orbitRibbonVerts.data(), sizeof(OrbitRibbonVertex) * orbitRibbon.vertexCount);

        orbitRibbon.vbv.BufferLocation = orbitRibbon.vertexResource->GetGPUVirtualAddress();
        orbitRibbon.vbv.SizeInBytes = UINT(sizeof(OrbitRibbonVertex) * orbitRibbon.vertexCount);
        orbitRibbon.vbv.StrideInBytes = sizeof(OrbitRibbonVertex);
    }

    // =========================================================
    // Assimp model mesh
    // =========================================================
    modelData = LoadObjFile_Assimp("Resources/ball", "ball.obj");
    assert(!modelData.vertices.empty());
    assert(!modelData.indices.empty());
    modelMesh = CreateGpuMeshResource(
        device,
        uploadCommandList,
        modelData,
        initialUploadResources_);

    // =========================================================
    // AnimatedCube model and animation
    // =========================================================
    animatedCubeData = LoadObjFile_Assimp("Resources/AnimatedCube", "AnimatedCube.gltf");
    animatedCubeAnimation = LoadAnimationFile("Resources/AnimatedCube", "AnimatedCube.gltf");
    if (!animatedCubeData.vertices.empty() && !animatedCubeData.indices.empty()) {
        animatedCubeMesh = CreateGpuMeshResource(
            device,
            uploadCommandList,
            animatedCubeData,
            initialUploadResources_);

        animatedCubeTransformResource = CreateBufferResource(device, sizeof(TransformationMatrix));
        animatedCubeTransformResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&animatedCubeTransformData));
        animatedCubeTransformData->WVP = MakeIdentity4x4();
        animatedCubeTransformData->World = MakeIdentity4x4();
        animatedCubeTransformData->WorldInverseTranspose = MakeIdentity4x4();

    } else {
        OutputDebugStringA("[AppSceneResources] AnimatedCube model has no indexed mesh data.\n");
    }

    // =========================================================
    // Managed VFX model library and object instances
    // =========================================================
    vfxModelLibrary.clear();
    if (modelMesh.indexCount > 0) {
        vfxModelLibrary.push_back({
            "ball",
            "Resources/ball",
            "ball.obj",
            modelData,
            modelMesh,
            textureSrvHandleGPU2,
            true,
        });
    }
    if (animatedCubeMesh.indexCount > 0) {
        vfxModelLibrary.push_back({
            "animated_cube",
            "Resources/AnimatedCube",
            "AnimatedCube.gltf",
            animatedCubeData,
            animatedCubeMesh,
            animatedCubeTextureSrvHandleGPU,
            animatedCubeTextureSrvHandleGPU.ptr != 0,
        });
    }
    auto findManagedTextureGpu = [&](const char* textureName) {
        for (const AppManagedTextureResource& texture : vfxTextureLibrary) {
            if (texture.name == textureName && texture.gpu.ptr != 0) {
                return texture.gpu;
            }
        }
        return terrainAlbedoTextureSrvHandleGPU.ptr != 0 ? terrainAlbedoTextureSrvHandleGPU : textureSrvHandleGPU2;
    };
    auto registerCourseMesh = [&](const char* name, const char* directory, const char* filename, const char* textureName) {
        ModelData courseMeshData = LoadObjFile_Assimp(directory, filename);
        if (courseMeshData.vertices.empty() || courseMeshData.indices.empty()) {
            OutputDebugStringA(("[AppSceneResources] Course mesh has no indexed data: " + std::string(name) + "\n").c_str());
            return;
        }

        GpuMeshResource courseMesh = CreateGpuMeshResource(
            device,
            uploadCommandList,
            courseMeshData,
            initialUploadResources_);
        if (courseMesh.indexCount == 0) {
            OutputDebugStringA(("[AppSceneResources] Course mesh GPU upload failed: " + std::string(name) + "\n").c_str());
            return;
        }

        vfxModelLibrary.push_back({
            name,
            directory,
            filename,
            std::move(courseMeshData),
            courseMesh,
            findManagedTextureGpu(textureName),
            findManagedTextureGpu(textureName).ptr != 0,
        });
    };
    registerCourseMesh("organic_arch_large", "Resources/course_meshes/OrganicArchLarge", "OrganicArchLarge.obj", "courseOrganicRock");
    registerCourseMesh("rib_tunnel_wall", "Resources/course_meshes/RibTunnelWall", "RibTunnelWall.obj", "courseRibRock");
    registerCourseMesh("root_spire_column", "Resources/course_meshes/RootSpireColumn", "RootSpireColumn.obj", "courseRootRock");
    registerCourseMesh("curved_canyon_wall", "Resources/course_meshes/CurvedCanyonWall", "CurvedCanyonWall.obj", "courseOrganicRock");
    registerCourseMesh("vista_hole_wall", "Resources/course_meshes/VistaHoleWall", "VistaHoleWall.obj", "courseVistaRock");
    registerCourseMesh("spire_broken_bridge_arc", "Resources/course_meshes/SpireBrokenBridgeArc", "SpireBrokenBridgeArc.obj", "courseRootRock");
    registerCourseMesh(
        "multi_material_demo",
        "Resources/tests/MultiMaterial",
        "MultiMaterial.obj",
        "default");

    trainingSwordModelIndex = UINT32_MAX;
    ModelData trainingSwordData = BuildTrainingSwordModelDataForSubmission();
    GpuMeshResource trainingSwordMesh = CreateGpuMeshResource(
        device,
        uploadCommandList,
        trainingSwordData,
        initialUploadResources_);
    const D3D12_GPU_DESCRIPTOR_HANDLE trainingSwordTexture =
        findManagedTextureGpu("default");
    if (trainingSwordMesh.indexCount > 0 && trainingSwordTexture.ptr != 0) {
        trainingSwordModelIndex = static_cast<uint32_t>(vfxModelLibrary.size());
        vfxModelLibrary.push_back({
            "training_sword",
            "<procedural>",
            "training_sword",
            std::move(trainingSwordData),
            trainingSwordMesh,
            trainingSwordTexture,
            true,
        });
    } else {
        OutputDebugStringA("[AppSceneResources] Training sword resource creation failed.\n");
    }

    for (AppManagedModelResource& managedModel : vfxModelLibrary) {
        managedModel.gpuMaterials = buildGpuMaterials(
            managedModel.model,
            managedModel.textureGpu.ptr != 0
                ? managedModel.textureGpu
                : textureSrvHandleGPU2);
        managedModel.loaded = managedModel.loaded &&
            ValidateModelDataMaterialLayout(managedModel.model) &&
            !managedModel.gpuMaterials.empty();
    }

    vfxModelObjects.clear();
    vfxModelObjects.resize(kRuntimeVfxModelObjectCount);
    for (size_t index = 0; index < vfxModelObjects.size(); ++index) {
        AppModelObjectInstance& object = vfxModelObjects[index];
        object.name = "vfx_model_object_" + std::to_string(index);
        object.modelIndex = vfxModelLibrary.empty()
            ? 0
            : static_cast<uint32_t>(index % vfxModelLibrary.size());
        object.visible = true;
        object.transformResource = CreateBufferResource(device, sizeof(TransformationMatrix));
        object.transformResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&object.transformData));
        object.transformData->WVP = MakeIdentity4x4();
        object.transformData->World = MakeIdentity4x4();
        object.transformData->WorldInverseTranspose = MakeIdentity4x4();
    }

    weaponAttachmentObject = {};
    weaponAttachmentObject.name = "weapon_attachment";
    weaponAttachmentObject.modelIndex = trainingSwordModelIndex;
    weaponAttachmentObject.visible = false;
    weaponAttachmentObject.transformResource =
        CreateBufferResource(device, sizeof(TransformationMatrix));
    weaponAttachmentObject.transformResource->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&weaponAttachmentObject.transformData));
    weaponAttachmentObject.transformData->WVP = MakeIdentity4x4();
    weaponAttachmentObject.transformData->World = MakeIdentity4x4();
    weaponAttachmentObject.transformData->WorldInverseTranspose = MakeIdentity4x4();

    // =========================================================
    // Skinned model instances
    // =========================================================
    const Transform skinnedDefaultTransform{
        { 0.45f, 0.45f, 0.45f },
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, -0.4f, -6.3f },
    };
    LoadSkinnedModelInstance(
        skinnedModels[0],
        device,
        uploadCommandList,
        srvDescriptorHeap,
        descriptorSizeSRV,
        GetSkinningDescriptorBaseIndex(0),
        initialUploadResources_,
        "simpleSkin",
        "Resources/simpleSkin",
        "simpleSkin.gltf",
        skinnedDefaultTransform);
    LoadSkinnedModelInstance(
        skinnedModels[1],
        device,
        uploadCommandList,
        srvDescriptorHeap,
        descriptorSizeSRV,
        GetSkinningDescriptorBaseIndex(1),
        initialUploadResources_,
        "human walk",
        "Resources/human",
        "walk_gltf.gltf",
        skinnedDefaultTransform);
    LoadSkinnedModelInstance(
        skinnedModels[2],
        device,
        uploadCommandList,
        srvDescriptorHeap,
        descriptorSizeSRV,
        GetSkinningDescriptorBaseIndex(2),
        initialUploadResources_,
        "human sneakWalk",
        "Resources/human",
        "sneakWalk.gltf",
        skinnedDefaultTransform);
    for (SkinnedModelInstance& skinnedModel : skinnedModels) {
        if (!skinnedModel.loaded) {
            continue;
        }
        skinnedModel.gpuMaterials = buildGpuMaterials(
            skinnedModel.model,
            textureSrvHandleGPU);
        skinnedModel.loaded = skinnedModel.loaded &&
            ValidateModelDataMaterialLayout(skinnedModel.model) &&
            !skinnedModel.gpuMaterials.empty();
    }

    size_t maxJointCount = 0;
    for (const SkinnedModelInstance& instance : skinnedModels) {
        if (instance.loaded) {
            maxJointCount = (std::max)(maxJointCount, instance.skeleton.joints.size());
        }
    }
    if (maxJointCount > 0) {
        const size_t hierarchyLineVertices = maxJointCount > 0 ? (maxJointCount - 1) * 2 : 0;
        const size_t fallbackAnimatedJointLineVertices = maxJointCount > 0 ? (maxJointCount - 1) * 2 : 0;
        const size_t jointMarkerVertices = maxJointCount * 6;
        skeletonDebugVertexCapacity =
            UINT(hierarchyLineVertices + fallbackAnimatedJointLineVertices + jointMarkerVertices);
        skeletonDebugVertexResource = CreateBufferResource(
            device,
            sizeof(SkeletonDebugLineVertex) * skeletonDebugVertexCapacity);
        skeletonDebugVertexResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&mappedSkeletonDebugLines));
        skeletonDebugVBV.BufferLocation = skeletonDebugVertexResource->GetGPUVirtualAddress();
        skeletonDebugVBV.SizeInBytes =
            UINT(sizeof(SkeletonDebugLineVertex) * skeletonDebugVertexCapacity);
        skeletonDebugVBV.StrideInBytes = sizeof(SkeletonDebugLineVertex);

        skeletonDebugTransformResource =
            CreateBufferResource(device, sizeof(TransformationMatrix));
        skeletonDebugTransformResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&skeletonDebugTransformData));
        skeletonDebugTransformData->WVP = MakeIdentity4x4();
        skeletonDebugTransformData->World = MakeIdentity4x4();
        skeletonDebugTransformData->WorldInverseTranspose = MakeIdentity4x4();
    }

    if (!debugDraw.Initialize(device, 65536)) {
        OutputDebugStringA("[AppSceneResources] DebugDraw initialization failed.\n");
    }
    if (!courseMeshRenderQueue.Initialize(device, 256)) {
        OutputDebugStringA("[AppSceneResources] CourseMeshRenderQueue initialization failed.\n");
        return false;
    }

    return true;
}

void AppSceneResources::ReleaseInitialUploadResources() {
    initialUploadResources_.clear();
}

void AppSceneResources::UpdateCameraWorldPosition(const Vector3& worldPosition) {
    if (mappedCamera == nullptr) {
        return;
    }

    mappedCamera->worldPosition = worldPosition;
}

SkinnedModelInstance* AppSceneResources::GetActiveSkinnedModel() {
    if (activeSkinnedModelIndex >= skinnedModels.size()) {
        return nullptr;
    }
    SkinnedModelInstance& instance = skinnedModels[activeSkinnedModelIndex];
    return instance.loaded ? &instance : nullptr;
}

const SkinnedModelInstance* AppSceneResources::GetActiveSkinnedModel() const {
    if (activeSkinnedModelIndex >= skinnedModels.size()) {
        return nullptr;
    }
    const SkinnedModelInstance& instance = skinnedModels[activeSkinnedModelIndex];
    return instance.loaded ? &instance : nullptr;
}

const AppManagedModelResource* AppSceneResources::FindManagedModel(uint32_t modelIndex) const {
    if (modelIndex >= vfxModelLibrary.size()) {
        return nullptr;
    }
    const AppManagedModelResource& model = vfxModelLibrary[modelIndex];
    return model.loaded ? &model : nullptr;
}

void AppSceneResources::UpdateWeaponAttachment(
    const WeaponAttachmentTelemetry& telemetry,
    const Matrix4x4& viewMatrix,
    const Matrix4x4& projMatrix) {
    weaponAttachmentObject.modelIndex = telemetry.modelIndex;
    weaponAttachmentObject.visible =
        telemetry.status == WeaponAttachmentStatus::Active &&
        weaponAttachmentObject.transformData != nullptr &&
        FindManagedModel(telemetry.modelIndex) != nullptr;
    if (!weaponAttachmentObject.visible) {
        return;
    }

    Matrix4x4 worldMatrix = telemetry.worldMatrix;
    weaponAttachmentObject.transformData->World = worldMatrix;
    weaponAttachmentObject.transformData->WVP =
        Multiply(worldMatrix, Multiply(viewMatrix, projMatrix));
    weaponAttachmentObject.transformData->WorldInverseTranspose =
        Transpose(Inverse(worldMatrix));
}

void AppSceneResources::SyncCourseMeshRenderQueue(
    const CourseSpawnRuntime& courseRuntime,
    const CourseAsset* course,
    float currentDistance,
    const RailPath& railPath,
    const Matrix4x4& viewMatrix,
    const Matrix4x4& projMatrix) {
    std::vector<CourseMeshModelBinding> bindings;
    bindings.reserve(vfxModelLibrary.size());
    for (const AppManagedModelResource& model : vfxModelLibrary) {
        CourseMeshModelBinding binding{};
        binding.name = model.name;
        binding.rootLocal = model.model.rootNode.localMatrix;
        binding.loaded =
            model.loaded &&
            model.mesh.indexCount > 0 &&
            model.textureGpu.ptr != 0;
        bindings.push_back(binding);
    }

    courseMeshRenderQueue.SyncFromCourseRuntime(
        courseRuntime,
        course,
        currentDistance,
        railPath,
        bindings,
        viewMatrix,
        projMatrix);
}

void AppSceneResources::UpdateTransforms(
    const AppRuntimeState& runtimeState,
    Matrix4x4* wvpData,
    const Matrix4x4& viewMatrix,
    const Matrix4x4& projMatrix,
    uint32_t windowWidth,
    uint32_t windowHeight) {
    if (wvpData != nullptr) {
        Matrix4x4 worldMatrix = MakeAffineMatrix(
            runtimeState.transform.scale,
            runtimeState.transform.rotate,
            runtimeState.transform.translate);
        *wvpData = Multiply(worldMatrix, Multiply(viewMatrix, projMatrix));
    }

    if (transformationMatrixDataSprite != nullptr) {
        Matrix4x4 worldMatrixSprite = MakeAffineMatrix(
            runtimeState.transformSprite.scale,
            runtimeState.transformSprite.rotate,
            runtimeState.transformSprite.translate);
        Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(
            0.0f,
            0.0f,
            float(windowWidth),
            float(windowHeight),
            0.0f,
            100.0f);
        transformationMatrixDataSprite->World = worldMatrixSprite;
        transformationMatrixDataSprite->WVP = Multiply(
            worldMatrixSprite,
            Multiply(MakeIdentity4x4(), projectionMatrixSprite));
        transformationMatrixDataSprite->WorldInverseTranspose =
            Transpose(Inverse(worldMatrixSprite));
    }

    if (materialDataSprite != nullptr) {
        Matrix4x4 uvTransformMatrix = MakeScaleMatrix(runtimeState.uvTransformSprite.scale);
        uvTransformMatrix = Multiply(
            uvTransformMatrix,
            MakeRoateZMatrix(runtimeState.uvTransformSprite.rotate.z));
        uvTransformMatrix = Multiply(
            uvTransformMatrix,
            MakeTranslateMatrix(runtimeState.uvTransformSprite.translate));
        materialDataSprite->uvTransform = uvTransformMatrix;
    }

    if (sphere.mappedCBV != nullptr) {
        Matrix4x4 worldMatrixSphere = MakeAffineMatrix(
            runtimeState.transform.scale,
            runtimeState.transform.rotate,
            runtimeState.transform.translate);
        const Matrix4x4& rootLocal = modelData.rootNode.localMatrix;
        Matrix4x4 worldWithNode = Multiply(rootLocal, worldMatrixSphere);
        Matrix4x4 wvpWithNode = Multiply(worldWithNode, Multiply(viewMatrix, projMatrix));
        sphere.mappedCBV->World = worldWithNode;
        sphere.mappedCBV->WVP = wvpWithNode;
        sphere.mappedCBV->WorldInverseTranspose = Transpose(Inverse(worldWithNode));
    }

    if (animatedCubeTransformData != nullptr) {
        Matrix4x4 animatedLocal = animatedCubeData.rootNode.localMatrix;
        if (const NodeAnimation* nodeAnimation =
                FindAnimatedCubeNodeAnimation(animatedCubeAnimation, animatedCubeData.rootNode)) {
            animatedLocal = MakeNodeAnimationMatrix(*nodeAnimation, runtimeState.animatedCubeTime);
        }

        Matrix4x4 baseWorld = MakeAffineMatrix(
            runtimeState.animatedCubeTransform.scale,
            runtimeState.animatedCubeTransform.rotate,
            runtimeState.animatedCubeTransform.translate);
        Matrix4x4 worldMatrix = Multiply(animatedLocal, baseWorld);
        animatedCubeTransformData->World = worldMatrix;
        animatedCubeTransformData->WVP = Multiply(worldMatrix, Multiply(viewMatrix, projMatrix));
        animatedCubeTransformData->WorldInverseTranspose = Transpose(Inverse(worldMatrix));

    }

    for (size_t index = 0;
         index < vfxModelObjects.size() && index < runtimeState.vfxModelObjects.size();
         ++index) {
        AppModelObjectInstance& object = vfxModelObjects[index];
        const RuntimeVfxModelObjectState& objectState = runtimeState.vfxModelObjects[index];
        uint32_t modelIndex = objectState.modelIndex;
        if (!vfxModelLibrary.empty()) {
            modelIndex = (std::min)(
                modelIndex,
                static_cast<uint32_t>(vfxModelLibrary.size() - 1));
        }

        const AppManagedModelResource* managedModel = FindManagedModel(modelIndex);
        object.modelIndex = modelIndex;
        object.transform = objectState.transform;
        object.visible =
            runtimeState.showVfxModelObjects &&
            objectState.visible &&
            managedModel != nullptr &&
            object.transformData != nullptr;

        if (!object.visible) {
            continue;
        }

        Matrix4x4 worldMatrix = MakeAffineMatrix(
            object.transform.scale,
            object.transform.rotate,
            object.transform.translate);
        worldMatrix = Multiply(managedModel->model.rootNode.localMatrix, worldMatrix);
        object.transformData->World = worldMatrix;
        object.transformData->WVP = Multiply(worldMatrix, Multiply(viewMatrix, projMatrix));
        object.transformData->WorldInverseTranspose = Transpose(Inverse(worldMatrix));
    }

    SkinnedModelInstance* activeSkinnedModel = GetActiveSkinnedModel();
    if (activeSkinnedModel != nullptr) {
        activeSkinnedModel->visible = runtimeState.showSkinnedModel;
        activeSkinnedModel->transform = runtimeState.skinnedModelTransform;
        if (activeSkinnedModel->visible) {
            UpdateSkinnedModelInstanceTransform(*activeSkinnedModel, viewMatrix, projMatrix);
        }

        if (runtimeState.showSkeletonDebug &&
            mappedSkeletonDebugLines != nullptr &&
            skeletonDebugTransformData != nullptr) {
            skeletonDebugVertexCount = 0;
            const Vector4 rootColor = { 1.0f, 0.9f, 0.1f, 1.0f };
            const Vector4 childColor = { 0.0f, 0.95f, 1.0f, 1.0f };
            const Vector4 markerXColor = { 1.0f, 0.15f, 0.15f, 1.0f };
            const Vector4 markerYColor = { 0.1f, 1.0f, 0.25f, 1.0f };
            const Vector4 markerZColor = { 0.2f, 0.55f, 1.0f, 1.0f };
            const Vector4 fallbackLinkColor = { 1.0f, 0.15f, 1.0f, 1.0f };
            const float markerSize = 0.06f;
            std::vector<Vector3> animatedJointPositions;
            size_t animatedHierarchyLinkCount = 0;
            auto pushLine = [&](const Vector3& a, const Vector3& b, const Vector4& colorA, const Vector4& colorB) {
                if (skeletonDebugVertexCount + 2 > skeletonDebugVertexCapacity) {
                    return;
                }
                mappedSkeletonDebugLines[skeletonDebugVertexCount++] = {
                    { a.x, a.y, a.z, 1.0f },
                    colorA,
                };
                mappedSkeletonDebugLines[skeletonDebugVertexCount++] = {
                    { b.x, b.y, b.z, 1.0f },
                    colorB,
                };
            };
            auto distanceSquared = [](const Vector3& a, const Vector3& b) {
                const float dx = a.x - b.x;
                const float dy = a.y - b.y;
                const float dz = a.z - b.z;
                return dx * dx + dy * dy + dz * dz;
            };

            const Skeleton& skeleton = activeSkinnedModel->skeleton;
            const AnimationClip& animation = activeSkinnedModel->animation;
            for (const Joint& joint : skeleton.joints) {
                const Vector3 jointPosition = ExtractTranslation(joint.skeletonSpaceMatrix);
                const bool isAnimatedJoint =
                    animation.nodeAnimations.find(joint.name) != animation.nodeAnimations.end();
                if (isAnimatedJoint) {
                    animatedJointPositions.push_back(jointPosition);
                }

                pushLine(
                    { jointPosition.x - markerSize, jointPosition.y, jointPosition.z },
                    { jointPosition.x + markerSize, jointPosition.y, jointPosition.z },
                    markerXColor,
                    markerXColor);
                pushLine(
                    { jointPosition.x, jointPosition.y - markerSize, jointPosition.z },
                    { jointPosition.x, jointPosition.y + markerSize, jointPosition.z },
                    markerYColor,
                    markerYColor);
                pushLine(
                    { jointPosition.x, jointPosition.y, jointPosition.z - markerSize },
                    { jointPosition.x, jointPosition.y, jointPosition.z + markerSize },
                    markerZColor,
                    markerZColor);

                if (!joint.parent.has_value()) {
                    continue;
                }

                const Joint& parent = skeleton.joints[static_cast<size_t>(*joint.parent)];
                const Vector3 parentPosition = ExtractTranslation(parent.skeletonSpaceMatrix);
                const bool parentIsAnimatedJoint =
                    animation.nodeAnimations.find(parent.name) != animation.nodeAnimations.end();
                if (isAnimatedJoint && parentIsAnimatedJoint &&
                    distanceSquared(parentPosition, jointPosition) > 0.0001f) {
                    ++animatedHierarchyLinkCount;
                }
                pushLine(parentPosition, jointPosition, rootColor, childColor);
            }

            if (animatedHierarchyLinkCount == 0 && animatedJointPositions.size() >= 2) {
                for (size_t index = 0; index + 1 < animatedJointPositions.size(); ++index) {
                    pushLine(
                        animatedJointPositions[index],
                        animatedJointPositions[index + 1],
                        fallbackLinkColor,
                        fallbackLinkColor);
                }
            }

            Matrix4x4 baseWorld = MakeAffineMatrix(
                activeSkinnedModel->transform.scale,
                activeSkinnedModel->transform.rotate,
                activeSkinnedModel->transform.translate);
            skeletonDebugTransformData->World = baseWorld;
            skeletonDebugTransformData->WVP = Multiply(baseWorld, Multiply(viewMatrix, projMatrix));
            skeletonDebugTransformData->WorldInverseTranspose = Transpose(Inverse(baseWorld));
        } else {
            skeletonDebugVertexCount = 0;
        }
    } else {
        skeletonDebugVertexCount = 0;
    }

    if (skybox.mappedCBV != nullptr) {
        Matrix4x4 skyboxView = viewMatrix;
        skyboxView.m[3][0] = 0.0f;
        skyboxView.m[3][1] = 0.0f;
        skyboxView.m[3][2] = 0.0f;
        skybox.mappedCBV->World = MakeIdentity4x4();
        skybox.mappedCBV->WVP = Multiply(skyboxView, projMatrix);
        skybox.mappedCBV->WorldInverseTranspose = MakeIdentity4x4();
    }
}

void AppSceneResources::SyncRuntimeState(AppRuntimeState& runtimeState, float deltaTime) {
    RuntimeSkinnedAnimationBlendState& animationBlend =
        runtimeState.skinnedAnimationBlend;
    const bool validAnimationBlend =
        animationBlend.active &&
        animationBlend.fromModelIndex < skinnedModels.size() &&
        animationBlend.toModelIndex < skinnedModels.size() &&
        animationBlend.fromModelIndex != animationBlend.toModelIndex &&
        skinnedModels[animationBlend.fromModelIndex].loaded &&
        skinnedModels[animationBlend.toModelIndex].loaded;
    if (animationBlend.active && !validAnimationBlend) {
        CancelSkinnedAnimationBlend(runtimeState);
    }

    activeSkinnedModelIndex = validAnimationBlend
        ? animationBlend.fromModelIndex
        : (std::min)(
            runtimeState.selectedSkinnedModelIndex,
            uint32_t(skinnedModels.size() - 1));
    runtimeState.selectedSkinnedModelIndex = activeSkinnedModelIndex;
    runtimeState.selectedVfxModelObjectIndex = (std::min)(
        runtimeState.selectedVfxModelObjectIndex,
        uint32_t(runtimeState.vfxModelObjects.size() - 1));
    if (!vfxModelLibrary.empty()) {
        const uint32_t maxModelIndex = static_cast<uint32_t>(vfxModelLibrary.size() - 1);
        for (RuntimeVfxModelObjectState& objectState : runtimeState.vfxModelObjects) {
            objectState.modelIndex = (std::min)(objectState.modelIndex, maxModelIndex);
        }
    }

    SkinnedModelInstance* activeSkinnedModel = GetActiveSkinnedModel();
    const bool updateActiveSkinned =
        activeSkinnedModel != nullptr &&
        (runtimeState.showSkinnedModel ||
            runtimeState.showSkeletonDebug ||
            runtimeState.vfx.enableSkinnedSurfaceVfx ||
            runtimeState.handParticleAttachment.enabled ||
            runtimeState.leftHandParticleAttachment.enabled ||
            runtimeState.weaponAttachment.enabled ||
            animationBlend.active);

    if (updateActiveSkinned) {
        activeSkinnedModel->visible = runtimeState.showSkinnedModel;
        activeSkinnedModel->transform = runtimeState.skinnedModelTransform;
        if (animationBlend.active) {
            SkinnedModelInstance& targetModel =
                skinnedModels[animationBlend.toModelIndex];
            targetModel.visible = false;
            targetModel.transform = runtimeState.skinnedModelTransform;

            const auto advanceAnimationClock = [&](
                SkinnedModelInstance& model,
                float& animationTime) {
                model.animator.time = animationTime;
                model.animator.playing = runtimeState.playAnimatedCube;
                model.animator.loop = runtimeState.loopAnimatedCube;
                model.animator.speed = runtimeState.animatedCubeSpeed;
                if (runtimeState.playAnimatedCube) {
                    model.animator.Update(deltaTime, model.animation.duration);
                    animationTime = model.animator.time;
                }
            };
            advanceAnimationClock(*activeSkinnedModel, animationBlend.fromTime);
            advanceAnimationClock(targetModel, animationBlend.toTime);

            const float blendAlpha =
                AdvanceSkinnedAnimationBlend(runtimeState, deltaTime);
            ApplyAnimationBlend(
                activeSkinnedModel->skeleton,
                activeSkinnedModel->animation,
                animationBlend.fromTime,
                targetModel.animation,
                animationBlend.toTime,
                blendAlpha);
            UpdateSkeleton(activeSkinnedModel->skeleton);
            UpdateSkinCluster(
                activeSkinnedModel->skinCluster,
                activeSkinnedModel->skeleton);
            runtimeState.animatedCubeTime = animationBlend.fromTime;

            if (blendAlpha >= 1.0f) {
                targetModel.animator.time = animationBlend.toTime;
                targetModel.animator.playing = runtimeState.playAnimatedCube;
                targetModel.animator.loop = runtimeState.loopAnimatedCube;
                targetModel.animator.speed = runtimeState.animatedCubeSpeed;
                ApplyAnimation(
                    targetModel.skeleton,
                    targetModel.animation,
                    animationBlend.toTime);
                UpdateSkeleton(targetModel.skeleton);
                UpdateSkinCluster(targetModel.skinCluster, targetModel.skeleton);
                targetModel.visible = runtimeState.showSkinnedModel;
                activeSkinnedModel->visible = false;
                activeSkinnedModelIndex = animationBlend.toModelIndex;
                CompleteSkinnedAnimationBlend(runtimeState);
            }
        } else {
            UpdateSkinnedModelInstance(
                *activeSkinnedModel,
                deltaTime,
                runtimeState.playAnimatedCube,
                runtimeState.loopAnimatedCube,
                runtimeState.animatedCubeSpeed,
                runtimeState.animatedCubeTime);
        }
    } else if (runtimeState.playAnimatedCube) {
        Animator animator{};
        animator.time = runtimeState.animatedCubeTime;
        animator.speed = runtimeState.animatedCubeSpeed;
        animator.playing = runtimeState.playAnimatedCube;
        animator.loop = runtimeState.loopAnimatedCube;
        animator.Update(deltaTime, animatedCubeAnimation.duration);
        runtimeState.animatedCubeTime = animator.time;
    } else if (activeSkinnedModel != nullptr) {
        activeSkinnedModel->visible = false;
    }

    directionalLightData = runtimeState.directionalLightData;
    directionalLightData.direction = Normalize(directionalLightData.direction);
    runtimeState.directionalLightData.direction = directionalLightData.direction;
    if (mappedLight != nullptr) {
        *mappedLight = directionalLightData;
    }

    pointLightData = runtimeState.pointLightData;
    pointLightData.position.x = sinf(deltaTime) * 2.0f;
    runtimeState.pointLightData.position = pointLightData.position;
    if (mappedPointLight != nullptr) {
        *mappedPointLight = pointLightData;
    }

    spotLight = runtimeState.spotLight;
    spotLight.direction = Normalize(spotLight.direction);
    runtimeState.spotLight.direction = spotLight.direction;
    if (mappedSpotLight != nullptr) {
        *mappedSpotLight = spotLight;
    }

    if (materialData != nullptr) {
        runtimeState.materialData.specularMode = std::clamp(runtimeState.materialData.specularMode, 0, 1);
        *materialData = runtimeState.materialData;
        auto syncGpuMaterial = [&](AppGpuMaterialResource& gpuMaterial) {
            if (gpuMaterial.mappedConstants == nullptr) {
                return;
            }
            *gpuMaterial.mappedConstants = runtimeState.materialData;
            gpuMaterial.mappedConstants->color = {
                runtimeState.materialData.color.x * gpuMaterial.source.baseColorFactor.x,
                runtimeState.materialData.color.y * gpuMaterial.source.baseColorFactor.y,
                runtimeState.materialData.color.z * gpuMaterial.source.baseColorFactor.z,
                runtimeState.materialData.color.w * gpuMaterial.source.baseColorFactor.w,
            };
        };
        for (AppManagedModelResource& managedModel : vfxModelLibrary) {
            for (AppGpuMaterialResource& gpuMaterial : managedModel.gpuMaterials) {
                syncGpuMaterial(gpuMaterial);
            }
        }
        for (SkinnedModelInstance& skinnedModel : skinnedModels) {
            for (AppGpuMaterialResource& gpuMaterial : skinnedModel.gpuMaterials) {
                syncGpuMaterial(gpuMaterial);
            }
        }
    }

    if (terrainMaterialData != nullptr) {
        TerrainAuthoringState& terrain = runtimeState.terrain;
        terrain.materialBrightness = std::clamp(terrain.materialBrightness, 0.05f, 3.0f);
        terrain.materialNoiseStrength = std::clamp(terrain.materialNoiseStrength, 0.0f, 2.0f);
        terrain.materialStrataStrength = std::clamp(terrain.materialStrataStrength, 0.0f, 2.0f);
        terrain.materialStrataBreakupStrength = std::clamp(terrain.materialStrataBreakupStrength, 0.0f, 1.5f);
        terrain.materialSpecularStrength = std::clamp(terrain.materialSpecularStrength, 0.0f, 0.25f);
        terrain.materialRimLightStrength = std::clamp(terrain.materialRimLightStrength, 0.0f, 2.0f);
        terrain.materialBacklightRimBoost = std::clamp(terrain.materialBacklightRimBoost, 0.0f, 2.0f);
        terrain.materialFloorSandShadowStrength = std::clamp(terrain.materialFloorSandShadowStrength, 0.0f, 1.5f);
        terrain.materialDetailNormalStrength = std::clamp(terrain.materialDetailNormalStrength, 0.0f, 2.0f);
        terrain.materialMicroDetailStrength = std::clamp(terrain.materialMicroDetailStrength, 0.0f, 2.0f);
        terrain.materialDetailCacheScale = std::clamp(terrain.materialDetailCacheScale, 0.25f, 4.0f);
        terrain.materialDetailTileWorldSize = std::clamp(terrain.materialDetailTileWorldSize, 32.0f, 240.0f);
        terrain.materialDetailNearScale = std::clamp(terrain.materialDetailNearScale, 0.25f, 3.0f);
        terrain.materialDetailFarScale = std::clamp(terrain.materialDetailFarScale, 0.15f, 1.5f);
        terrain.materialDetailDistanceBlend = std::clamp(terrain.materialDetailDistanceBlend, 40.0f, 420.0f);
        terrain.materialDetailNormalMapStrength = std::clamp(terrain.materialDetailNormalMapStrength, 0.0f, 2.0f);
        terrain.materialDetailHybridBlend = std::clamp(terrain.materialDetailHybridBlend, 0.0f, 1.0f);
        terrain.materialCavityAoStrength = std::clamp(terrain.materialCavityAoStrength, 0.0f, 1.5f);
        terrain.materialSkyFillStrength = std::clamp(terrain.materialSkyFillStrength, 0.0f, 1.2f);
        terrainMaterialData->color = {
            terrain.materialBaseColor.x * terrain.materialBrightness,
            terrain.materialBaseColor.y * terrain.materialBrightness,
            terrain.materialBaseColor.z * terrain.materialBrightness,
            terrain.materialNoiseStrength,
        };
        terrainMaterialData->padding[0] = terrain.materialDetailNormalStrength;
        terrainMaterialData->padding[1] = terrain.materialCavityAoStrength;
        terrainMaterialData->padding[2] = terrain.materialSkyFillStrength;
        terrainMaterialData->enableLighting =
            terrain.displayMode == TerrainDisplayMode::Unlit ? 0 : 1;
        terrainMaterialData->shininess = terrain.materialSpecularStrength;
        terrainMaterialData->environmentCoefficient = terrain.materialStrataStrength;
        terrainMaterialData->specularMode = 1;
        terrainMaterialData->padding2[0] = terrain.materialRimLightStrength;
        terrainMaterialData->padding2[1] = terrain.materialMicroDetailStrength;
        terrainMaterialData->padding2[2] = terrain.useDetailTextureCache ? 1.0f : 0.0f;
        terrainMaterialData->padding2[3] = terrain.materialDetailCacheScale;
        terrainMaterialData->padding2[4] = terrain.materialDetailTileWorldSize;
        terrainMaterialData->padding2[5] = terrain.materialDetailNearScale;
        terrainMaterialData->padding2[6] = terrain.materialDetailFarScale;
        terrainMaterialData->padding2[7] = terrain.materialDetailDistanceBlend;
        terrainMaterialData->padding2[8] = terrain.useDetailNormalMap ? 1.0f : 0.0f;
        terrainMaterialData->padding2[9] = terrain.materialDetailNormalMapStrength;
        terrainMaterialData->padding2[10] = terrain.materialDetailHybridBlend;
        terrainMaterialData->padding2[11] = terrain.invertDetailNormalY ? 1.0f : 0.0f;
        terrainMaterialData->padding2[12] =
            terrain.displayMode == TerrainDisplayMode::DetailNormal ? 1.0f : 0.0f;
        terrainMaterialData->padding2[13] = terrain.materialStrataBreakupStrength;
        terrainMaterialData->padding2[14] = terrain.materialFloorSandShadowStrength;
        terrainMaterialData->padding2[15] = terrain.materialBacklightRimBoost;
        terrainMaterialData->uvTransform = MakeIdentity4x4();
    }
}
