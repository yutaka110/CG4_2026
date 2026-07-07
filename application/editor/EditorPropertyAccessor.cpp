#include "EditorPropertyAccessor.h"

#include <cstdio>

namespace editor {

std::string FormatEditorPropertyValue(
    const EditorPropertyDescriptor& descriptor,
    const EditorPropertyValue& value) {
    char buffer[128]{};
    switch (descriptor.kind) {
    case EditorPropertyKind::Bool:
        return value.boolValue ? "true" : "false";
    case EditorPropertyKind::Int:
        std::snprintf(buffer, sizeof(buffer), "%d", value.intValue);
        return buffer;
    case EditorPropertyKind::UInt:
        std::snprintf(buffer, sizeof(buffer), "%u", value.uintValue);
        return buffer;
    case EditorPropertyKind::Float:
        std::snprintf(buffer, sizeof(buffer), "%.3f", value.floatValue);
        return buffer;
    case EditorPropertyKind::Vec2:
    case EditorPropertyKind::Vec3:
    case EditorPropertyKind::Vec4:
    case EditorPropertyKind::Color:
        std::snprintf(
            buffer,
            sizeof(buffer),
            "%.3f, %.3f, %.3f",
            value.vec3Value.x,
            value.vec3Value.y,
            value.vec3Value.z);
        return buffer;
    case EditorPropertyKind::String:
    case EditorPropertyKind::Enum:
    case EditorPropertyKind::AssetRef:
    case EditorPropertyKind::ObjectRef:
        return value.stringValue;
    }
    return {};
}

} // namespace editor
