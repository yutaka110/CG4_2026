#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "EditorError.h"
#include "EditorExecutionService.h"

namespace editor {

class EditorExecutionContext final {
public:
    bool Register(IEditorExecutionService& service, EditorError* error = nullptr);
    void Unregister(std::string_view serviceId);
    void Clear();

    IEditorExecutionService* Find(std::string_view serviceId) const noexcept;
    std::size_t ServiceCount() const noexcept { return services_.size(); }

private:
    std::vector<IEditorExecutionService*> services_;
};

} // namespace editor
