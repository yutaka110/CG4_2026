#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

struct CourseAsset;

namespace editor {

std::size_t EnsureCourseWorldObjectGuids(
    CourseAsset& course,
    std::string_view documentIdentity);
bool ValidateCourseWorldObjectGuids(
    const CourseAsset& course,
    std::vector<std::string>* diagnostics = nullptr);

} // namespace editor
