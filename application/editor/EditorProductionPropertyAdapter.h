#pragma once

#include "EditorPropertyAccessor.h"

struct AppRuntimeState;
struct CourseAsset;
class EffectRuntime;
class PostProcessStack;

namespace editor {

class EditorProductionPropertyAdapter final : public EditorPropertyAccessor {
public:
    EditorProductionPropertyAdapter(
        EffectRuntime* effectRuntime,
        PostProcessStack* postProcessStack,
        AppRuntimeState* runtimeState,
        CourseAsset* course,
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
    EffectRuntime* effectRuntime_ = nullptr;
    PostProcessStack* postProcessStack_ = nullptr;
    AppRuntimeState* runtimeState_ = nullptr;
    CourseAsset* course_ = nullptr;
    bool markEdits_ = true;
};

} // namespace editor
