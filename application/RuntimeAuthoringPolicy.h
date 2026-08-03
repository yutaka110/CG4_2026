#pragma once

namespace app {

// Release is currently an optimized editor executable, so editor symbols are
// still linked. Presentation builds nevertheless must be runtime-read-only.
constexpr bool ResolveRuntimeAuthoringEnabled(bool releasePresentation) noexcept {
    return !releasePresentation;
}

#if defined(GE3_RELEASE_PRESENTATION) && GE3_RELEASE_PRESENTATION
inline constexpr bool kRuntimeAuthoringEnabled =
    ResolveRuntimeAuthoringEnabled(true);
static_assert(
    !kRuntimeAuthoringEnabled,
    "Release presentation builds must never permit runtime authoring.");
#else
inline constexpr bool kRuntimeAuthoringEnabled =
    ResolveRuntimeAuthoringEnabled(false);
#endif

} // namespace app
