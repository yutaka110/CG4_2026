#include "EditorDetailsViewState.h"

#include "io/EditorFileTransaction.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace editor {
namespace {

constexpr int kDetailsStateVersion = 1;
constexpr std::chrono::milliseconds kSaveDebounce(750);

std::string Lower(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return result;
}

bool ContainsInsensitive(std::string_view value, std::string_view query) {
    return query.empty() || Lower(value).find(Lower(query)) != std::string::npos;
}

} // namespace

std::string EditorPrefabOverrideRegistry::Key(
    const EditorObjectHandle& object,
    const EditorPropertyDescriptor& descriptor) {
    return std::to_string(static_cast<uint32_t>(object.domain)) + ":" +
        object.stableId + ":" + descriptor.name;
}

bool EditorPrefabOverrideRegistry::Publish(
    const EditorObjectHandle& object,
    const EditorPropertyDescriptor& descriptor,
    EditorPrefabOverrideInfo info) {
    if (object.stableId.empty() || descriptor.name.empty()) return false;
    overrides_[Key(object, descriptor)] = std::move(info);
    ++revision_;
    return true;
}

EditorPrefabOverrideInfo EditorPrefabOverrideRegistry::QueryOverride(
    const EditorObjectHandle& object,
    const EditorPropertyDescriptor& descriptor) const {
    const auto found = overrides_.find(Key(object, descriptor));
    if (found != overrides_.end()) return found->second;
    return {};
}

bool EditorPrefabOverrideRegistry::RevertOverride(
    const EditorObjectHandle& object,
    const EditorPropertyDescriptor& descriptor,
    std::string* errorMessage) {
    const auto found = overrides_.find(Key(object, descriptor));
    if (found == overrides_.end() || !found->second.canRevert) {
        if (errorMessage != nullptr) *errorMessage = "Prefab override is not revertible.";
        return false;
    }
    overrides_.erase(found);
    ++revision_;
    return true;
}

EditorDetailsViewState::EditorDetailsViewState()
    : path_("generated/editor/DetailsState.ini") {}

void EditorDetailsViewState::SetPath(std::filesystem::path path) {
    if (path.empty() || path == path_) return;
    path_ = std::move(path);
    loaded_ = false;
    dirty_ = false;
    dirtyTouchedAt_ = {};
    Reset();
}

void EditorDetailsViewState::EnsureLoaded() {
    if (!loaded_) Load();
}

bool EditorDetailsViewState::Load() {
    Reset();
    loaded_ = true;
    dirty_ = false;
    dirtyTouchedAt_ = {};
    std::ifstream input(path_, std::ios::binary);
    if (!input.is_open()) {
        lastLoadValid_ = true;
        statusMessage_ = "Details state not found; defaults are active.";
        ++revision_;
        return true;
    }
    std::string header;
    int version = 0;
    if (!(input >> header >> version) || header != "DETAILS_STATE" || version != kDetailsStateVersion) {
        lastLoadValid_ = false;
        statusMessage_ = "Details state schema is invalid; defaults are active.";
        ++revision_;
        return false;
    }
    bool sawEnd = false;
    std::string key;
    while (input >> key) {
        if (key == "END") { sawEnd = true; break; }
        if (key == "search") input >> std::quoted(searchText_);
        else if (key == "favoritesOnly") input >> favoritesOnly_;
        else if (key == "changedOnly") input >> changedOnly_;
        else if (key == "category") {
            std::string category;
            bool open = true;
            input >> std::quoted(category) >> open;
            if (!category.empty()) categoryOpen_[std::move(category)] = open;
        } else if (key == "favorite") {
            std::string favorite;
            input >> std::quoted(favorite);
            if (!favorite.empty()) favorites_.insert(std::move(favorite));
        } else input.setstate(std::ios::failbit);
        if (!input) break;
    }
    if (!sawEnd || (!input && !input.eof())) {
        Reset();
        loaded_ = true;
        lastLoadValid_ = false;
        statusMessage_ = "Details state is malformed; defaults are active.";
        ++revision_;
        return false;
    }
    lastLoadValid_ = true;
    statusMessage_ = "Details state loaded.";
    ++revision_;
    return true;
}

bool EditorDetailsViewState::Save() {
    std::ostringstream output;
    output << "DETAILS_STATE " << kDetailsStateVersion << '\n';
    output << "search " << std::quoted(searchText_) << '\n';
    output << "favoritesOnly " << (favoritesOnly_ ? 1 : 0) << '\n';
    output << "changedOnly " << (changedOnly_ ? 1 : 0) << '\n';
    std::vector<std::string> categories;
    for (const auto& pair : categoryOpen_) categories.push_back(pair.first);
    std::sort(categories.begin(), categories.end());
    for (const std::string& category : categories) {
        output << "category " << std::quoted(category) << " " <<
            (categoryOpen_.at(category) ? 1 : 0) << '\n';
    }
    std::vector<std::string> favorites(favorites_.begin(), favorites_.end());
    std::sort(favorites.begin(), favorites.end());
    for (const std::string& favorite : favorites) {
        output << "favorite " << std::quoted(favorite) << '\n';
    }
    output << "END\n";
    EditorFileTransaction transaction;
    std::string error;
    if (!transaction.StageTextWrite(path_, output.str(), {}, &error) ||
        !transaction.Execute(nullptr, &error)) {
        statusMessage_ = error.empty() ? "Failed to save Details state." : error;
        return false;
    }
    dirty_ = false;
    dirtyTouchedAt_ = {};
    statusMessage_ = "Details state saved atomically.";
    ++revision_;
    return true;
}

void EditorDetailsViewState::SaveIfDirty() {
    if (!dirty_) return;
    if (dirtyTouchedAt_ != std::chrono::steady_clock::time_point{} &&
        std::chrono::steady_clock::now() - dirtyTouchedAt_ < kSaveDebounce) return;
    Save();
}

void EditorDetailsViewState::SetSearchText(std::string value) {
    if (searchText_ == value) return;
    searchText_ = std::move(value);
    MarkDirty();
}

void EditorDetailsViewState::SetFavoritesOnly(bool value) {
    if (favoritesOnly_ == value) return;
    favoritesOnly_ = value;
    MarkDirty();
}

void EditorDetailsViewState::SetChangedOnly(bool value) {
    if (changedOnly_ == value) return;
    changedOnly_ = value;
    MarkDirty();
}

void EditorDetailsViewState::SetCategoryOpen(std::string category, bool open) {
    if (category.empty()) return;
    const auto found = categoryOpen_.find(category);
    if (found != categoryOpen_.end() && found->second == open) return;
    categoryOpen_[std::move(category)] = open;
    MarkDirty();
}

bool EditorDetailsViewState::IsCategoryOpen(std::string_view category) const {
    const auto found = categoryOpen_.find(std::string(category));
    return found == categoryOpen_.end() || found->second;
}

std::string EditorDetailsViewState::PropertyKey(
    EditorDomainId domain,
    std::string_view propertyName) {
    return std::to_string(static_cast<uint32_t>(domain)) + ":" + std::string(propertyName);
}

bool EditorDetailsViewState::ToggleFavorite(
    EditorDomainId domain,
    std::string_view propertyName) {
    if (propertyName.empty()) return false;
    const std::string key = PropertyKey(domain, propertyName);
    const auto found = favorites_.find(key);
    if (found == favorites_.end()) favorites_.insert(key);
    else favorites_.erase(found);
    MarkDirty();
    return true;
}

bool EditorDetailsViewState::IsFavorite(
    EditorDomainId domain,
    std::string_view propertyName) const {
    return favorites_.find(PropertyKey(domain, propertyName)) != favorites_.end();
}

bool EditorDetailsViewState::Matches(const EditorPropertyDescriptor& descriptor) const {
    if (favoritesOnly_ && !IsFavorite(descriptor.domain, descriptor.name)) return false;
    return ContainsInsensitive(descriptor.name, searchText_) ||
        ContainsInsensitive(descriptor.displayName, searchText_) ||
        ContainsInsensitive(descriptor.category, searchText_) ||
        ContainsInsensitive(ToString(descriptor.kind), searchText_) ||
        ContainsInsensitive(descriptor.validationHint, searchText_);
}

void EditorDetailsViewState::MarkDirty() {
    dirty_ = true;
    dirtyTouchedAt_ = std::chrono::steady_clock::now();
    ++revision_;
}

void EditorDetailsViewState::Reset() {
    searchText_.clear();
    favoritesOnly_ = false;
    changedOnly_ = false;
    categoryOpen_.clear();
    favorites_.clear();
    lastLoadValid_ = true;
    statusMessage_.clear();
}

const char* ToString(EditorPrefabOverrideState state) {
    switch (state) {
    case EditorPrefabOverrideState::NotApplicable: return "N/A";
    case EditorPrefabOverrideState::Inherited: return "Inherited";
    case EditorPrefabOverrideState::Overridden: return "Overridden";
    case EditorPrefabOverrideState::Added: return "Added";
    case EditorPrefabOverrideState::Removed: return "Removed";
    }
    return "N/A";
}

} // namespace editor
