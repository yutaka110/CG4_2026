#pragma once

#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

namespace course_asset_parsing {

inline std::string Trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

inline std::vector<std::string> SplitPipe(const std::string& line) {
    std::vector<std::string> parts;
    std::string part;
    std::stringstream stream(line);
    while (std::getline(stream, part, '|')) {
        parts.push_back(Trim(part));
    }
    return parts;
}

inline float ParseFloatOr(const std::vector<std::string>& parts, size_t index, float fallback) {
    if (index >= parts.size()) {
        return fallback;
    }
    char* end = nullptr;
    const float value = std::strtof(parts[index].c_str(), &end);
    return end != parts[index].c_str() ? value : fallback;
}

inline bool ParseBoolOr(const std::vector<std::string>& parts, size_t index, bool fallback) {
    if (index >= parts.size()) {
        return fallback;
    }
    const std::string& value = parts[index];
    if (value == "1" || value == "true" || value == "yes") {
        return true;
    }
    if (value == "0" || value == "false" || value == "no") {
        return false;
    }
    return fallback;
}

} // namespace course_asset_parsing
