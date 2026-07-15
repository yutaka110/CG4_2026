#include "EditorAssetThumbnailTextureLoader.h"

#include "../../externals/DirectXTex/DirectXTex.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>

namespace editor {
namespace {

std::wstring ToLowerExtension(const std::filesystem::path& path) {
    std::wstring extension = path.extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return extension;
}

bool LoadScratchImage(
    const std::filesystem::path& path,
    DirectX::ScratchImage& image,
    std::string& outError) {
    DirectX::TexMetadata metadata{};
    const std::wstring extension = ToLowerExtension(path);
    HRESULT hr = E_FAIL;
    if (extension == L".dds") {
        hr = DirectX::LoadFromDDSFile(path.wstring().c_str(), DirectX::DDS_FLAGS_NONE, &metadata, image);
    } else if (extension == L".tga") {
        hr = DirectX::LoadFromTGAFile(path.wstring().c_str(), &metadata, image);
    } else {
        hr = DirectX::LoadFromWICFile(path.wstring().c_str(), DirectX::WIC_FLAGS_NONE, &metadata, image);
    }

    if (FAILED(hr)) {
        outError = "Texture thumbnail decode failed.";
        return false;
    }
    if (metadata.dimension != DirectX::TEX_DIMENSION_TEXTURE2D || metadata.width == 0 || metadata.height == 0) {
        outError = "Texture thumbnail source is not a valid 2D texture.";
        return false;
    }
    return true;
}

bool NormalizeScratchImage(
    const DirectX::ScratchImage& source,
    uint32_t maxExtent,
    DirectX::ScratchImage& output,
    std::string& outError) {
    const DirectX::TexMetadata metadata = source.GetMetadata();
    const DirectX::Image* image = source.GetImage(0, 0, 0);
    if (image == nullptr) {
        outError = "Texture thumbnail source image is empty.";
        return false;
    }

    DirectX::ScratchImage decompressed;
    const DirectX::Image* workingImage = image;
    if (DirectX::IsCompressed(metadata.format)) {
        if (FAILED(DirectX::Decompress(*image, DXGI_FORMAT_R8G8B8A8_UNORM, decompressed))) {
            outError = "Texture thumbnail decompression failed.";
            return false;
        }
        workingImage = decompressed.GetImage(0, 0, 0);
    }

    DirectX::ScratchImage converted;
    if (workingImage != nullptr &&
        workingImage->format != DXGI_FORMAT_R8G8B8A8_UNORM) {
        if (FAILED(DirectX::Convert(
                *workingImage,
                DXGI_FORMAT_R8G8B8A8_UNORM,
                DirectX::TEX_FILTER_DEFAULT,
                DirectX::TEX_THRESHOLD_DEFAULT,
                converted))) {
            outError = "Texture thumbnail format conversion failed.";
            return false;
        }
        workingImage = converted.GetImage(0, 0, 0);
    }
    if (workingImage == nullptr) {
        outError = "Texture thumbnail conversion produced an empty image.";
        return false;
    }

    const size_t maxDimension = (std::max)(1u, maxExtent);
    const double scale = (std::min)(
        1.0,
        static_cast<double>(maxDimension) / static_cast<double>((std::max)(workingImage->width, workingImage->height)));
    const size_t targetWidth = (std::max<size_t>)(1, static_cast<size_t>(workingImage->width * scale));
    const size_t targetHeight = (std::max<size_t>)(1, static_cast<size_t>(workingImage->height * scale));

    DirectX::ScratchImage resized;
    const DirectX::Image* convertedSource = workingImage;
    if (targetWidth != workingImage->width || targetHeight != workingImage->height) {
        if (FAILED(DirectX::Resize(
                *workingImage,
                targetWidth,
                targetHeight,
                DirectX::TEX_FILTER_DEFAULT,
                resized))) {
            // DirectXTex can delegate resize work to platform codecs whose
            // availability differs between editor build configurations. Keep
            // thumbnail generation deterministic with an RGBA8 CPU fallback.
            if (FAILED(output.Initialize2D(
                    DXGI_FORMAT_R8G8B8A8_UNORM,
                    targetWidth,
                    targetHeight,
                    1,
                    1))) {
                outError = "Texture thumbnail resize fallback allocation failed.";
                return false;
            }
            const DirectX::Image* fallback = output.GetImage(0, 0, 0);
            uint8_t* fallbackPixels = output.GetPixels();
            if (fallback == nullptr || fallbackPixels == nullptr) {
                outError = "Texture thumbnail resize fallback produced an empty image.";
                return false;
            }
            for (size_t y = 0; y < targetHeight; ++y) {
                const size_t sourceY = (std::min)(
                    workingImage->height - 1,
                    y * workingImage->height / targetHeight);
                const uint8_t* sourceRow =
                    workingImage->pixels + sourceY * workingImage->rowPitch;
                uint8_t* targetRow = fallbackPixels + y * fallback->rowPitch;
                for (size_t x = 0; x < targetWidth; ++x) {
                    const size_t sourceX = (std::min)(
                        workingImage->width - 1,
                        x * workingImage->width / targetWidth);
                    std::copy_n(sourceRow + sourceX * 4, 4, targetRow + x * 4);
                }
            }
            return true;
        }
        convertedSource = resized.GetImage(0, 0, 0);
    }

    if (convertedSource == nullptr) {
        outError = "Texture thumbnail resize produced an empty image.";
        return false;
    }

    if (convertedSource->format == DXGI_FORMAT_R8G8B8A8_UNORM) {
        if (FAILED(output.InitializeFromImage(*convertedSource))) {
            outError = "Texture thumbnail copy failed.";
            return false;
        }
        return true;
    }

    if (FAILED(DirectX::Convert(
            *convertedSource,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            DirectX::TEX_FILTER_DEFAULT,
            DirectX::TEX_THRESHOLD_DEFAULT,
            output))) {
        outError = "Texture thumbnail format conversion failed.";
        return false;
    }
    return true;
}

bool CopyToPixelData(
    const DirectX::ScratchImage& image,
    EditorAssetThumbnailPixelData& outPixels,
    std::string& outError) {
    const DirectX::Image* thumbnail = image.GetImage(0, 0, 0);
    if (thumbnail == nullptr || thumbnail->pixels == nullptr) {
        outError = "Texture thumbnail normalized image is empty.";
        return false;
    }

    outPixels.width = static_cast<uint32_t>(thumbnail->width);
    outPixels.height = static_cast<uint32_t>(thumbnail->height);
    outPixels.rowPitch = outPixels.width * 4u;
    outPixels.rgba8.assign(
        static_cast<size_t>(outPixels.rowPitch) * outPixels.height,
        0u);
    for (uint32_t y = 0; y < outPixels.height; ++y) {
        const uint8_t* sourceRow = thumbnail->pixels + static_cast<size_t>(thumbnail->rowPitch) * y;
        uint8_t* targetRow = outPixels.rgba8.data() + static_cast<size_t>(outPixels.rowPitch) * y;
        std::copy(sourceRow, sourceRow + outPixels.rowPitch, targetRow);
    }
    return true;
}

} // namespace

bool LoadEditorAssetTextureThumbnailPixels(
    const std::string& sourcePath,
    uint32_t maxExtent,
    EditorAssetThumbnailPixelData& outPixels,
    std::string& outError) {
    outPixels = {};
    if (sourcePath.empty()) {
        outError = "Texture thumbnail source path is empty.";
        return false;
    }

    const std::filesystem::path path(sourcePath);
    if (!std::filesystem::exists(path)) {
        outError = "Texture thumbnail source file is missing.";
        return false;
    }

    DirectX::ScratchImage source;
    if (!LoadScratchImage(path, source, outError)) {
        return false;
    }

    DirectX::ScratchImage normalized;
    if (!NormalizeScratchImage(source, maxExtent, normalized, outError)) {
        return false;
    }
    return CopyToPixelData(normalized, outPixels, outError);
}

} // namespace editor
