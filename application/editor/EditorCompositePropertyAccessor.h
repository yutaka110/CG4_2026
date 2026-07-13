#pragma once

#include "EditorPropertyAccessor.h"

#include <vector>

namespace editor {

class EditorCompositePropertyAccessor final : public EditorPropertyAccessor {
public:
    void Add(EditorPropertyAccessor* accessor);
    void Clear();

    bool CanAccess(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor) const override;
    bool Get(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor,
        EditorPropertyValue& outValue) const override;
    bool Set(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor,
        const EditorPropertyValue& value,
        std::string* errorMessage = nullptr) override;

private:
    EditorPropertyAccessor* FindAccessor(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor) const;

    std::vector<EditorPropertyAccessor*> accessors_;
};

} // namespace editor
