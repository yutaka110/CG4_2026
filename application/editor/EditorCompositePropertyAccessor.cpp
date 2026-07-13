#include "EditorCompositePropertyAccessor.h"

namespace editor {

void EditorCompositePropertyAccessor::Add(EditorPropertyAccessor* accessor) {
    if (accessor != nullptr) {
        accessors_.push_back(accessor);
    }
}

void EditorCompositePropertyAccessor::Clear() {
    accessors_.clear();
}

EditorPropertyAccessor* EditorCompositePropertyAccessor::FindAccessor(
    const EditorObjectHandle& object,
    const EditorPropertyDescriptor& descriptor) const {
    for (EditorPropertyAccessor* accessor : accessors_) {
        if (accessor != nullptr && accessor->CanAccess(object, descriptor)) {
            return accessor;
        }
    }
    return nullptr;
}

bool EditorCompositePropertyAccessor::CanAccess(
    const EditorObjectHandle& object,
    const EditorPropertyDescriptor& descriptor) const {
    return FindAccessor(object, descriptor) != nullptr;
}

bool EditorCompositePropertyAccessor::Get(
    const EditorObjectHandle& object,
    const EditorPropertyDescriptor& descriptor,
    EditorPropertyValue& outValue) const {
    EditorPropertyAccessor* accessor = FindAccessor(object, descriptor);
    return accessor != nullptr && accessor->Get(object, descriptor, outValue);
}

bool EditorCompositePropertyAccessor::Set(
    const EditorObjectHandle& object,
    const EditorPropertyDescriptor& descriptor,
    const EditorPropertyValue& value,
    std::string* errorMessage) {
    EditorPropertyAccessor* accessor = FindAccessor(object, descriptor);
    if (accessor == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "No property adapter can edit this object.";
        }
        return false;
    }
    return accessor->Set(object, descriptor, value, errorMessage);
}

} // namespace editor
