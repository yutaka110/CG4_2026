#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "EditorAssetRegistry.h"
#include "EditorSelection.h"

namespace editor {

enum class EditorPropertyKind {
    Bool,
    Int,
    UInt,
    Float,
    String,
    Vec2,
    Vec3,
    Vec4,
    Color,
    Enum,
    AssetRef,
    ObjectRef,
};

struct EditorPropertyDescriptor {
    EditorDomainId domain = EditorDomainId::Unknown;
    std::string name;
    std::string displayName;
    EditorPropertyKind kind = EditorPropertyKind::String;
    std::string category;
    std::string valueType;
    EditorAssetKind assetKind = EditorAssetKind::Unknown;
    std::vector<std::string> enumOptions;
    float minValue = 0.0f;
    float maxValue = 0.0f;
    bool hasRange = false;
    bool readOnly = false;
    bool runtimeOnly = false;
};

class EditorPropertyRegistry {
public:
    void Clear();
    bool Register(EditorPropertyDescriptor descriptor);

    const EditorPropertyDescriptor* Find(
        EditorDomainId domain,
        std::string_view name) const;
    std::vector<const EditorPropertyDescriptor*> FindByDomain(EditorDomainId domain) const;

    std::size_t Count() const { return descriptors_.size(); }
    uint32_t Revision() const { return revision_; }
    const std::vector<EditorPropertyDescriptor>& Descriptors() const { return descriptors_; }

private:
    void Touch();

    std::vector<EditorPropertyDescriptor> descriptors_;
    uint32_t revision_ = 0;
};

void RegisterBuiltInCourseObjectProperties(EditorPropertyRegistry& registry);
const char* ToString(EditorPropertyKind kind);

} // namespace editor
