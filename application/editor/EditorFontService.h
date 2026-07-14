#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

struct ImFont;
struct ImFontAtlas;

namespace editor {

inline constexpr uint32_t kEditorFontSettingsSchemaVersion = 1;

struct EditorFontSettings {
    uint32_t schemaVersion = kEditorFontSettingsSchemaVersion;
    std::string regularFont = "MPLUSRounded1c-Medium.ttf";
    std::string monospaceFont;
    float regularSize = 16.0f;
    float monospaceSize = 15.0f;
    float uiScale = 1.0f;
    bool includeJapaneseGlyphs = true;

    friend bool operator==(const EditorFontSettings& lhs, const EditorFontSettings& rhs) = default;
};

struct EditorFontFileInfo {
    std::string relativePath;
    std::string displayName;
    uint64_t bytes = 0;
};

class EditorFontService {
public:
    EditorFontService();

    void SetPaths(std::filesystem::path fontRoot, std::filesystem::path settingsPath);
    bool Load(std::string* errorMessage = nullptr);
    bool Save(EditorFontSettings settings, std::string* errorMessage = nullptr);
    bool ResetToDefaults(std::string* errorMessage = nullptr);
    bool BuildAtlas(ImFontAtlas& atlas, std::string* errorMessage = nullptr);
    void OnContextDestroyed();

    std::vector<EditorFontFileInfo> DiscoverFonts() const;
    bool ValidateSettings(const EditorFontSettings& settings,
        std::string* errorMessage = nullptr) const;

    const EditorFontSettings& AppliedSettings() const { return appliedSettings_; }
    const EditorFontSettings& PendingSettings() const { return pendingSettings_; }
    const std::filesystem::path& FontRoot() const { return fontRoot_; }
    const std::filesystem::path& SettingsPath() const { return settingsPath_; }
    const std::string& StatusMessage() const { return statusMessage_; }
    bool Loaded() const { return loaded_; }
    bool UsingFallback() const { return usingFallback_; }
    bool RestartRequired() const { return pendingSettings_ != appliedSettings_; }
    ImFont* RegularFont() const { return regularFont_; }
    ImFont* MonospaceFont() const { return monospaceFont_; }

    static EditorFontSettings Defaults();

private:
    bool Parse(std::string_view text, EditorFontSettings& settings,
        std::string* errorMessage) const;
    std::string Serialize(const EditorFontSettings& settings) const;
    bool ResolveFont(std::string_view relativePath, std::filesystem::path& resolved,
        std::string* errorMessage) const;

    std::filesystem::path fontRoot_;
    std::filesystem::path settingsPath_;
    EditorFontSettings appliedSettings_ = Defaults();
    EditorFontSettings pendingSettings_ = Defaults();
    bool loaded_ = false;
    bool usingFallback_ = true;
    ImFont* regularFont_ = nullptr;
    ImFont* monospaceFont_ = nullptr;
    std::string statusMessage_;
};

} // namespace editor
