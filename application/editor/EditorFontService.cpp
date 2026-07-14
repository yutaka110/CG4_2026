#include "EditorFontService.h"

#include "io/EditorFileTransaction.h"
#include "../../externals/imgui/imgui.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>

namespace editor {
namespace {

constexpr uint64_t kMaxFontBytes = 64ull * 1024ull * 1024ull;
constexpr std::string_view kHeader = "EDITOR_FONT_SETTINGS";

std::string Lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool SupportedExtension(const std::filesystem::path& path) {
    const std::string extension = Lower(path.extension().string());
    return extension == ".ttf" || extension == ".otf" || extension == ".ttc";
}

bool ParseFloat(std::string_view text, float& value) {
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

bool ParseBool(std::string_view text, bool& value) {
    if (text == "true" || text == "1") value = true;
    else if (text == "false" || text == "0") value = false;
    else return false;
    return true;
}

bool Inside(const std::filesystem::path& root, const std::filesystem::path& child) {
    auto rootIt = root.begin();
    auto childIt = child.begin();
    for (; rootIt != root.end(); ++rootIt, ++childIt) {
        if (childIt == child.end() || Lower(rootIt->string()) != Lower(childIt->string())) return false;
    }
    return true;
}

} // namespace

EditorFontService::EditorFontService()
    : fontRoot_("Resources/Editor/Fonts"),
      settingsPath_("generated/editor/EditorFontSettings.ini") {}

EditorFontSettings EditorFontService::Defaults() { return {}; }

void EditorFontService::SetPaths(
    std::filesystem::path fontRoot, std::filesystem::path settingsPath) {
    if (!fontRoot.empty()) fontRoot_ = std::move(fontRoot);
    if (!settingsPath.empty()) settingsPath_ = std::move(settingsPath);
    loaded_ = false;
}

bool EditorFontService::ResolveFont(std::string_view relativePath,
    std::filesystem::path& resolved, std::string* errorMessage) const {
    if (relativePath.empty()) { resolved.clear(); return true; }
    const std::filesystem::path relative(relativePath);
    if (relative.is_absolute() || relative.has_root_path() || !SupportedExtension(relative)) {
        if (errorMessage != nullptr) *errorMessage = "Editor font must be a relative TTF/OTF/TTC path.";
        return false;
    }
    std::error_code error;
    const std::filesystem::path root = std::filesystem::weakly_canonical(fontRoot_, error);
    if (error) {
        if (errorMessage != nullptr) *errorMessage = "Editor font root could not be resolved.";
        return false;
    }
    resolved = std::filesystem::weakly_canonical(fontRoot_ / relative, error);
    if (error || !Inside(root, resolved) || !std::filesystem::is_regular_file(resolved, error)) {
        if (errorMessage != nullptr) *errorMessage = "Editor font is missing or outside Resources/Editor/Fonts.";
        return false;
    }
    const uint64_t bytes = std::filesystem::file_size(resolved, error);
    if (error || bytes == 0 || bytes > kMaxFontBytes) {
        if (errorMessage != nullptr) *errorMessage = "Editor font must be between 1 byte and 64 MiB.";
        return false;
    }
    return true;
}

bool EditorFontService::ValidateSettings(
    const EditorFontSettings& settings, std::string* errorMessage) const {
    if (settings.schemaVersion != kEditorFontSettingsSchemaVersion ||
        settings.regularSize < 8.0f || settings.regularSize > 48.0f ||
        settings.monospaceSize < 8.0f || settings.monospaceSize > 48.0f ||
        settings.uiScale < 0.75f || settings.uiScale > 2.5f) {
        if (errorMessage != nullptr) *errorMessage = "Editor font size or UI scale is outside the safe range.";
        return false;
    }
    std::filesystem::path resolved;
    if (!ResolveFont(settings.regularFont, resolved, errorMessage)) return false;
    if (!ResolveFont(settings.monospaceFont, resolved, errorMessage)) return false;
    return true;
}

bool EditorFontService::Parse(std::string_view text, EditorFontSettings& settings,
    std::string* errorMessage) const {
    std::istringstream input{std::string(text)};
    std::string line;
    if (!std::getline(input, line)) return false;
    std::istringstream header(line);
    std::string marker;
    uint32_t schema = 0;
    if (!(header >> marker >> schema) || marker != kHeader ||
        schema != kEditorFontSettingsSchemaVersion) {
        if (errorMessage != nullptr) *errorMessage = "Editor font settings header is invalid.";
        return false;
    }
    EditorFontSettings parsed;
    while (std::getline(input, line)) {
        if (line.empty()) continue;
        std::istringstream row(line);
        std::string key;
        row >> key;
        if (key == "regular") {
            if (!(row >> std::quoted(parsed.regularFont))) return false;
        } else if (key == "monospace") {
            if (!(row >> std::quoted(parsed.monospaceFont))) return false;
        } else if (key == "regularSize") {
            std::string value; if (!(row >> value) || !ParseFloat(value, parsed.regularSize)) return false;
        } else if (key == "monospaceSize") {
            std::string value; if (!(row >> value) || !ParseFloat(value, parsed.monospaceSize)) return false;
        } else if (key == "uiScale") {
            std::string value; if (!(row >> value) || !ParseFloat(value, parsed.uiScale)) return false;
        } else if (key == "japanese") {
            std::string value; if (!(row >> value) || !ParseBool(value, parsed.includeJapaneseGlyphs)) return false;
        } else {
            if (errorMessage != nullptr) *errorMessage = "Editor font settings contain an unknown field.";
            return false;
        }
    }
    if (!ValidateSettings(parsed, errorMessage)) return false;
    settings = std::move(parsed);
    return true;
}

std::string EditorFontService::Serialize(const EditorFontSettings& settings) const {
    std::ostringstream output;
    output << kHeader << ' ' << kEditorFontSettingsSchemaVersion << '\n';
    output << "regular " << std::quoted(settings.regularFont) << '\n';
    output << "monospace " << std::quoted(settings.monospaceFont) << '\n';
    output << "regularSize " << settings.regularSize << '\n';
    output << "monospaceSize " << settings.monospaceSize << '\n';
    output << "uiScale " << settings.uiScale << '\n';
    output << "japanese " << (settings.includeJapaneseGlyphs ? "true" : "false") << '\n';
    return output.str();
}

bool EditorFontService::Load(std::string* errorMessage) {
    appliedSettings_ = Defaults();
    pendingSettings_ = appliedSettings_;
    std::ifstream stream(settingsPath_, std::ios::binary);
    if (!stream.is_open()) {
        loaded_ = true;
        statusMessage_ = "Using default ImGui font settings; no saved font settings were found.";
        return true;
    }
    const std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    EditorFontSettings parsed;
    std::string error;
    if (text.size() > 64u * 1024u || !Parse(text, parsed, &error)) {
        loaded_ = true;
        statusMessage_ = "Saved font settings were rejected; using safe defaults. " + error;
        if (errorMessage != nullptr) *errorMessage = statusMessage_;
        return false;
    }
    appliedSettings_ = parsed;
    pendingSettings_ = parsed;
    loaded_ = true;
    statusMessage_ = "Editor font settings loaded.";
    return true;
}

bool EditorFontService::Save(EditorFontSettings settings, std::string* errorMessage) {
    settings.schemaVersion = kEditorFontSettingsSchemaVersion;
    if (!ValidateSettings(settings, errorMessage)) return false;
    EditorFileTransaction transaction;
    if (!transaction.StageTextWrite(settingsPath_, Serialize(settings), {}, errorMessage) ||
        !transaction.Execute(nullptr, errorMessage)) return false;
    pendingSettings_ = std::move(settings);
    statusMessage_ = RestartRequired()
        ? "Font settings saved. Restart the Editor to rebuild the DX12 font atlas."
        : "Font settings saved.";
    return true;
}

bool EditorFontService::ResetToDefaults(std::string* errorMessage) {
    return Save(Defaults(), errorMessage);
}

std::vector<EditorFontFileInfo> EditorFontService::DiscoverFonts() const {
    std::vector<EditorFontFileInfo> result;
    std::error_code error;
    if (!std::filesystem::is_directory(fontRoot_, error)) return result;
    for (std::filesystem::recursive_directory_iterator iterator(fontRoot_, error), end;
         !error && iterator != end; iterator.increment(error)) {
        if (!iterator->is_regular_file(error) || !SupportedExtension(iterator->path())) continue;
        const uint64_t bytes = iterator->file_size(error);
        if (error || bytes == 0 || bytes > kMaxFontBytes) { error.clear(); continue; }
        EditorFontFileInfo info;
        info.relativePath = std::filesystem::relative(iterator->path(), fontRoot_, error).generic_string();
        if (error) { error.clear(); continue; }
        info.displayName = iterator->path().stem().string();
        info.bytes = bytes;
        result.push_back(std::move(info));
    }
    std::sort(result.begin(), result.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.relativePath < rhs.relativePath;
    });
    return result;
}

bool EditorFontService::BuildAtlas(ImFontAtlas& atlas, std::string* errorMessage) {
    if (!loaded_) Load();
    atlas.Clear();
    regularFont_ = nullptr;
    monospaceFont_ = nullptr;
    usingFallback_ = false;
    const ImWchar* ranges = appliedSettings_.includeJapaneseGlyphs
        ? atlas.GetGlyphRangesJapanese() : atlas.GetGlyphRangesDefault();
    ImFontConfig config;
    config.OversampleH = 2;
    config.OversampleV = 2;
    config.PixelSnapH = false;
    const float scale = appliedSettings_.uiScale;
    std::filesystem::path regularPath;
    std::string regularError;
    if (!appliedSettings_.regularFont.empty() &&
        ResolveFont(appliedSettings_.regularFont, regularPath, &regularError)) {
        regularFont_ = atlas.AddFontFromFileTTF(regularPath.string().c_str(),
            appliedSettings_.regularSize * scale, &config, ranges);
    }
    if (regularFont_ == nullptr) {
        ImFontConfig fallbackConfig;
        fallbackConfig.SizePixels = appliedSettings_.regularSize * scale;
        regularFont_ = atlas.AddFontDefault(&fallbackConfig);
        usingFallback_ = true;
    }
    std::filesystem::path monoPath;
    if (!appliedSettings_.monospaceFont.empty() &&
        ResolveFont(appliedSettings_.monospaceFont, monoPath, nullptr)) {
        monospaceFont_ = atlas.AddFontFromFileTTF(monoPath.string().c_str(),
            appliedSettings_.monospaceSize * scale, &config, ranges);
    }
    if (monospaceFont_ == nullptr) monospaceFont_ = regularFont_;
    if (regularFont_ == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "ImGui could not create a fallback font.";
        return false;
    }
    statusMessage_ = usingFallback_
        ? "Using the built-in ImGui fallback font. Add a licensed font under Resources/Editor/Fonts for Japanese glyphs."
        : "Configured Editor fonts were added to the ImGui atlas.";
    if (!regularError.empty() && errorMessage != nullptr) *errorMessage = regularError;
    return true;
}

void EditorFontService::OnContextDestroyed() {
    regularFont_ = nullptr;
    monospaceFont_ = nullptr;
}

} // namespace editor
