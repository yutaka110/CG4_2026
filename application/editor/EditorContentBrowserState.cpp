#include "EditorContentBrowserState.h"

#include "io/EditorFileTransaction.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>

namespace editor {
namespace {

constexpr int kContentBrowserStateVersion = 1;
constexpr std::chrono::milliseconds kContentBrowserSaveDebounce(750);

std::string NormalizePath(std::filesystem::path path) {
    return path.lexically_normal().generic_string();
}

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

bool IsAssetInFolder(const EditorAssetRecord& record, std::string_view folder) {
    if (folder.empty() || folder == "Resources") return true;
    const std::string source = NormalizePath(record.sourcePath);
    std::string prefix = NormalizePath(std::filesystem::path(folder));
    if (!prefix.empty() && prefix.back() != '/') prefix.push_back('/');
    return source.rfind(prefix, 0) == 0;
}

bool ParseKind(int value, EditorAssetKind& kind) {
    if (value < static_cast<int>(EditorAssetKind::Unknown) ||
        value > static_cast<int>(EditorAssetKind::MaterialInstance)) return false;
    kind = static_cast<EditorAssetKind>(value);
    return true;
}

std::string Trim(std::string value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return {};
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

} // namespace

EditorContentBrowserState::EditorContentBrowserState()
    : path_("generated/editor/ContentBrowserState.ini") {}

bool EditorAssetWorkspaceStatusRegistry::Publish(
    std::string guid,
    EditorAssetWorkspaceStatus status) {
    if (guid.empty()) return false;
    statuses_[std::move(guid)] = std::move(status);
    ++revision_;
    return true;
}

bool EditorAssetWorkspaceStatusRegistry::Remove(std::string_view guid) {
    if (statuses_.erase(std::string(guid)) == 0) return false;
    ++revision_;
    return true;
}

void EditorAssetWorkspaceStatusRegistry::Clear() {
    if (statuses_.empty()) return;
    statuses_.clear();
    ++revision_;
}

EditorAssetWorkspaceStatus EditorAssetWorkspaceStatusRegistry::QueryStatus(
    const EditorAssetRecord& record) const {
    const auto found = statuses_.find(record.guid);
    if (found != statuses_.end()) return found->second;
    EditorAssetWorkspaceStatus status{};
    if (record.missing) {
        status.cook = EditorAssetCookStatus::Failed;
        status.detail = "Asset source is missing; source-control provider has not published a state.";
    } else {
        status.detail = "No source-control/cook provider state has been published for this Asset.";
    }
    return status;
}

void EditorContentBrowserState::SetPath(std::filesystem::path path) {
    if (path.empty() || path == path_) return;
    path_ = std::move(path);
    loaded_ = false;
    dirty_ = false;
    dirtyTouchedAt_ = {};
    Reset();
}

void EditorContentBrowserState::EnsureLoaded() {
    if (!loaded_) Load();
}

bool EditorContentBrowserState::Load() {
    Reset();
    loaded_ = true;
    dirty_ = false;
    dirtyTouchedAt_ = {};
    std::ifstream input(path_, std::ios::binary);
    if (!input.is_open()) {
        lastLoadValid_ = true;
        statusMessage_ = "Content Browser state not found; defaults are active.";
        ++revision_;
        return true;
    }

    std::string header;
    int version = 0;
    if (!(input >> header >> version) || header != "CONTENT_BROWSER_STATE" ||
        version != kContentBrowserStateVersion) {
        lastLoadValid_ = false;
        statusMessage_ = "Content Browser state schema is invalid; defaults are active.";
        ++revision_;
        return false;
    }

    std::string key;
    bool sawEnd = false;
    while (input >> key) {
        if (key == "END") {
            sawEnd = true;
            break;
        }
        if (key == "folder") input >> std::quoted(selectedFolder_);
        else if (key == "search") input >> std::quoted(searchText_);
        else if (key == "tag") input >> std::quoted(tagFilter_);
        else if (key == "kind") {
            int value = 0;
            input >> value;
            if (!ParseKind(value, kindFilter_)) input.setstate(std::ios::failbit);
        } else if (key == "view") {
            int value = 0;
            input >> value;
            if (value < 0 || value > 1) input.setstate(std::ios::failbit);
            else viewMode_ = static_cast<EditorContentBrowserViewMode>(value);
        } else if (key == "selected") input >> std::quoted(selectedAssetGuid_);
        else if (key == "activeCollection") input >> std::quoted(activeCollection_);
        else if (key == "favoritesOnly") input >> favoritesOnly_;
        else if (key == "favorite") {
            std::string guid;
            input >> std::quoted(guid);
            if (!guid.empty()) favorites_.insert(std::move(guid));
        } else if (key == "collection") {
            std::string name;
            std::string guid;
            input >> std::quoted(name) >> std::quoted(guid);
            if (!name.empty()) {
                EditorContentBrowserCollection& collection = collections_[name];
                collection.name = name;
                if (!guid.empty()) collection.assetGuids.insert(std::move(guid));
            }
        } else {
            input.setstate(std::ios::failbit);
        }
        if (!input) break;
    }

    if (!sawEnd || (!input && !input.eof())) {
        Reset();
        loaded_ = true;
        lastLoadValid_ = false;
        statusMessage_ = "Content Browser state is malformed; defaults are active.";
        ++revision_;
        return false;
    }
    if (selectedFolder_.empty()) selectedFolder_ = "Resources";
    if (!activeCollection_.empty() && collections_.find(activeCollection_) == collections_.end()) {
        activeCollection_.clear();
    }
    lastLoadValid_ = true;
    statusMessage_ = "Content Browser state loaded.";
    ++revision_;
    return true;
}

bool EditorContentBrowserState::Save() {
    std::ostringstream output;
    output << "CONTENT_BROWSER_STATE " << kContentBrowserStateVersion << '\n';
    output << "folder " << std::quoted(selectedFolder_) << '\n';
    output << "search " << std::quoted(searchText_) << '\n';
    output << "tag " << std::quoted(tagFilter_) << '\n';
    output << "kind " << static_cast<int>(kindFilter_) << '\n';
    output << "view " << static_cast<int>(viewMode_) << '\n';
    output << "selected " << std::quoted(selectedAssetGuid_) << '\n';
    output << "activeCollection " << std::quoted(activeCollection_) << '\n';
    output << "favoritesOnly " << (favoritesOnly_ ? 1 : 0) << '\n';
    std::vector<std::string> favorites(favorites_.begin(), favorites_.end());
    std::sort(favorites.begin(), favorites.end());
    for (const std::string& guid : favorites) output << "favorite " << std::quoted(guid) << '\n';
    std::vector<std::string> names;
    names.reserve(collections_.size());
    for (const auto& pair : collections_) names.push_back(pair.first);
    std::sort(names.begin(), names.end());
    for (const std::string& name : names) {
        const EditorContentBrowserCollection& collection = collections_.at(name);
        if (collection.assetGuids.empty()) {
            output << "collection " << std::quoted(name) << " " << std::quoted(std::string{}) << '\n';
            continue;
        }
        std::vector<std::string> guids(collection.assetGuids.begin(), collection.assetGuids.end());
        std::sort(guids.begin(), guids.end());
        for (const std::string& guid : guids) {
            output << "collection " << std::quoted(name) << " " << std::quoted(guid) << '\n';
        }
    }
    output << "END\n";

    EditorFileTransaction transaction;
    std::string error;
    if (!transaction.StageTextWrite(path_, output.str(), {}, &error) ||
        !transaction.Execute(nullptr, &error)) {
        statusMessage_ = error.empty() ? "Failed to save Content Browser state." : error;
        return false;
    }
    dirty_ = false;
    dirtyTouchedAt_ = {};
    statusMessage_ = "Content Browser state saved atomically.";
    ++revision_;
    return true;
}

void EditorContentBrowserState::SaveIfDirty() {
    if (!dirty_) return;
    if (dirtyTouchedAt_ != std::chrono::steady_clock::time_point{} &&
        std::chrono::steady_clock::now() - dirtyTouchedAt_ < kContentBrowserSaveDebounce) return;
    Save();
}

void EditorContentBrowserState::SetSelectedFolder(std::string value) {
    value = NormalizePath(value.empty() ? std::filesystem::path("Resources") : std::filesystem::path(value));
    if (selectedFolder_ == value) return;
    selectedFolder_ = std::move(value);
    MarkDirty();
}

void EditorContentBrowserState::SetSearchText(std::string value) {
    if (searchText_ == value) return;
    searchText_ = std::move(value);
    MarkDirty();
}

void EditorContentBrowserState::SetTagFilter(std::string value) {
    if (tagFilter_ == value) return;
    tagFilter_ = std::move(value);
    MarkDirty();
}

void EditorContentBrowserState::SetKindFilter(EditorAssetKind value) {
    if (kindFilter_ == value) return;
    kindFilter_ = value;
    MarkDirty();
}

void EditorContentBrowserState::SetViewMode(EditorContentBrowserViewMode value) {
    if (viewMode_ == value) return;
    viewMode_ = value;
    MarkDirty();
}

void EditorContentBrowserState::SetSelectedAssetGuid(std::string value) {
    if (selectedAssetGuid_ == value) return;
    selectedAssetGuid_ = std::move(value);
    MarkDirty();
}

void EditorContentBrowserState::SetActiveCollection(std::string value) {
    if (!value.empty() && collections_.find(value) == collections_.end()) return;
    if (activeCollection_ == value) return;
    activeCollection_ = std::move(value);
    MarkDirty();
}

void EditorContentBrowserState::SetFavoritesOnly(bool value) {
    if (favoritesOnly_ == value) return;
    favoritesOnly_ = value;
    MarkDirty();
}

bool EditorContentBrowserState::ToggleFavorite(std::string_view guid) {
    if (guid.empty()) return false;
    const std::string value(guid);
    const auto found = favorites_.find(value);
    if (found == favorites_.end()) favorites_.insert(value);
    else favorites_.erase(found);
    MarkDirty();
    return true;
}

bool EditorContentBrowserState::IsFavorite(std::string_view guid) const {
    return favorites_.find(std::string(guid)) != favorites_.end();
}

bool EditorContentBrowserState::CreateCollection(std::string name) {
    name = Trim(std::move(name));
    if (name.empty() || collections_.find(name) != collections_.end()) return false;
    collections_.emplace(name, EditorContentBrowserCollection{name, {}});
    MarkDirty();
    return true;
}

bool EditorContentBrowserState::RemoveCollection(std::string_view name) {
    const std::string value(name);
    if (collections_.erase(value) == 0) return false;
    if (activeCollection_ == value) activeCollection_.clear();
    MarkDirty();
    return true;
}

bool EditorContentBrowserState::AddToCollection(std::string_view name, std::string_view guid) {
    auto found = collections_.find(std::string(name));
    if (found == collections_.end() || guid.empty()) return false;
    if (!found->second.assetGuids.insert(std::string(guid)).second) return false;
    MarkDirty();
    return true;
}

bool EditorContentBrowserState::RemoveFromCollection(std::string_view name, std::string_view guid) {
    auto found = collections_.find(std::string(name));
    if (found == collections_.end() || found->second.assetGuids.erase(std::string(guid)) == 0) return false;
    MarkDirty();
    return true;
}

bool EditorContentBrowserState::IsInCollection(std::string_view name, std::string_view guid) const {
    const auto found = collections_.find(std::string(name));
    return found != collections_.end() &&
        found->second.assetGuids.find(std::string(guid)) != found->second.assetGuids.end();
}

std::vector<std::string> EditorContentBrowserState::BuildFolders(
    const EditorAssetRegistry& registry) const {
    std::set<std::string> folders{"Resources"};
    for (const EditorAssetRecord& record : registry.Records()) {
        std::filesystem::path parent = std::filesystem::path(record.sourcePath).parent_path();
        while (!parent.empty()) {
            const std::string value = NormalizePath(parent);
            if (value.empty() || value == ".") break;
            folders.insert(value);
            if (value == "Resources") break;
            parent = parent.parent_path();
        }
    }
    return {folders.begin(), folders.end()};
}

std::vector<std::string> EditorContentBrowserState::BuildTags(
    const EditorAssetRegistry& registry) const {
    std::set<std::string> tags;
    for (const EditorAssetRecord& record : registry.Records()) {
        tags.insert(record.tags.begin(), record.tags.end());
    }
    return {tags.begin(), tags.end()};
}

std::vector<const EditorAssetRecord*> EditorContentBrowserState::FilterAssets(
    const EditorAssetRegistry& registry) const {
    std::vector<const EditorAssetRecord*> result;
    const EditorContentBrowserCollection* collection = nullptr;
    const auto collectionIt = collections_.find(activeCollection_);
    if (collectionIt != collections_.end()) collection = &collectionIt->second;
    for (const EditorAssetRecord& record : registry.Records()) {
        if (kindFilter_ != EditorAssetKind::Unknown && record.kind != kindFilter_) continue;
        if (!IsAssetInFolder(record, selectedFolder_)) continue;
        if (favoritesOnly_ && !IsFavorite(record.guid)) continue;
        if (collection != nullptr && collection->assetGuids.find(record.guid) == collection->assetGuids.end()) continue;
        if (!tagFilter_.empty() && std::find(record.tags.begin(), record.tags.end(), tagFilter_) == record.tags.end()) continue;
        if (!searchText_.empty()) {
            bool matched = ContainsInsensitive(record.id, searchText_) ||
                ContainsInsensitive(record.displayName, searchText_) ||
                ContainsInsensitive(record.guid, searchText_) ||
                ContainsInsensitive(record.sourcePath, searchText_) ||
                ContainsInsensitive(ToString(record.kind), searchText_);
            for (const std::string& tag : record.tags) matched = matched || ContainsInsensitive(tag, searchText_);
            if (!matched) continue;
        }
        result.push_back(&record);
    }
    std::sort(result.begin(), result.end(), [](const EditorAssetRecord* left, const EditorAssetRecord* right) {
        if (left->sourcePath != right->sourcePath) return left->sourcePath < right->sourcePath;
        return left->guid < right->guid;
    });
    return result;
}

void EditorContentBrowserState::MarkDirty() {
    dirty_ = true;
    dirtyTouchedAt_ = std::chrono::steady_clock::now();
    ++revision_;
}

void EditorContentBrowserState::Reset() {
    selectedFolder_ = "Resources";
    searchText_.clear();
    tagFilter_.clear();
    kindFilter_ = EditorAssetKind::Unknown;
    viewMode_ = EditorContentBrowserViewMode::Grid;
    selectedAssetGuid_.clear();
    activeCollection_.clear();
    favoritesOnly_ = false;
    favorites_.clear();
    collections_.clear();
    lastLoadValid_ = true;
    statusMessage_.clear();
}

const char* ToString(EditorContentBrowserViewMode mode) {
    return mode == EditorContentBrowserViewMode::Grid ? "Grid" : "List";
}

const char* ToString(EditorAssetSourceControlStatus status) {
    switch (status) {
    case EditorAssetSourceControlStatus::Unknown: return "Unknown";
    case EditorAssetSourceControlStatus::Untracked: return "Untracked";
    case EditorAssetSourceControlStatus::Added: return "Added";
    case EditorAssetSourceControlStatus::Tracked: return "Tracked";
    case EditorAssetSourceControlStatus::Modified: return "Modified";
    case EditorAssetSourceControlStatus::Conflicted: return "Conflicted";
    case EditorAssetSourceControlStatus::Locked: return "Locked";
    }
    return "Unknown";
}

const char* ToString(EditorAssetCookStatus status) {
    switch (status) {
    case EditorAssetCookStatus::Unknown: return "Unknown";
    case EditorAssetCookStatus::NotCooked: return "Not Cooked";
    case EditorAssetCookStatus::UpToDate: return "Up To Date";
    case EditorAssetCookStatus::OutOfDate: return "Out Of Date";
    case EditorAssetCookStatus::Failed: return "Failed";
    }
    return "Unknown";
}

} // namespace editor
