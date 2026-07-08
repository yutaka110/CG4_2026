#pragma once

#include <cstddef>
#include <vector>

#include "EditorValidation.h"

namespace editor {

class EditorValidationAdapter {
public:
    virtual ~EditorValidationAdapter() = default;
    virtual void Validate(EditorValidationReport& report) const = 0;
};

class EditorValidationService {
public:
    void ClearAdapters();
    void AddAdapter(const EditorValidationAdapter* adapter);
    EditorValidationReport Validate() const;

    std::size_t AdapterCount() const { return adapters_.size(); }

private:
    std::vector<const EditorValidationAdapter*> adapters_;
};

} // namespace editor
