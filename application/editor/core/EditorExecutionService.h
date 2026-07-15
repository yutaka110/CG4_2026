#pragma once

#include <string_view>

namespace editor {

class IEditorExecutionService {
public:
    virtual ~IEditorExecutionService() = default;
    virtual std::string_view ServiceId() const noexcept = 0;
};

} // namespace editor
