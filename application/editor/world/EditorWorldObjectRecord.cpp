#include "EditorWorldObjectRecord.h"

#include <atomic>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace editor {
namespace {

std::string HexGuid(uint64_t high, uint64_t low) {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(16) << high << std::setw(16) << low;
    return stream.str();
}

} // namespace

std::string EditorWorldObjectId::StableId() const {
    return BuildEditorWorldStableId(document, providerId, objectGuid);
}

std::string BuildEditorWorldStableId(
    const EditorDocumentId& document,
    std::string_view providerId,
    std::string_view objectGuid) {
    return "world:" + document.type + ":" + document.assetGuid + ":" +
        std::string(providerId) + ":" + std::string(objectGuid);
}

std::string MakeDeterministicEditorWorldGuid(
    std::string_view nameSpace,
    std::string_view kind,
    std::string_view legacyKey,
    uint64_t ordinal) {
    const std::string identity = std::string(nameSpace) + "|" + std::string(kind) + "|" +
        std::string(legacyKey) + "|" + std::to_string(ordinal);
    return HexGuid(
        EditorDocumentHash64(identity, 1469598103934665603ull),
        EditorDocumentHash64(identity, 1099511628211ull));
}

std::string GenerateEditorWorldGuid() {
    static std::atomic<uint64_t> sequence{1};
    const uint64_t counter = sequence.fetch_add(1, std::memory_order_relaxed);
    const uint64_t ticks = static_cast<uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    const std::string identity = std::to_string(ticks) + "|" + std::to_string(counter);
    return HexGuid(
        EditorDocumentHash64(identity, 1469598103934665603ull),
        EditorDocumentHash64(identity, 1099511628211ull));
}

} // namespace editor
