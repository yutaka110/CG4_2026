#pragma once

#include <cstdint>
#include <string>

#include "utils/math/Vector.h"

namespace editor {

struct EditorPropertyValue {
    bool boolValue = false;
    int intValue = 0;
    uint32_t uintValue = 0;
    float floatValue = 0.0f;
    Vector3 vec3Value = {0.0f, 0.0f, 0.0f};
    std::string stringValue;
};

} // namespace editor
