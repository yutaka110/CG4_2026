#pragma once

#include "../EditorCommandProvider.h"

namespace editor {

class EditorBlenderSceneImportCommandProvider final
    : public EditorCommandProvider {
public:
    void RegisterCommands(EditorContext& context) const override;
};

} // namespace editor
