#include "BlenderLevelJsonLoader.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace ge3::level {
namespace {

enum class JsonType : uint8_t {
    Null,
    Boolean,
    Number,
    String,
    Array,
    Object,
};

struct JsonValue {
    JsonType type = JsonType::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<JsonValue> array;
    std::unordered_map<std::string, JsonValue> object;
};

bool IsHexDigit(char value) noexcept {
    return (value >= '0' && value <= '9') ||
           (value >= 'a' && value <= 'f') ||
           (value >= 'A' && value <= 'F');
}

uint32_t HexValue(char value) noexcept {
    if (value >= '0' && value <= '9') return static_cast<uint32_t>(value - '0');
    if (value >= 'a' && value <= 'f') return static_cast<uint32_t>(value - 'a' + 10);
    return static_cast<uint32_t>(value - 'A' + 10);
}

bool IsLowerHexGuid(std::string_view value) noexcept {
    if (value.size() != 32) return false;
    return std::all_of(value.begin(), value.end(), [](char character) {
        return (character >= '0' && character <= '9') ||
               (character >= 'a' && character <= 'f');
    });
}

void AppendUtf8(uint32_t codePoint, std::string& output) {
    if (codePoint <= 0x7Fu) {
        output.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7FFu) {
        output.push_back(static_cast<char>(0xC0u | (codePoint >> 6u)));
        output.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
    } else if (codePoint <= 0xFFFFu) {
        output.push_back(static_cast<char>(0xE0u | (codePoint >> 12u)));
        output.push_back(static_cast<char>(0x80u | ((codePoint >> 6u) & 0x3Fu)));
        output.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
    } else {
        output.push_back(static_cast<char>(0xF0u | (codePoint >> 18u)));
        output.push_back(static_cast<char>(0x80u | ((codePoint >> 12u) & 0x3Fu)));
        output.push_back(static_cast<char>(0x80u | ((codePoint >> 6u) & 0x3Fu)));
        output.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
    }
}

bool ValidateUtf8(std::string_view input, std::size_t& invalidOffset) noexcept {
    std::size_t index = 0;
    while (index < input.size()) {
        const uint8_t lead = static_cast<uint8_t>(input[index]);
        if (lead <= 0x7Fu) {
            ++index;
            continue;
        }

        uint32_t codePoint = 0;
        std::size_t continuationCount = 0;
        uint32_t minimum = 0;
        if (lead >= 0xC2u && lead <= 0xDFu) {
            continuationCount = 1;
            codePoint = lead & 0x1Fu;
            minimum = 0x80u;
        } else if (lead >= 0xE0u && lead <= 0xEFu) {
            continuationCount = 2;
            codePoint = lead & 0x0Fu;
            minimum = 0x800u;
        } else if (lead >= 0xF0u && lead <= 0xF4u) {
            continuationCount = 3;
            codePoint = lead & 0x07u;
            minimum = 0x10000u;
        } else {
            invalidOffset = index;
            return false;
        }

        if (index + continuationCount >= input.size()) {
            invalidOffset = index;
            return false;
        }
        for (std::size_t continuation = 1; continuation <= continuationCount; ++continuation) {
            const uint8_t value = static_cast<uint8_t>(input[index + continuation]);
            if ((value & 0xC0u) != 0x80u) {
                invalidOffset = index + continuation;
                return false;
            }
            codePoint = (codePoint << 6u) | (value & 0x3Fu);
        }
        if (codePoint < minimum || codePoint > 0x10FFFFu ||
            (codePoint >= 0xD800u && codePoint <= 0xDFFFu)) {
            invalidOffset = index;
            return false;
        }
        index += continuationCount + 1;
    }
    return true;
}

std::pair<std::size_t, std::size_t> LineAndColumnAt(
    std::string_view input,
    std::size_t offset) noexcept {
    std::size_t line = 1;
    std::size_t column = 1;
    const std::size_t end = (std::min)(offset, input.size());
    for (std::size_t index = 0; index < end; ++index) {
        if (input[index] == '\n') {
            ++line;
            column = 1;
        } else {
            ++column;
        }
    }
    return {line, column};
}

class JsonParser {
public:
    JsonParser(std::string_view input, const BlenderLevelJsonLimits& limits)
        : input_(input), limits_(limits) {}

    bool Parse(JsonValue& output, BlenderLevelLoadError& error) {
        SkipWhitespace();
        if (!ParseValue(output, 0, error)) return false;
        SkipWhitespace();
        if (!AtEnd()) return Fail(error, "Unexpected content after the root JSON value.");
        return true;
    }

private:
    bool AtEnd() const noexcept { return position_ >= input_.size(); }
    char Peek() const noexcept { return AtEnd() ? '\0' : input_[position_]; }

    char Advance() noexcept {
        if (AtEnd()) return '\0';
        const char value = input_[position_++];
        if (value == '\n') {
            ++line_;
            column_ = 1;
        } else {
            ++column_;
        }
        return value;
    }

    void SkipWhitespace() noexcept {
        while (!AtEnd()) {
            const char value = Peek();
            if (value != ' ' && value != '\t' && value != '\r' && value != '\n') break;
            Advance();
        }
    }

    bool Fail(BlenderLevelLoadError& error, std::string message) const {
        error.code = BlenderLevelLoadErrorCode::JsonSyntax;
        error.line = line_;
        error.column = column_;
        error.message = std::move(message);
        return false;
    }

    bool ConsumeLiteral(std::string_view literal, BlenderLevelLoadError& error) {
        for (char expected : literal) {
            if (AtEnd() || Advance() != expected) {
                return Fail(error, "Invalid JSON literal.");
            }
        }
        return true;
    }

    bool CountNode(BlenderLevelLoadError& error) {
        ++nodeCount_;
        if (nodeCount_ > limits_.maximumJsonNodes) {
            error.code = BlenderLevelLoadErrorCode::ResourceLimit;
            error.line = line_;
            error.column = column_;
            error.message = "JSON node count exceeds the configured limit.";
            return false;
        }
        return true;
    }

    bool ParseValue(
        JsonValue& output,
        std::size_t depth,
        BlenderLevelLoadError& error) {
        if (!CountNode(error)) return false;
        constexpr std::size_t kJsonEnvelopeDepth = 16u;
        const std::size_t jsonDepthLimit =
            limits_.maximumHierarchyDepth >
                    (std::numeric_limits<std::size_t>::max)() - kJsonEnvelopeDepth
                ? (std::numeric_limits<std::size_t>::max)()
                : limits_.maximumHierarchyDepth + kJsonEnvelopeDepth;
        if (depth > jsonDepthLimit) {
            error.code = BlenderLevelLoadErrorCode::ResourceLimit;
            error.line = line_;
            error.column = column_;
            error.message = "JSON nesting depth exceeds the configured limit.";
            return false;
        }
        if (AtEnd()) return Fail(error, "Expected a JSON value.");

        switch (Peek()) {
        case 'n':
            if (!ConsumeLiteral("null", error)) return false;
            output.type = JsonType::Null;
            return true;
        case 't':
            if (!ConsumeLiteral("true", error)) return false;
            output.type = JsonType::Boolean;
            output.boolean = true;
            return true;
        case 'f':
            if (!ConsumeLiteral("false", error)) return false;
            output.type = JsonType::Boolean;
            output.boolean = false;
            return true;
        case '"':
            output.type = JsonType::String;
            return ParseString(output.string, error);
        case '[':
            return ParseArray(output, depth, error);
        case '{':
            return ParseObject(output, depth, error);
        default:
            if (Peek() == '-' || (Peek() >= '0' && Peek() <= '9')) {
                output.type = JsonType::Number;
                return ParseNumber(output.number, error);
            }
            return Fail(error, "Unexpected character while reading a JSON value.");
        }
    }

    bool ParseString(std::string& output, BlenderLevelLoadError& error) {
        if (Advance() != '"') return Fail(error, "Expected a JSON string.");
        output.clear();
        while (!AtEnd()) {
            const char value = Advance();
            if (value == '"') return true;
            if (static_cast<unsigned char>(value) < 0x20u) {
                return Fail(error, "Unescaped control character in JSON string.");
            }
            if (value != '\\') {
                output.push_back(value);
            } else {
                if (AtEnd()) return Fail(error, "Incomplete JSON string escape.");
                const char escaped = Advance();
                switch (escaped) {
                case '"': output.push_back('"'); break;
                case '\\': output.push_back('\\'); break;
                case '/': output.push_back('/'); break;
                case 'b': output.push_back('\b'); break;
                case 'f': output.push_back('\f'); break;
                case 'n': output.push_back('\n'); break;
                case 'r': output.push_back('\r'); break;
                case 't': output.push_back('\t'); break;
                case 'u': {
                    uint32_t codePoint = 0;
                    if (!ParseUnicodeEscape(codePoint, error)) return false;
                    AppendUtf8(codePoint, output);
                    break;
                }
                default:
                    return Fail(error, "Unsupported JSON string escape.");
                }
            }
            if (output.size() > limits_.maximumStringBytes) {
                error.code = BlenderLevelLoadErrorCode::ResourceLimit;
                error.line = line_;
                error.column = column_;
                error.message = "JSON string exceeds the configured byte limit.";
                return false;
            }
        }
        return Fail(error, "Unterminated JSON string.");
    }

    bool ParseFourHex(uint32_t& output, BlenderLevelLoadError& error) {
        output = 0;
        for (uint32_t index = 0; index < 4; ++index) {
            if (AtEnd() || !IsHexDigit(Peek())) {
                return Fail(error, "Invalid JSON Unicode escape.");
            }
            output = (output << 4u) | HexValue(Advance());
        }
        return true;
    }

    bool ParseUnicodeEscape(uint32_t& output, BlenderLevelLoadError& error) {
        uint32_t first = 0;
        if (!ParseFourHex(first, error)) return false;
        if (first >= 0xD800u && first <= 0xDBFFu) {
            if (AtEnd() || Advance() != '\\' || AtEnd() || Advance() != 'u') {
                return Fail(error, "High surrogate is missing its low surrogate.");
            }
            uint32_t second = 0;
            if (!ParseFourHex(second, error)) return false;
            if (second < 0xDC00u || second > 0xDFFFu) {
                return Fail(error, "Invalid low surrogate in JSON string.");
            }
            output = 0x10000u + ((first - 0xD800u) << 10u) + (second - 0xDC00u);
            return true;
        }
        if (first >= 0xDC00u && first <= 0xDFFFu) {
            return Fail(error, "Unexpected low surrogate in JSON string.");
        }
        output = first;
        return true;
    }

    bool ParseNumber(double& output, BlenderLevelLoadError& error) {
        const std::size_t start = position_;
        if (Peek() == '-') Advance();
        if (AtEnd()) return Fail(error, "Incomplete JSON number.");

        if (Peek() == '0') {
            Advance();
            if (!AtEnd() && Peek() >= '0' && Peek() <= '9') {
                return Fail(error, "Leading zero in JSON number.");
            }
        } else if (Peek() >= '1' && Peek() <= '9') {
            while (!AtEnd() && Peek() >= '0' && Peek() <= '9') Advance();
        } else {
            return Fail(error, "Invalid integer part in JSON number.");
        }

        if (!AtEnd() && Peek() == '.') {
            Advance();
            if (AtEnd() || Peek() < '0' || Peek() > '9') {
                return Fail(error, "JSON fraction has no digits.");
            }
            while (!AtEnd() && Peek() >= '0' && Peek() <= '9') Advance();
        }
        if (!AtEnd() && (Peek() == 'e' || Peek() == 'E')) {
            Advance();
            if (!AtEnd() && (Peek() == '+' || Peek() == '-')) Advance();
            if (AtEnd() || Peek() < '0' || Peek() > '9') {
                return Fail(error, "JSON exponent has no digits.");
            }
            while (!AtEnd() && Peek() >= '0' && Peek() <= '9') Advance();
        }

        const char* first = input_.data() + start;
        const char* last = input_.data() + position_;
        const auto converted = std::from_chars(first, last, output, std::chars_format::general);
        if (converted.ec != std::errc{} || converted.ptr != last || !std::isfinite(output)) {
            return Fail(error, "JSON number is outside the supported finite range.");
        }
        return true;
    }

    bool ParseArray(
        JsonValue& output,
        std::size_t depth,
        BlenderLevelLoadError& error) {
        output.type = JsonType::Array;
        Advance();
        SkipWhitespace();
        if (Peek() == ']') {
            Advance();
            return true;
        }
        while (!AtEnd()) {
            JsonValue element{};
            if (!ParseValue(element, depth + 1u, error)) return false;
            output.array.push_back(std::move(element));
            SkipWhitespace();
            if (Peek() == ']') {
                Advance();
                return true;
            }
            if (Advance() != ',') return Fail(error, "Expected ',' or ']' in JSON array.");
            SkipWhitespace();
        }
        return Fail(error, "Unterminated JSON array.");
    }

    bool ParseObject(
        JsonValue& output,
        std::size_t depth,
        BlenderLevelLoadError& error) {
        output.type = JsonType::Object;
        Advance();
        SkipWhitespace();
        if (Peek() == '}') {
            Advance();
            return true;
        }
        while (!AtEnd()) {
            if (Peek() != '"') return Fail(error, "Expected a string key in JSON object.");
            std::string key;
            if (!ParseString(key, error)) return false;
            SkipWhitespace();
            if (Advance() != ':') return Fail(error, "Expected ':' after JSON object key.");
            SkipWhitespace();
            JsonValue value{};
            if (!ParseValue(value, depth + 1u, error)) return false;
            if (!output.object.emplace(std::move(key), std::move(value)).second) {
                return Fail(error, "Duplicate key in JSON object.");
            }
            SkipWhitespace();
            if (Peek() == '}') {
                Advance();
                return true;
            }
            if (Advance() != ',') return Fail(error, "Expected ',' or '}' in JSON object.");
            SkipWhitespace();
        }
        return Fail(error, "Unterminated JSON object.");
    }

    std::string_view input_;
    const BlenderLevelJsonLimits& limits_;
    std::size_t position_ = 0;
    std::size_t line_ = 1;
    std::size_t column_ = 1;
    std::size_t nodeCount_ = 0;
};

const JsonValue* Member(const JsonValue& object, std::string_view name) {
    if (object.type != JsonType::Object) return nullptr;
    const auto found = object.object.find(std::string(name));
    return found == object.object.end() ? nullptr : &found->second;
}

bool IsKnownMember(
    std::string_view value,
    std::initializer_list<std::string_view> allowed) {
    return std::find(allowed.begin(), allowed.end(), value) != allowed.end();
}

class BlenderLevelDecoder {
public:
    BlenderLevelDecoder(
        const BlenderLevelJsonLimits& limits,
        std::filesystem::path source)
        : limits_(limits), source_(std::move(source)) {}

    BlenderLevelLoadResult Decode(const JsonValue& root) {
        BlenderLevelData data{};
        if (!DecodeRoot(root, data)) return Failure();
        BlenderLevelLoadResult result{};
        result.data = std::move(data);
        return result;
    }

private:
    BlenderLevelLoadResult Failure() {
        BlenderLevelLoadResult result{};
        result.error = std::move(error_);
        return result;
    }

    bool Fail(
        BlenderLevelLoadErrorCode code,
        std::string path,
        std::string message) {
        if (error_.code == BlenderLevelLoadErrorCode::None) {
            error_.code = code;
            error_.source = source_;
            error_.jsonPath = std::move(path);
            error_.message = std::move(message);
        }
        return false;
    }

    bool RequireType(
        const JsonValue* value,
        JsonType expected,
        std::string_view path,
        std::string_view typeName) {
        if (value == nullptr) {
            return Fail(
                BlenderLevelLoadErrorCode::SchemaViolation,
                std::string(path),
                "Required value is missing.");
        }
        if (value->type != expected) {
            return Fail(
                BlenderLevelLoadErrorCode::SchemaViolation,
                std::string(path),
                "Expected " + std::string(typeName) + ".");
        }
        return true;
    }

    bool CheckKnownMembers(
        const JsonValue& object,
        std::string_view path,
        std::initializer_list<std::string_view> allowed) {
        if (object.type != JsonType::Object) {
            return Fail(
                BlenderLevelLoadErrorCode::SchemaViolation,
                std::string(path),
                "Expected an object.");
        }
        for (const auto& [name, ignored] : object.object) {
            static_cast<void>(ignored);
            if (!IsKnownMember(name, allowed)) {
                return Fail(
                    BlenderLevelLoadErrorCode::SchemaViolation,
                    std::string(path) + "." + name,
                    "Unknown property is not allowed by Blender Level JSON v1.");
            }
        }
        return true;
    }

    bool ReadString(
        const JsonValue& object,
        std::string_view member,
        std::string_view path,
        std::string& output,
        std::size_t minimum,
        std::size_t maximum) {
        const std::string valuePath = std::string(path) + "." + std::string(member);
        const JsonValue* value = Member(object, member);
        if (!RequireType(value, JsonType::String, valuePath, "a string")) return false;
        if (value->string.size() < minimum || value->string.size() > maximum) {
            return Fail(
                BlenderLevelLoadErrorCode::SchemaViolation,
                valuePath,
                "String length is outside the allowed range.");
        }
        output = value->string;
        return true;
    }

    bool ReadNumber(
        const JsonValue& object,
        std::string_view member,
        std::string_view path,
        double& output) {
        const std::string valuePath = std::string(path) + "." + std::string(member);
        const JsonValue* value = Member(object, member);
        if (!RequireType(value, JsonType::Number, valuePath, "a number")) return false;
        output = value->number;
        return true;
    }

    bool ReadGuid(
        const JsonValue& object,
        std::string_view member,
        std::string_view path,
        std::string& output,
        bool requireUniqueObjectGuid) {
        if (!ReadString(object, member, path, output, 32, 32)) return false;
        const std::string valuePath = std::string(path) + "." + std::string(member);
        if (!IsLowerHexGuid(output)) {
            return Fail(
                BlenderLevelLoadErrorCode::SchemaViolation,
                valuePath,
                "GUID must contain exactly 32 lowercase hexadecimal characters.");
        }
        if (requireUniqueObjectGuid && !objectGuids_.insert(output).second) {
            return Fail(
                BlenderLevelLoadErrorCode::SchemaViolation,
                valuePath,
                "Object GUID is duplicated.");
        }
        return true;
    }

    bool ReadVector3(
        const JsonValue& object,
        std::string_view member,
        std::string_view path,
        BlenderLevelVector3& output) {
        const std::string valuePath = std::string(path) + "." + std::string(member);
        const JsonValue* value = Member(object, member);
        if (!RequireType(value, JsonType::Array, valuePath, "an array")) return false;
        if (value->array.size() != 3) {
            return Fail(
                BlenderLevelLoadErrorCode::SchemaViolation,
                valuePath,
                "Vector must contain exactly three numbers.");
        }
        double components[3]{};
        for (std::size_t index = 0; index < 3; ++index) {
            if (value->array[index].type != JsonType::Number ||
                !std::isfinite(value->array[index].number)) {
                return Fail(
                    BlenderLevelLoadErrorCode::SchemaViolation,
                    valuePath + "[" + std::to_string(index) + "]",
                    "Vector component must be a finite number.");
            }
            components[index] = value->array[index].number;
        }
        output = {components[0], components[1], components[2]};
        return true;
    }

    bool DecodeRoot(const JsonValue& root, BlenderLevelData& output) {
        constexpr std::string_view path = "$";
        if (!CheckKnownMembers(
                root,
                path,
                {"schema_version", "scene_guid", "name", "coordinate_system", "objects"})) {
            return false;
        }

        const JsonValue* schema = Member(root, "schema_version");
        if (!RequireType(schema, JsonType::Number, "$.schema_version", "a number")) return false;
        if (schema->number != static_cast<double>(kBlenderLevelSchemaVersion)) {
            return Fail(
                BlenderLevelLoadErrorCode::SchemaViolation,
                "$.schema_version",
                "Only Blender Level JSON schema_version 1 is supported.");
        }
        output.schemaVersion = kBlenderLevelSchemaVersion;
        if (!ReadGuid(root, "scene_guid", path, output.sceneGuid, false) ||
            !ReadString(root, "name", path, output.name, 1, 255)) {
            return false;
        }

        const JsonValue* coordinates = Member(root, "coordinate_system");
        if (!RequireType(
                coordinates,
                JsonType::Object,
                "$.coordinate_system",
                "an object") ||
            !DecodeCoordinateSystem(*coordinates, output.coordinateSystem)) {
            return false;
        }

        const JsonValue* objects = Member(root, "objects");
        if (!RequireType(objects, JsonType::Array, "$.objects", "an array")) return false;
        output.objects.reserve(objects->array.size());
        for (std::size_t index = 0; index < objects->array.size(); ++index) {
            BlenderLevelObject object{};
            const std::string objectPath = "$.objects[" + std::to_string(index) + "]";
            if (!DecodeObject(objects->array[index], objectPath, 0, object)) return false;
            output.objects.push_back(std::move(object));
        }
        return true;
    }

    bool DecodeCoordinateSystem(
        const JsonValue& value,
        BlenderLevelCoordinateSystem& output) {
        constexpr std::string_view path = "$.coordinate_system";
        if (!CheckKnownMembers(
                value,
                path,
                {"handedness", "up_axis", "forward_axis", "unit_scale_meters",
                 "transform_space", "rotation_unit", "rotation_order"}) ||
            !ReadString(value, "handedness", path, output.handedness, 1, 32) ||
            !ReadString(value, "up_axis", path, output.upAxis, 1, 8) ||
            !ReadString(value, "forward_axis", path, output.forwardAxis, 1, 8) ||
            !ReadNumber(value, "unit_scale_meters", path, output.unitScaleMeters) ||
            !ReadString(value, "transform_space", path, output.transformSpace, 1, 32) ||
            !ReadString(value, "rotation_unit", path, output.rotationUnit, 1, 32) ||
            !ReadString(value, "rotation_order", path, output.rotationOrder, 1, 8)) {
            return false;
        }
        if (output.handedness != "RIGHT_HANDED" ||
            output.upAxis != "Z" ||
            output.forwardAxis != "-Y" ||
            output.transformSpace != "LOCAL" ||
            output.rotationUnit != "DEGREES" ||
            output.rotationOrder != "XYZ" ||
            !std::isfinite(output.unitScaleMeters) ||
            output.unitScaleMeters <= 0.0) {
            return Fail(
                BlenderLevelLoadErrorCode::SchemaViolation,
                std::string(path),
                "Coordinate metadata does not match the Blender Level JSON v1 contract.");
        }
        return true;
    }

    bool DecodeObject(
        const JsonValue& value,
        const std::string& path,
        std::size_t depth,
        BlenderLevelObject& output) {
        if (depth > limits_.maximumHierarchyDepth) {
            return Fail(
                BlenderLevelLoadErrorCode::ResourceLimit,
                path,
                "Object hierarchy depth exceeds the configured limit.");
        }
        ++objectCount_;
        if (objectCount_ > limits_.maximumObjects) {
            return Fail(
                BlenderLevelLoadErrorCode::ResourceLimit,
                path,
                "Object count exceeds the configured limit.");
        }
        if (!CheckKnownMembers(
                value,
                path,
                {"guid", "type", "name", "spawn_kind", "enemy_type", "transform",
                 "file_name", "collider", "children"})) {
            return false;
        }
        if (!ReadGuid(value, "guid", path, output.guid, true) ||
            !ReadString(value, "type", path, output.blenderType, 1, 255) ||
            !ReadString(value, "name", path, output.name, 1, 255)) {
            return false;
        }

        std::string spawnKind;
        if (!ReadString(value, "spawn_kind", path, spawnKind, 1, 16)) return false;
        if (spawnKind == "NONE") output.spawnKind = BlenderSpawnKind::None;
        else if (spawnKind == "PLAYER") output.spawnKind = BlenderSpawnKind::Player;
        else if (spawnKind == "ENEMY") output.spawnKind = BlenderSpawnKind::Enemy;
        else {
            return Fail(
                BlenderLevelLoadErrorCode::SchemaViolation,
                path + ".spawn_kind",
                "spawn_kind must be NONE, PLAYER, or ENEMY.");
        }

        if (const JsonValue* enemy = Member(value, "enemy_type")) {
            if (enemy->type != JsonType::String) {
                return Fail(
                    BlenderLevelLoadErrorCode::SchemaViolation,
                    path + ".enemy_type",
                    "enemy_type must be a string.");
            }
            if (enemy->string == "DRONE") output.enemyType = BlenderEnemyType::Drone;
            else if (enemy->string == "TURRET") output.enemyType = BlenderEnemyType::Turret;
            else if (enemy->string == "BOSS") output.enemyType = BlenderEnemyType::Boss;
            else {
                return Fail(
                    BlenderLevelLoadErrorCode::SchemaViolation,
                    path + ".enemy_type",
                    "enemy_type must be DRONE, TURRET, or BOSS.");
            }
        } else if (output.spawnKind == BlenderSpawnKind::Enemy) {
            return Fail(
                BlenderLevelLoadErrorCode::SchemaViolation,
                path + ".enemy_type",
                "Enemy spawn object requires enemy_type.");
        }

        const JsonValue* transform = Member(value, "transform");
        if (!RequireType(transform, JsonType::Object, path + ".transform", "an object") ||
            !DecodeTransform(*transform, path + ".transform", output.transform)) {
            return false;
        }

        if (const JsonValue* fileName = Member(value, "file_name")) {
            if (fileName->type != JsonType::String || fileName->string.size() > 4096) {
                return Fail(
                    BlenderLevelLoadErrorCode::SchemaViolation,
                    path + ".file_name",
                    "file_name must be a string no longer than 4096 bytes.");
            }
            output.fileName = fileName->string;
        }

        if (const JsonValue* collider = Member(value, "collider")) {
            BlenderLevelCollider decoded{};
            if (!RequireType(collider, JsonType::Object, path + ".collider", "an object") ||
                !DecodeCollider(*collider, path + ".collider", decoded)) {
                return false;
            }
            output.collider = std::move(decoded);
        }

        if (const JsonValue* children = Member(value, "children")) {
            if (children->type != JsonType::Array) {
                return Fail(
                    BlenderLevelLoadErrorCode::SchemaViolation,
                    path + ".children",
                    "children must be an array.");
            }
            output.children.reserve(children->array.size());
            for (std::size_t index = 0; index < children->array.size(); ++index) {
                BlenderLevelObject child{};
                const std::string childPath =
                    path + ".children[" + std::to_string(index) + "]";
                if (!DecodeObject(children->array[index], childPath, depth + 1u, child)) {
                    return false;
                }
                output.children.push_back(std::move(child));
            }
        }
        return true;
    }

    bool DecodeTransform(
        const JsonValue& value,
        const std::string& path,
        BlenderLevelTransform& output) {
        if (!CheckKnownMembers(value, path, {"translation", "rotation", "scaling"}) ||
            !ReadVector3(value, "translation", path, output.translation) ||
            !ReadVector3(value, "rotation", path, output.rotationDegrees) ||
            !ReadVector3(value, "scaling", path, output.scaling)) {
            return false;
        }
        return true;
    }

    bool DecodeCollider(
        const JsonValue& value,
        const std::string& path,
        BlenderLevelCollider& output) {
        if (!CheckKnownMembers(value, path, {"type", "center", "size"}) ||
            !ReadString(value, "type", path, output.type, 1, 16) ||
            !ReadVector3(value, "center", path, output.center) ||
            !ReadVector3(value, "size", path, output.size)) {
            return false;
        }
        if (output.type != "BOX") {
            return Fail(
                BlenderLevelLoadErrorCode::SchemaViolation,
                path + ".type",
                "Only BOX collider is supported by Blender Level JSON v1.");
        }
        return true;
    }

    const BlenderLevelJsonLimits& limits_;
    std::filesystem::path source_;
    BlenderLevelLoadError error_{};
    std::unordered_set<std::string> objectGuids_;
    std::size_t objectCount_ = 0;
};

std::size_t CountObjects(const std::vector<BlenderLevelObject>& objects) noexcept {
    std::size_t count = objects.size();
    for (const BlenderLevelObject& object : objects) count += CountObjects(object.children);
    return count;
}

std::size_t CountSpawns(
    const std::vector<BlenderLevelObject>& objects,
    BlenderSpawnKind kind) noexcept {
    std::size_t count = 0;
    for (const BlenderLevelObject& object : objects) {
        if (object.spawnKind == kind) ++count;
        count += CountSpawns(object.children, kind);
    }
    return count;
}

const BlenderLevelObject* FindObjectRecursive(
    const std::vector<BlenderLevelObject>& objects,
    std::string_view guid) noexcept {
    for (const BlenderLevelObject& object : objects) {
        if (object.guid == guid) return &object;
        if (const BlenderLevelObject* child = FindObjectRecursive(object.children, guid)) {
            return child;
        }
    }
    return nullptr;
}

BlenderLevelLoadResult MakeReadFailure(
    BlenderLevelLoadErrorCode code,
    const std::filesystem::path& source,
    std::string message) {
    BlenderLevelLoadResult result{};
    result.error = BlenderLevelLoadError{
        code, source, {}, 0, 0, std::move(message)};
    return result;
}

} // namespace

std::size_t BlenderLevelData::ObjectCount() const noexcept {
    return CountObjects(objects);
}

std::size_t BlenderLevelData::PlayerSpawnCount() const noexcept {
    return CountSpawns(objects, BlenderSpawnKind::Player);
}

std::size_t BlenderLevelData::EnemySpawnCount() const noexcept {
    return CountSpawns(objects, BlenderSpawnKind::Enemy);
}

const BlenderLevelObject* BlenderLevelData::FindObject(std::string_view guid) const noexcept {
    return FindObjectRecursive(objects, guid);
}

BlenderLevelJsonLoader::BlenderLevelJsonLoader(BlenderLevelJsonLimits limits)
    : limits_(limits) {}

BlenderLevelLoadResult BlenderLevelJsonLoader::LoadFile(
    const std::filesystem::path& path) const {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream.is_open()) {
        return MakeReadFailure(
            BlenderLevelLoadErrorCode::FileOpenFailed,
            path,
            "Could not open Blender Level JSON file.");
    }

    const std::streamoff end = stream.tellg();
    if (end < 0) {
        return MakeReadFailure(
            BlenderLevelLoadErrorCode::FileReadFailed,
            path,
            "Could not determine Blender Level JSON file size.");
    }
    const auto fileBytes = static_cast<uint64_t>(end);
    if (fileBytes > static_cast<uint64_t>(limits_.maximumFileBytes) ||
        fileBytes > static_cast<uint64_t>((std::numeric_limits<std::size_t>::max)())) {
        return MakeReadFailure(
            BlenderLevelLoadErrorCode::FileTooLarge,
            path,
            "Blender Level JSON file exceeds the configured byte limit.");
    }

    std::string bytes(static_cast<std::size_t>(fileBytes), '\0');
    stream.seekg(0, std::ios::beg);
    if (!bytes.empty() &&
        !stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))) {
        return MakeReadFailure(
            BlenderLevelLoadErrorCode::FileReadFailed,
            path,
            "Could not read the complete Blender Level JSON file.");
    }
    return LoadJsonString(bytes, path);
}

BlenderLevelLoadResult BlenderLevelJsonLoader::LoadJsonString(
    std::string_view json,
    std::filesystem::path source) const {
    if (json.size() > limits_.maximumFileBytes) {
        return MakeReadFailure(
            BlenderLevelLoadErrorCode::FileTooLarge,
            source,
            "Blender Level JSON input exceeds the configured byte limit.");
    }
    if (json.size() >= 3 &&
        static_cast<uint8_t>(json[0]) == 0xEFu &&
        static_cast<uint8_t>(json[1]) == 0xBBu &&
        static_cast<uint8_t>(json[2]) == 0xBFu) {
        json.remove_prefix(3);
    }

    std::size_t invalidUtf8 = 0;
    if (!ValidateUtf8(json, invalidUtf8)) {
        const auto [line, column] = LineAndColumnAt(json, invalidUtf8);
        BlenderLevelLoadResult result{};
        result.error = BlenderLevelLoadError{
            BlenderLevelLoadErrorCode::InvalidUtf8,
            std::move(source),
            {},
            line,
            column,
            "Blender Level JSON contains invalid UTF-8."};
        return result;
    }

    JsonValue root{};
    BlenderLevelLoadError parseError{};
    parseError.source = source;
    JsonParser parser(json, limits_);
    if (!parser.Parse(root, parseError)) {
        BlenderLevelLoadResult result{};
        result.error = std::move(parseError);
        return result;
    }

    BlenderLevelDecoder decoder(limits_, std::move(source));
    return decoder.Decode(root);
}

const char* ToString(BlenderSpawnKind value) noexcept {
    switch (value) {
    case BlenderSpawnKind::None: return "NONE";
    case BlenderSpawnKind::Player: return "PLAYER";
    case BlenderSpawnKind::Enemy: return "ENEMY";
    }
    return "NONE";
}

const char* ToString(BlenderEnemyType value) noexcept {
    switch (value) {
    case BlenderEnemyType::None: return "NONE";
    case BlenderEnemyType::Drone: return "DRONE";
    case BlenderEnemyType::Turret: return "TURRET";
    case BlenderEnemyType::Boss: return "BOSS";
    }
    return "NONE";
}

const char* ToString(BlenderLevelLoadErrorCode value) noexcept {
    switch (value) {
    case BlenderLevelLoadErrorCode::None: return "None";
    case BlenderLevelLoadErrorCode::FileOpenFailed: return "FileOpenFailed";
    case BlenderLevelLoadErrorCode::FileReadFailed: return "FileReadFailed";
    case BlenderLevelLoadErrorCode::FileTooLarge: return "FileTooLarge";
    case BlenderLevelLoadErrorCode::InvalidUtf8: return "InvalidUtf8";
    case BlenderLevelLoadErrorCode::JsonSyntax: return "JsonSyntax";
    case BlenderLevelLoadErrorCode::SchemaViolation: return "SchemaViolation";
    case BlenderLevelLoadErrorCode::ResourceLimit: return "ResourceLimit";
    }
    return "None";
}

} // namespace ge3::level
