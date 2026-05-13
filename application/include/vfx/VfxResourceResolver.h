#pragma once

#include "vfx/VfxResources.h"

struct AppVfxRuntimeState;

namespace vfx {
class VfxResourceResolver {
public:
    VfxTypedResourceSet SelectPassResources(const AppVfxRuntimeState& runtimeState) const;
    VfxTypedResourceSet SelectBeamDedicatedResources() const;
};
} // namespace vfx
