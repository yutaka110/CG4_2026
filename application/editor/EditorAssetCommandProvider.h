#pragma once

#include "EditorCommandProvider.h"

namespace editor {

class EditorAssetCommandProvider final : public EditorCommandProvider {
public:
    void RegisterCommands(EditorContext& context) const override;
};

} // namespace editor
