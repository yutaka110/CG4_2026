#pragma once

#include "EditorPropertyAccessor.h"

struct AppRuntimeState;
struct CourseAsset;

namespace editor {

class CourseObjectPropertyAdapter final : public EditorPropertyAccessor {
public:
    CourseObjectPropertyAdapter(
        CourseAsset* course,
        AppRuntimeState* runtimeState,
        bool markEdits = true);

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
    CourseAsset* course_ = nullptr;
    AppRuntimeState* runtimeState_ = nullptr;
    bool markEdits_ = true;
};

} // namespace editor
