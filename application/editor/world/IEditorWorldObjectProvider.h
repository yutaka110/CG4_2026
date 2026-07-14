#pragma once

#include "EditorWorldObjectRecord.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

struct EditorWorldProviderEnumeration {
    std::vector<EditorWorldObjectRecord> objects;
    std::vector<std::string> diagnostics;
};

class IEditorWorldObjectProvider {
public:
    virtual ~IEditorWorldObjectProvider() = default;

    virtual std::string_view ProviderId() const noexcept = 0;
    virtual int32_t Priority() const noexcept = 0;
    virtual bool Enumerate(
        EditorWorldProviderEnumeration* output,
        std::string* errorMessage) const = 0;
    virtual bool Resolve(
        const EditorObjectHandle& handle,
        EditorWorldObjectRecord* record) const = 0;
};

} // namespace editor
