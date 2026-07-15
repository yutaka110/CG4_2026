#include "EditorPropertyAccessor.h"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>

namespace editor {
namespace {

void SetParseError(std::string* errorMessage, std::string message) {
    if (errorMessage != nullptr) {
        *errorMessage = std::move(message);
    }
}

std::string_view Trim(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\n' || text.front() == '\r')) {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\n' || text.back() == '\r')) {
        text.remove_suffix(1);
    }
    return text;
}

bool ParseInt(std::string_view text, int& outValue) {
    text = Trim(text);
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto result = std::from_chars(begin, end, outValue);
    return result.ec == std::errc{} && result.ptr == end;
}

bool ParseUInt(std::string_view text, uint32_t& outValue) {
    text = Trim(text);
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto result = std::from_chars(begin, end, outValue);
    return result.ec == std::errc{} && result.ptr == end;
}

bool ParseFloat(std::string_view text, float& outValue) {
    const std::string trimmed(Trim(text));
    if (trimmed.empty()) {
        return false;
    }
    char* parseEnd = nullptr;
    outValue = std::strtof(trimmed.c_str(), &parseEnd);
    return parseEnd != nullptr && *parseEnd == '\0';
}

bool ParseVec3(std::string_view text, Vector3& outValue) {
    float values[3]{};
    std::size_t component = 0;
    while (component < 3) {
        const std::size_t comma = text.find(',');
        const std::string_view token =
            comma == std::string_view::npos ? text : text.substr(0, comma);
        if (!ParseFloat(token, values[component])) {
            return false;
        }
        ++component;
        if (component == 3 && comma != std::string_view::npos) {
            return false;
        }
        if (comma == std::string_view::npos) {
            break;
        }
        text.remove_prefix(comma + 1);
    }
    if (component != 3) {
        return false;
    }
    outValue = {values[0], values[1], values[2]};
    return true;
}

bool ContainsEnumOption(const EditorPropertyDescriptor& descriptor, std::string_view value) {
    return std::any_of(
        descriptor.enumOptions.begin(),
        descriptor.enumOptions.end(),
        [&](const std::string& option) {
            return option == value;
        });
}

} // namespace

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
    case EditorPropertyKind::Array:
    case EditorPropertyKind::Map:
    case EditorPropertyKind::Struct:
        return value.stringValue;
    }
    return {};
}

bool ParseEditorPropertyValue(
    const EditorPropertyDescriptor& descriptor,
    std::string_view text,
    EditorPropertyValue& outValue,
    std::string* errorMessage) {
    text = Trim(text);
    switch (descriptor.kind) {
    case EditorPropertyKind::Bool:
        if (text == "true" || text == "1") {
            outValue.boolValue = true;
            return true;
        }
        if (text == "false" || text == "0") {
            outValue.boolValue = false;
            return true;
        }
        SetParseError(errorMessage, "Failed to parse bool property value.");
        return false;
    case EditorPropertyKind::Int:
        if (ParseInt(text, outValue.intValue)) {
            return true;
        }
        SetParseError(errorMessage, "Failed to parse int property value.");
        return false;
    case EditorPropertyKind::UInt:
        if (ParseUInt(text, outValue.uintValue)) {
            return true;
        }
        SetParseError(errorMessage, "Failed to parse uint property value.");
        return false;
    case EditorPropertyKind::Float:
        if (ParseFloat(text, outValue.floatValue)) {
            return true;
        }
        SetParseError(errorMessage, "Failed to parse float property value.");
        return false;
    case EditorPropertyKind::Vec2:
    case EditorPropertyKind::Vec3:
    case EditorPropertyKind::Vec4:
    case EditorPropertyKind::Color:
        if (ParseVec3(text, outValue.vec3Value)) {
            return true;
        }
        SetParseError(errorMessage, "Failed to parse vector property value.");
        return false;
    case EditorPropertyKind::String:
    case EditorPropertyKind::AssetRef:
    case EditorPropertyKind::ObjectRef:
    case EditorPropertyKind::Array:
    case EditorPropertyKind::Map:
    case EditorPropertyKind::Struct:
        outValue.stringValue = std::string(text);
        return true;
    case EditorPropertyKind::Enum:
        if (!descriptor.enumOptions.empty() && !ContainsEnumOption(descriptor, text)) {
            SetParseError(errorMessage, "Failed to parse enum property value.");
            return false;
        }
        outValue.stringValue = std::string(text);
        return true;
    }
    SetParseError(errorMessage, "Unsupported property value kind.");
    return false;
}

} // namespace editor
