#pragma once

#include <cstdint>
#include <string_view>

#include "../core/EditorError.h"

namespace editor {

class EditorPlaySnapshot;
class EditorRuntimeChangeSet;

class IEditorPlayIsolationProvider {
public:
    virtual ~IEditorPlayIsolationProvider() = default;

    virtual std::string_view Id() const noexcept = 0;
    virtual std::string_view Label() const noexcept = 0;
    virtual int Order() const noexcept { return 0; }
    virtual bool Available() const noexcept = 0;
    virtual bool Capture(EditorPlaySnapshot& snapshot, EditorError* error) const = 0;
    virtual bool Restore(const EditorPlaySnapshot& snapshot, EditorError* error) const = 0;
    virtual bool BuildRuntimeChangeSet(
        const EditorPlaySnapshot& snapshot,
        EditorRuntimeChangeSet& changes,
        EditorError* error) const = 0;
    virtual uint64_t AuthoringFingerprint() const = 0;
};

} // namespace editor
