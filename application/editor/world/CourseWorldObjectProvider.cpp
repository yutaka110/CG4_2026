#include "CourseWorldObjectProvider.h"

#include "CourseWorldIdentity.h"
#include "../../course/CourseAsset.h"

#include <iomanip>
#include <algorithm>
#include <sstream>
#include <utility>

namespace editor {
namespace {

std::string IndexedSortKey(std::string_view group, std::size_t index) {
    std::ostringstream stream;
    stream << group << ':' << std::setfill('0') << std::setw(10) << index;
    return stream.str();
}

EditorObjectHandle MakeHandle(
    const EditorDocumentId& document,
    std::string_view provider,
    std::string_view guid,
    EditorDomainId domain,
    uint64_t localIndex,
    std::string displayName) {
    EditorObjectHandle handle{};
    handle.domain = domain;
    handle.stableId = BuildEditorWorldStableId(document, provider, guid);
    handle.localIndex = localIndex;
    handle.displayName = std::move(displayName);
    return handle;
}

EditorWorldObjectRecord MakeVirtual(
    const EditorDocumentId& document,
    std::string_view provider,
    std::string_view guid,
    std::string displayName,
    std::string sortKey,
    const EditorObjectHandle& parent = {}) {
    EditorWorldObjectRecord record{};
    record.document = document;
    record.providerId = std::string(provider);
    record.objectGuid = std::string(guid);
    record.displayName = std::move(displayName);
    record.typeName = "Folder";
    record.sortKey = std::move(sortKey);
    record.handle = MakeHandle(
        document, provider, guid, EditorDomainId::Unknown, 0, record.displayName);
    record.parent = parent;
    record.virtualNode = true;
    return record;
}

EditorWorldObjectCapabilities CourseEditableCapabilities(bool rename, bool transform) {
    EditorWorldObjectCapabilities value =
        EditorWorldObjectCapability::Duplicate |
        EditorWorldObjectCapability::Delete |
        EditorWorldObjectCapability::Visibility |
        EditorWorldObjectCapability::Lock;
    if (rename) value = value | EditorWorldObjectCapability::Rename;
    if (transform) value = value | EditorWorldObjectCapability::Transform;
    return value;
}

class CourseWorldMutationPayload final : public IEditorWorldMutationPayload {
public:
    std::vector<CourseTerrainPlacement> terrain;
    std::vector<CourseRockCluster> rocks;
    std::vector<CourseCameraKey> cameras;
    std::vector<CourseEventMarker> events;

    std::size_t EstimatedBytes() const noexcept override {
        std::size_t bytes = sizeof(CourseWorldMutationPayload) +
            terrain.capacity() * sizeof(CourseTerrainPlacement) +
            rocks.capacity() * sizeof(CourseRockCluster) +
            cameras.capacity() * sizeof(CourseCameraKey) +
            events.capacity() * sizeof(CourseEventMarker);
        for (const auto& value : terrain) {
            bytes += value.id.capacity() + value.meshId.capacity() + value.editorGuid.capacity() + 3;
        }
        for (const auto& value : rocks) {
            bytes += value.id.capacity() + value.meshId.capacity() + value.editorGuid.capacity() + 3;
            bytes += value.instanceOverrides.capacity() * sizeof(CourseRockCluster::InstanceTransformOverride);
        }
        for (const auto& value : cameras) bytes += value.editorGuid.capacity() + 1;
        for (const auto& value : events) {
            bytes += value.type.capacity() + value.id.capacity() + value.payload.capacity() +
                value.editorGuid.capacity() + 4;
        }
        return bytes;
    }
};

template <typename T>
auto FindGuid(std::vector<T>& values, std::string_view guid) {
    return std::find_if(values.begin(), values.end(), [&](const T& value) {
        return value.editorGuid == guid;
    });
}

template <typename T>
std::string UniqueCopyName(const std::vector<T>& values, const std::string& original) {
    const std::string base = original.empty() ? "Copy" : original + "_copy";
    std::string candidate = base;
    uint32_t suffix = 2;
    const auto exists = [&](const std::string& value) {
        return std::any_of(values.begin(), values.end(), [&](const T& item) {
            return item.id == value;
        });
    };
    while (exists(candidate)) candidate = base + std::to_string(suffix++);
    return candidate;
}

template <typename T>
bool NameExists(
    const std::vector<T>& values,
    std::string_view name,
    std::string_view excludedGuid) {
    return std::any_of(values.begin(), values.end(), [&](const T& item) {
        return item.editorGuid != excludedGuid && item.id == name;
    });
}

EditorWorldObjectId MakeId(
    const EditorDocumentId& document,
    std::string_view provider,
    const std::string& guid) {
    return EditorWorldObjectId{document, std::string(provider), guid};
}

} // namespace

void CourseWorldObjectProvider::Bind(CourseAsset* course, EditorDocumentId document) {
    course_ = course;
    document_ = std::move(document);
}

std::size_t CourseWorldObjectProvider::EnsurePersistentIdentities() {
    if (course_ == nullptr || !document_.IsValid()) return 0;
    return EnsureCourseWorldObjectGuids(*course_, document_.assetGuid);
}

bool CourseWorldObjectProvider::Enumerate(
    EditorWorldProviderEnumeration* output,
    std::string* errorMessage) const {
    if (output == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Course world enumeration output is null.";
        return false;
    }
    output->objects.clear();
    output->diagnostics.clear();
    if (course_ == nullptr || !document_.IsValid()) return true;

    output->objects.reserve(
        5 + course_->terrainPlacements.size() + course_->rockClusters.size() +
        course_->cameraKeys.size() + course_->events.size());

    ValidateCourseWorldObjectGuids(*course_, &output->diagnostics);
    const std::string_view provider = ProviderId();
    EditorWorldObjectRecord root = MakeVirtual(
        document_, provider, "root", course_->name, "00");
    const EditorObjectHandle rootHandle = root.handle;
    output->objects.push_back(std::move(root));
    EditorWorldObjectRecord terrainFolder = MakeVirtual(
        document_, provider, "folder-terrain", "Terrain Placements", "01", rootHandle);
    EditorWorldObjectRecord rockFolder = MakeVirtual(
        document_, provider, "folder-rocks", "Rock Clusters", "02", rootHandle);
    EditorWorldObjectRecord cameraFolder = MakeVirtual(
        document_, provider, "folder-cameras", "Camera Keys", "03", rootHandle);
    EditorWorldObjectRecord eventFolder = MakeVirtual(
        document_, provider, "folder-events", "Events", "04", rootHandle);
    const EditorObjectHandle terrainParent = terrainFolder.handle;
    const EditorObjectHandle rockParent = rockFolder.handle;
    const EditorObjectHandle cameraParent = cameraFolder.handle;
    const EditorObjectHandle eventParent = eventFolder.handle;
    output->objects.push_back(std::move(terrainFolder));
    output->objects.push_back(std::move(rockFolder));
    output->objects.push_back(std::move(cameraFolder));
    output->objects.push_back(std::move(eventFolder));

    const auto append = [&](std::string_view guid,
                            EditorDomainId domain,
                            std::size_t index,
                            std::string displayName,
                            std::string typeName,
                            std::string sortGroup,
                            const EditorObjectHandle& parent,
                            bool rename,
                            bool transform,
                            bool visible,
                            bool locked) {
        EditorWorldObjectRecord record{};
        record.document = document_;
        record.providerId = std::string(provider);
        record.objectGuid = std::string(guid);
        record.displayName = displayName.empty()
            ? typeName + " " + std::to_string(index)
            : std::move(displayName);
        record.typeName = std::move(typeName);
        record.sortKey = IndexedSortKey(sortGroup, index);
        record.capabilities = CourseEditableCapabilities(rename, transform);
        record.handle = MakeHandle(
            document_, provider, guid, domain, index, record.displayName);
        record.parent = parent;
        record.visible = visible;
        record.locked = locked;
        output->objects.push_back(std::move(record));
    };
    for (std::size_t index = 0; index < course_->terrainPlacements.size(); ++index) {
        const CourseTerrainPlacement& value = course_->terrainPlacements[index];
        append(value.editorGuid, EditorDomainId::CourseTerrainPlacement, index,
            value.id, "Terrain Placement", "01", terrainParent,
            true, true, value.editorVisible, value.editorLocked);
    }
    for (std::size_t index = 0; index < course_->rockClusters.size(); ++index) {
        const CourseRockCluster& value = course_->rockClusters[index];
        append(value.editorGuid, EditorDomainId::CourseRockCluster, index,
            value.id, "Rock Cluster", "02", rockParent,
            true, true, value.editorVisible, value.editorLocked);
    }
    for (std::size_t index = 0; index < course_->cameraKeys.size(); ++index) {
        const CourseCameraKey& value = course_->cameraKeys[index];
        append(value.editorGuid, EditorDomainId::CourseCameraKey, index,
            "Camera Key " + std::to_string(index), "Camera Key", "03", cameraParent,
            false, true, value.editorVisible, value.editorLocked);
    }
    for (std::size_t index = 0; index < course_->events.size(); ++index) {
        const CourseEventMarker& value = course_->events[index];
        append(value.editorGuid, EditorDomainId::CourseEventMarker, index,
            value.id, "Event Marker", "04", eventParent,
            true, false, value.editorVisible, value.editorLocked);
    }
    return true;
}

bool CourseWorldObjectProvider::Resolve(
    const EditorObjectHandle& handle,
    EditorWorldObjectRecord* record) const {
    EditorWorldProviderEnumeration enumeration{};
    if (!Enumerate(&enumeration, nullptr)) return false;
    for (const EditorWorldObjectRecord& candidate : enumeration.objects) {
        if (!candidate.handle.SameObject(handle)) continue;
        if (record != nullptr) *record = candidate;
        return true;
    }
    return false;
}

bool CourseWorldObjectProvider::BuildMutation(
    const EditorWorldProviderMutationRequest& request,
    EditorWorldMutationPlan* plan,
    std::string* errorMessage) const {
    if (course_ == nullptr || plan == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Course World mutation target is unavailable.";
        return false;
    }
    if (request.kind == EditorWorldMutationKind::Reparent) {
        if (errorMessage != nullptr) {
            *errorMessage = "Course object categories are fixed and cannot be reparented.";
        }
        return false;
    }
    if (request.kind == EditorWorldMutationKind::Rename &&
        (request.name.empty() || request.name.find_first_of("|\r\n") != std::string::npos)) {
        if (errorMessage != nullptr) *errorMessage = "Course object name is empty or invalid.";
        return false;
    }

    auto before = std::make_shared<CourseWorldMutationPayload>();
    before->terrain = course_->terrainPlacements;
    before->rocks = course_->rockClusters;
    before->cameras = course_->cameraKeys;
    before->events = course_->events;
    auto after = std::make_shared<CourseWorldMutationPayload>(*before);
    std::vector<EditorWorldObjectId> selection;

    for (const EditorWorldObjectId& target : request.targets) {
        bool found = false;
        auto terrain = FindGuid(after->terrain, target.objectGuid);
        if (terrain != after->terrain.end()) {
            found = true;
            if (request.kind == EditorWorldMutationKind::Rename) {
                if (NameExists(after->terrain, request.name, target.objectGuid)) {
                    if (errorMessage != nullptr) {
                        *errorMessage = "A Terrain Placement with that name already exists.";
                    }
                    return false;
                }
                terrain->id = request.name;
            }
            if (request.kind == EditorWorldMutationKind::SetVisibility) terrain->editorVisible = request.value;
            if (request.kind == EditorWorldMutationKind::SetLocked) terrain->editorLocked = request.value;
            if (request.kind == EditorWorldMutationKind::Duplicate) {
                CourseTerrainPlacement copy = *terrain;
                copy.editorGuid = GenerateEditorWorldGuid();
                copy.id = UniqueCopyName(after->terrain, terrain->id);
                terrain = after->terrain.insert(terrain + 1, std::move(copy));
                selection.push_back(MakeId(document_, ProviderId(), terrain->editorGuid));
            } else if (request.kind == EditorWorldMutationKind::Delete) {
                after->terrain.erase(terrain);
            } else {
                selection.push_back(target);
            }
        }
        auto rock = FindGuid(after->rocks, target.objectGuid);
        if (!found && rock != after->rocks.end()) {
            found = true;
            if (request.kind == EditorWorldMutationKind::Rename) {
                if (NameExists(after->rocks, request.name, target.objectGuid)) {
                    if (errorMessage != nullptr) {
                        *errorMessage = "A Rock Cluster with that name already exists.";
                    }
                    return false;
                }
                rock->id = request.name;
            }
            if (request.kind == EditorWorldMutationKind::SetVisibility) rock->editorVisible = request.value;
            if (request.kind == EditorWorldMutationKind::SetLocked) rock->editorLocked = request.value;
            if (request.kind == EditorWorldMutationKind::Duplicate) {
                CourseRockCluster copy = *rock;
                copy.editorGuid = GenerateEditorWorldGuid();
                copy.id = UniqueCopyName(after->rocks, rock->id);
                rock = after->rocks.insert(rock + 1, std::move(copy));
                selection.push_back(MakeId(document_, ProviderId(), rock->editorGuid));
            } else if (request.kind == EditorWorldMutationKind::Delete) {
                after->rocks.erase(rock);
            } else {
                selection.push_back(target);
            }
        }
        auto camera = FindGuid(after->cameras, target.objectGuid);
        if (!found && camera != after->cameras.end()) {
            found = true;
            if (request.kind == EditorWorldMutationKind::Rename) {
                if (errorMessage != nullptr) *errorMessage = "Course Camera Keys cannot be renamed.";
                return false;
            }
            if (request.kind == EditorWorldMutationKind::SetVisibility) camera->editorVisible = request.value;
            if (request.kind == EditorWorldMutationKind::SetLocked) camera->editorLocked = request.value;
            if (request.kind == EditorWorldMutationKind::Duplicate) {
                CourseCameraKey copy = *camera;
                copy.editorGuid = GenerateEditorWorldGuid();
                camera = after->cameras.insert(camera + 1, std::move(copy));
                selection.push_back(MakeId(document_, ProviderId(), camera->editorGuid));
            } else if (request.kind == EditorWorldMutationKind::Delete) {
                after->cameras.erase(camera);
            } else {
                selection.push_back(target);
            }
        }
        auto event = FindGuid(after->events, target.objectGuid);
        if (!found && event != after->events.end()) {
            found = true;
            if (request.kind == EditorWorldMutationKind::Rename) {
                if (NameExists(after->events, request.name, target.objectGuid)) {
                    if (errorMessage != nullptr) {
                        *errorMessage = "A Course Event with that name already exists.";
                    }
                    return false;
                }
                event->id = request.name;
            }
            if (request.kind == EditorWorldMutationKind::SetVisibility) event->editorVisible = request.value;
            if (request.kind == EditorWorldMutationKind::SetLocked) event->editorLocked = request.value;
            if (request.kind == EditorWorldMutationKind::Duplicate) {
                CourseEventMarker copy = *event;
                copy.editorGuid = GenerateEditorWorldGuid();
                copy.id = UniqueCopyName(after->events, event->id);
                event = after->events.insert(event + 1, std::move(copy));
                selection.push_back(MakeId(document_, ProviderId(), event->editorGuid));
            } else if (request.kind == EditorWorldMutationKind::Delete) {
                after->events.erase(event);
            } else {
                selection.push_back(target);
            }
        }
        if (!found) {
            if (errorMessage != nullptr) *errorMessage = "Course World object no longer exists.";
            return false;
        }
    }

    plan->before = EditorWorldMutationState{
        std::string(ProviderId()), document_, std::move(before)};
    plan->after = EditorWorldMutationState{
        std::string(ProviderId()), document_, std::move(after)};
    plan->resultingSelection = std::move(selection);
    plan->label = std::string(ToString(request.kind)) + " Course World Object";
    return true;
}

bool CourseWorldObjectProvider::ApplyMutationState(
    const EditorWorldMutationState& state,
    std::string* errorMessage) {
    if (course_ == nullptr || state.providerId != ProviderId() || state.document != document_) {
        if (errorMessage != nullptr) *errorMessage = "Course World mutation state target mismatch.";
        return false;
    }
    const auto* payload = dynamic_cast<const CourseWorldMutationPayload*>(state.payload.get());
    if (payload == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Course World mutation payload type mismatch.";
        return false;
    }
    course_->terrainPlacements = payload->terrain;
    course_->rockClusters = payload->rocks;
    course_->cameraKeys = payload->cameras;
    course_->events = payload->events;
    return true;
}

} // namespace editor
