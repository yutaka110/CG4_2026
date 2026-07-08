#pragma once

#include <string>

#include "EditorPropertyRegistry.h"
#include "EditorPropertyValue.h"

namespace editor {

class EditorPropertyAccessor {
public:
    virtual ~EditorPropertyAccessor() = default;

    virtual bool CanAccess(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor) const = 0;
    virtual bool Get(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor,
        EditorPropertyValue& outValue) const = 0;
    virtual bool Set(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor,
        const EditorPropertyValue& value,
        std::string* errorMessage = nullptr) = 0;
};

std::string FormatEditorPropertyValue(
    const EditorPropertyDescriptor& descriptor,
    const EditorPropertyValue& value);

} // namespace editor
