#pragma once

#include <filesystem>
#include <string>

namespace editor {

struct EditorProjectPathResolution {
    bool accepted = false;
    std::filesystem::path absolutePath;
    std::filesystem::path projectRelativePath;
    std::string message;
};

class EditorProjectPathPolicy {
public:
    explicit EditorProjectPathPolicy(
        std::filesystem::path projectRoot = std::filesystem::current_path());

    const std::filesystem::path& ProjectRoot() const noexcept { return projectRoot_; }
    EditorProjectPathResolution Resolve(const std::filesystem::path& path) const;
    bool IsInsideProject(const std::filesystem::path& path) const;

    static bool IsSafeTransactionId(const std::string& transactionId);

private:
    std::filesystem::path projectRoot_;
};

} // namespace editor
