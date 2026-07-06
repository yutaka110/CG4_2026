#pragma once

#include <filesystem>
#include <string>

class PostProcessStack;

class PostProcessPresetStore {
public:
    explicit PostProcessPresetStore(std::filesystem::path path = DefaultPath());

    bool Load(PostProcessStack& stack, std::string* error = nullptr);
    bool Save(const PostProcessStack& stack, std::string* error = nullptr);

    const std::filesystem::path& Path() const { return path_; }

    static std::filesystem::path DefaultPath();

private:
    std::filesystem::path path_;
};
