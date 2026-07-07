#pragma once

#include "EditorAssetRegistry.h"
#include "EditorValidationService.h"

struct CourseAsset;

namespace editor {

class CourseObjectValidationAdapter final : public EditorValidationAdapter {
public:
    CourseObjectValidationAdapter(
        const CourseAsset* course,
        const EditorAssetRegistry* assetRegistry = nullptr);

    void Validate(EditorValidationReport& report) const override;

private:
    const CourseAsset* course_ = nullptr;
    const EditorAssetRegistry* assetRegistry_ = nullptr;
};

} // namespace editor
