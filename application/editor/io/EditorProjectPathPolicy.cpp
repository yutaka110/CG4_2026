#include "EditorProjectPathPolicy.h"

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <system_error>

namespace editor {
namespace {

std::filesystem::path NormalizeAbsolute(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::path normalized = std::filesystem::weakly_canonical(path, error);
    if (error) {
        error.clear();
        normalized = std::filesystem::absolute(path, error).lexically_normal();
    }
    return normalized;
}

std::wstring NormalizeComponent(std::wstring value) {
#if defined(_WIN32)
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(::towlower(ch));
    });
#endif
    return value;
}

bool HasPrefix(
    const std::filesystem::path& candidate,
    const std::filesystem::path& root) {
    auto candidateIt = candidate.begin();
    for (auto rootIt = root.begin(); rootIt != root.end(); ++rootIt, ++candidateIt) {
        if (candidateIt == candidate.end() ||
            NormalizeComponent(candidateIt->wstring()) != NormalizeComponent(rootIt->wstring())) {
            return false;
        }
    }
    return true;
}

} // namespace

EditorProjectPathPolicy::EditorProjectPathPolicy(std::filesystem::path projectRoot)
    : projectRoot_(NormalizeAbsolute(std::move(projectRoot))) {
}

EditorProjectPathResolution EditorProjectPathPolicy::Resolve(
    const std::filesystem::path& path) const {
    EditorProjectPathResolution result{};
    if (path.empty()) {
        result.message = "Project path is empty.";
        return result;
    }

    const std::filesystem::path candidate = NormalizeAbsolute(
        path.is_absolute() ? path : projectRoot_ / path);
    if (!HasPrefix(candidate, projectRoot_) || candidate == projectRoot_) {
        result.message = "Path escapes the project root: " + path.generic_string();
        return result;
    }

    std::error_code error;
    const std::filesystem::path relative = std::filesystem::relative(candidate, projectRoot_, error);
    if (error || relative.empty() || relative.is_absolute()) {
        result.message = "Failed to resolve a project-relative path: " + path.generic_string();
        return result;
    }
    for (const auto& component : relative) {
        if (component == "..") {
            result.message = "Path traversal is not allowed: " + path.generic_string();
            return result;
        }
    }

    result.accepted = true;
    result.absolutePath = candidate;
    result.projectRelativePath = relative.lexically_normal();
    return result;
}

bool EditorProjectPathPolicy::IsInsideProject(const std::filesystem::path& path) const {
    return Resolve(path).accepted;
}

bool EditorProjectPathPolicy::IsSafeTransactionId(const std::string& transactionId) {
    if (transactionId.empty() || transactionId.size() > 96) {
        return false;
    }
    return std::all_of(transactionId.begin(), transactionId.end(), [](unsigned char ch) {
        return std::isalnum(ch) != 0 || ch == '-' || ch == '_';
    });
}

} // namespace editor
