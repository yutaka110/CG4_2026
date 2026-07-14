#pragma once

#include "EditorPropertyRegistry.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace editor {

enum class EditorPrefabOverrideState {
    NotApplicable,
    Inherited,
    Overridden,
    Added,
    Removed,
};

struct EditorPrefabOverrideInfo {
    EditorPrefabOverrideState state = EditorPrefabOverrideState::NotApplicable;
    bool canRevert = false;
    std::string sourcePrefab;
    std::string detail;
};

class IEditorPrefabOverrideProvider {
public:
    virtual ~IEditorPrefabOverrideProvider() = default;
    virtual EditorPrefabOverrideInfo QueryOverride(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor) const = 0;
    virtual bool RevertOverride(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor,
        std::string* errorMessage = nullptr) = 0;
};

class EditorPrefabOverrideRegistry final : public IEditorPrefabOverrideProvider {
public:
    bool Publish(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor,
        EditorPrefabOverrideInfo info);
    EditorPrefabOverrideInfo QueryOverride(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor) const override;
    bool RevertOverride(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor,
        std::string* errorMessage = nullptr) override;
    uint32_t Revision() const { return revision_; }

private:
    static std::string Key(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor);
    std::unordered_map<std::string, EditorPrefabOverrideInfo> overrides_;
    uint32_t revision_ = 0;
};

class EditorDetailsViewState {
public:
    EditorDetailsViewState();

    void SetPath(std::filesystem::path path);
    void EnsureLoaded();
    bool Load();
    bool Save();
    void SaveIfDirty();

    void SetSearchText(std::string text);
    void SetFavoritesOnly(bool enabled);
    void SetChangedOnly(bool enabled);
    void SetCategoryOpen(std::string category, bool open);
    bool IsCategoryOpen(std::string_view category) const;
    bool ToggleFavorite(EditorDomainId domain, std::string_view propertyName);
    bool IsFavorite(EditorDomainId domain, std::string_view propertyName) const;
    bool Matches(const EditorPropertyDescriptor& descriptor) const;

    const std::string& SearchText() const { return searchText_; }
    bool FavoritesOnly() const { return favoritesOnly_; }
    bool ChangedOnly() const { return changedOnly_; }
    bool Dirty() const { return dirty_; }
    bool LastLoadValid() const { return lastLoadValid_; }
    uint32_t Revision() const { return revision_; }
    const std::string& StatusMessage() const { return statusMessage_; }

private:
    static std::string PropertyKey(EditorDomainId domain, std::string_view propertyName);
    void MarkDirty();
    void Reset();

    std::filesystem::path path_;
    std::string searchText_;
    bool favoritesOnly_ = false;
    bool changedOnly_ = false;
    std::unordered_map<std::string, bool> categoryOpen_;
    std::unordered_set<std::string> favorites_;
    bool loaded_ = false;
    bool dirty_ = false;
    bool lastLoadValid_ = true;
    uint32_t revision_ = 0;
    std::chrono::steady_clock::time_point dirtyTouchedAt_{};
    std::string statusMessage_;
};

const char* ToString(EditorPrefabOverrideState state);

} // namespace editor
