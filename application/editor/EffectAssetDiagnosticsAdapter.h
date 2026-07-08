#pragma once

#include <vector>

#include "EditorValidationService.h"

struct LoadedEffectAsset;

namespace editor {

class EffectAssetDiagnosticsAdapter final : public EditorValidationAdapter {
public:
    explicit EffectAssetDiagnosticsAdapter(const std::vector<LoadedEffectAsset>* loadedEffectAssets);

    void Validate(EditorValidationReport& report) const override;

private:
    const std::vector<LoadedEffectAsset>* loadedEffectAssets_ = nullptr;
};

} // namespace editor
