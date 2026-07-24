#include "EditorSceneDocumentProvider.h"
#include "../scene/EditorPatrolComponent.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>

namespace editor {
namespace {

void AddIssue(
    EditorDocumentValidationReport& report,
    EditorDocumentIssueSeverity severity,
    std::string code,
    std::string message) {
    report.issues.push_back({severity, std::move(code), std::move(message)});
}

} // namespace

bool EditorSceneDocumentProvider::SupportsPath(const std::filesystem::path& path) const {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return extension == ".scene";
}

bool EditorSceneDocumentProvider::ReadSource(
    const std::filesystem::path& path,
    EditorDocumentContent* content,
    std::string* errorMessage) const {
    if (content == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Scene document content output is null.";
        return false;
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        std::error_code error;
        if (std::filesystem::exists(path, error)) {
            if (errorMessage != nullptr) *errorMessage = "Could not open Scene document: " + path.string();
            return false;
        }
        return Encode(EditorScene{}, content, errorMessage);
    }
    content->bytes.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    content->schemaVersion = 0;
    std::istringstream header(std::string(content->bytes.begin(), content->bytes.end()));
    std::string marker;
    if (!(header >> marker >> content->schemaVersion) || marker != "SCENE") {
        if (errorMessage != nullptr) *errorMessage = "Scene document header is invalid.";
        return false;
    }
    return true;
}

bool EditorSceneDocumentProvider::Serialize(
    const EditorDocumentId& id,
    EditorDocumentContent* content,
    std::string* errorMessage) const {
    const EditorScene* scene = Scene(id);
    if (scene == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Scene live model is unavailable.";
        return false;
    }
    return Encode(*scene, content, errorMessage);
}

bool EditorSceneDocumentProvider::Deserialize(
    const EditorDocumentId& id,
    const EditorDocumentContent& content,
    std::string* errorMessage) {
    EditorScene decoded{};
    if (!Decode(content, &decoded, errorMessage)) return false;
    const uint64_t nextRevision = Scene(id) != nullptr ? Scene(id)->revision + 1 : 1;
    decoded.revision = nextRevision;
    scenes_[id.Key()] = std::move(decoded);
    return true;
}

EditorDocumentValidationReport EditorSceneDocumentProvider::Validate(
    const EditorDocumentContent& content) const {
    EditorDocumentValidationReport report{};
    if (content.bytes.size() > 64u * 1024u * 1024u) {
        AddIssue(report, EditorDocumentIssueSeverity::Error, "scene.size",
            "Scene exceeds the 64 MiB safety limit.");
        return report;
    }
    EditorScene scene{};
    std::string error;
    if (!Decode(content, &scene, &error)) {
        AddIssue(report, EditorDocumentIssueSeverity::Error, "scene.parse", std::move(error));
        return report;
    }
    const EditorSceneValidationReport sceneReport = scene.Validate();
    for (const std::string& message : sceneReport.errors) {
        AddIssue(report, EditorDocumentIssueSeverity::Error, "scene.model", message);
    }
    for (const std::string& message : sceneReport.warnings) {
        AddIssue(report, EditorDocumentIssueSeverity::Warning, "scene.reference", message);
    }
    return report;
}

bool EditorSceneDocumentProvider::Migrate(
    const EditorDocumentContent& source,
    EditorDocumentContent* migrated,
    EditorDocumentMigrationReport* report,
    std::string* errorMessage) const {
    if (migrated == nullptr || source.schemaVersion == 0 ||
        source.schemaVersion > kEditorSceneSchemaVersion) {
        if (errorMessage != nullptr) *errorMessage = "Scene document has no compatible migration path.";
        return false;
    }
    if (source.schemaVersion < kEditorSceneSchemaVersion) {
        EditorScene legacy{};
        if (!Decode(source, &legacy, errorMessage) || !Encode(legacy, migrated, errorMessage)) {
            return false;
        }
        if (report != nullptr) {
            report->migrated = true;
            report->sourceSchemaVersion = source.schemaVersion;
            report->targetSchemaVersion = kEditorSceneSchemaVersion;
            if (source.schemaVersion == 1) {
                report->notes.push_back(
                    "Scene schema v1 was upgraded with an empty persistent "
                    "Prefab instance table.");
            }
            report->notes.push_back(
                "Legacy Scene Entities were upgraded with Runtime Enabled "
                "set to true.");
        }
        return true;
    }
    *migrated = source;
    if (report != nullptr) {
        report->migrated = false;
        report->sourceSchemaVersion = source.schemaVersion;
        report->targetSchemaVersion = source.schemaVersion;
    }
    return true;
}

void EditorSceneDocumentProvider::Release(const EditorDocumentId& id) {
    scenes_.erase(id.Key());
}

EditorScene* EditorSceneDocumentProvider::Scene(const EditorDocumentId& id) {
    const auto found = scenes_.find(id.Key());
    return found == scenes_.end() ? nullptr : &found->second;
}

const EditorScene* EditorSceneDocumentProvider::Scene(const EditorDocumentId& id) const {
    const auto found = scenes_.find(id.Key());
    return found == scenes_.end() ? nullptr : &found->second;
}

bool EditorSceneDocumentProvider::Encode(
    const EditorScene& scene,
    EditorDocumentContent* content,
    std::string* errorMessage) {
    if (content == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Scene serialization output is null.";
        return false;
    }
    const EditorSceneValidationReport validation = scene.Validate();
    if (!validation.Succeeded()) {
        if (errorMessage != nullptr) *errorMessage = validation.errors.front();
        return false;
    }
    std::ostringstream output;
    output << "SCENE " << kEditorSceneSchemaVersion << '\n';
    for (const EditorSceneEntity& entity : scene.entities) {
        output << "ENTITY " << std::quoted(entity.guid) << ' '
               << std::quoted(entity.parentGuid) << ' ' << std::quoted(entity.name) << ' '
               << (entity.visible ? 1 : 0) << ' ' << (entity.locked ? 1 : 0) << ' '
               << (entity.runtimeEnabled ? 1 : 0) << '\n';
        for (const EditorSceneComponent& component : entity.components) {
            output << "COMPONENT " << std::quoted(entity.guid) << ' '
                   << std::quoted(component.typeId) << ' ' << (component.enabled ? 1 : 0) << '\n';
            for (const EditorSceneProperty& property : component.properties) {
                output << "PROPERTY " << std::quoted(entity.guid) << ' '
                       << std::quoted(component.typeId) << ' ' << std::quoted(property.name) << ' '
                       << std::quoted(property.value) << '\n';
            }
            for (const EditorSceneObjectReference& reference : component.references) {
                output << "REFERENCE " << std::quoted(entity.guid) << ' '
                       << std::quoted(component.typeId) << ' ' << std::quoted(reference.property) << ' '
                       << std::quoted(reference.entityGuid) << ' ' << std::quoted(reference.assetGuid) << '\n';
            }
        }
    }
    for (const EditorScenePrefabInstance& instance : scene.prefabInstances) {
        output << "PREFAB_INSTANCE " << std::quoted(instance.instanceGuid) << ' '
               << std::quoted(instance.prefabAssetGuid) << ' '
               << std::quoted(instance.rootEntityGuid) << ' '
               << instance.sourceSchemaVersion << ' '
               << static_cast<uint32_t>(instance.status) << '\n';
        for (const EditorScenePrefabEntityBinding& binding : instance.bindings) {
            output << "PREFAB_BINDING " << std::quoted(instance.instanceGuid) << ' '
                   << std::quoted(binding.sourceEntityGuid) << ' '
                   << std::quoted(binding.instanceEntityGuid) << '\n';
        }
        for (const EditorScenePrefabOverride& value : instance.overrides) {
            output << "PREFAB_OVERRIDE " << std::quoted(instance.instanceGuid) << ' '
                   << std::quoted(value.id) << ' '
                   << static_cast<uint32_t>(value.kind) << ' '
                   << std::quoted(value.sourceEntityGuid) << ' '
                   << std::quoted(value.instanceEntityGuid) << ' '
                   << std::quoted(value.componentTypeId) << ' '
                   << std::quoted(value.propertyName) << ' '
                   << std::quoted(value.inheritedValue) << ' '
                   << std::quoted(value.instanceValue) << '\n';
        }
    }
    output << "END\n";
    const std::string bytes = output.str();
    content->schemaVersion = kEditorSceneSchemaVersion;
    content->bytes.assign(bytes.begin(), bytes.end());
    return true;
}

bool EditorSceneDocumentProvider::Decode(
    const EditorDocumentContent& content,
    EditorScene* scene,
    std::string* errorMessage) {
    if (scene == nullptr || content.bytes.empty()) {
        if (errorMessage != nullptr) *errorMessage = "Scene content is empty.";
        return false;
    }
    std::istringstream input(std::string(content.bytes.begin(), content.bytes.end()));
    std::string line;
    if (!std::getline(input, line)) return false;
    std::istringstream header(line);
    std::string marker;
    uint32_t schema = 0;
    if (!(header >> marker >> schema) || marker != "SCENE" || schema == 0 ||
        schema > kEditorSceneSchemaVersion) {
        if (errorMessage != nullptr) *errorMessage = "Scene schema header is unsupported.";
        return false;
    }
    EditorScene decoded{};
    decoded.schemaVersion = kEditorSceneSchemaVersion;
    bool ended = false;
    std::size_t lineNumber = 1;
    while (std::getline(input, line)) {
        ++lineNumber;
        if (line.empty()) continue;
        std::istringstream row(line);
        std::string kind;
        row >> kind;
        if (kind == "END") {
            ended = true;
            break;
        }
        if (kind == "ENTITY") {
            EditorSceneEntity entity{};
            int visible = 1;
            int locked = 0;
            int runtimeEnabled = 1;
            if (!(row >> std::quoted(entity.guid) >> std::quoted(entity.parentGuid) >>
                  std::quoted(entity.name) >> visible >> locked)) {
                if (errorMessage != nullptr) *errorMessage = "Invalid ENTITY at line " + std::to_string(lineNumber);
                return false;
            }
            if (schema >= 3 && !(row >> runtimeEnabled)) {
                if (errorMessage != nullptr) {
                    *errorMessage =
                        "Invalid Runtime Enabled state at line " +
                        std::to_string(lineNumber);
                }
                return false;
            }
            entity.visible = visible != 0;
            entity.locked = locked != 0;
            entity.runtimeEnabled = runtimeEnabled != 0;
            decoded.entities.push_back(std::move(entity));
            continue;
        }
        if (kind == "PREFAB_INSTANCE") {
            EditorScenePrefabInstance instance{};
            uint32_t status = 0;
            if (!(row >> std::quoted(instance.instanceGuid) >>
                  std::quoted(instance.prefabAssetGuid) >>
                  std::quoted(instance.rootEntityGuid) >>
                  instance.sourceSchemaVersion >> status) ||
                status > static_cast<uint32_t>(EditorScenePrefabInstanceStatus::Disconnected)) {
                if (errorMessage != nullptr) *errorMessage =
                    "Invalid PREFAB_INSTANCE at line " + std::to_string(lineNumber);
                return false;
            }
            instance.status = static_cast<EditorScenePrefabInstanceStatus>(status);
            decoded.prefabInstances.push_back(std::move(instance));
            continue;
        }
        if (kind == "PREFAB_BINDING") {
            std::string instanceGuid;
            EditorScenePrefabEntityBinding binding{};
            if (!(row >> std::quoted(instanceGuid) >>
                  std::quoted(binding.sourceEntityGuid) >>
                  std::quoted(binding.instanceEntityGuid))) {
                if (errorMessage != nullptr) *errorMessage =
                    "Invalid PREFAB_BINDING at line " + std::to_string(lineNumber);
                return false;
            }
            const auto instance = std::find_if(
                decoded.prefabInstances.begin(), decoded.prefabInstances.end(),
                [&](const auto& value) { return value.instanceGuid == instanceGuid; });
            if (instance == decoded.prefabInstances.end()) {
                if (errorMessage != nullptr) *errorMessage =
                    "PREFAB_BINDING precedes its instance at line " + std::to_string(lineNumber);
                return false;
            }
            instance->bindings.push_back(std::move(binding));
            continue;
        }
        if (kind == "PREFAB_OVERRIDE") {
            std::string instanceGuid;
            EditorScenePrefabOverride value{};
            uint32_t overrideKind = 0;
            if (!(row >> std::quoted(instanceGuid) >> std::quoted(value.id) >> overrideKind >>
                  std::quoted(value.sourceEntityGuid) >>
                  std::quoted(value.instanceEntityGuid) >>
                  std::quoted(value.componentTypeId) >>
                  std::quoted(value.propertyName) >>
                  std::quoted(value.inheritedValue) >>
                  std::quoted(value.instanceValue)) ||
                overrideKind > static_cast<uint32_t>(EditorScenePrefabOverrideKind::RemovedComponent)) {
                if (errorMessage != nullptr) *errorMessage =
                    "Invalid PREFAB_OVERRIDE at line " + std::to_string(lineNumber);
                return false;
            }
            value.kind = static_cast<EditorScenePrefabOverrideKind>(overrideKind);
            const auto instance = std::find_if(
                decoded.prefabInstances.begin(), decoded.prefabInstances.end(),
                [&](const auto& item) { return item.instanceGuid == instanceGuid; });
            if (instance == decoded.prefabInstances.end()) {
                if (errorMessage != nullptr) *errorMessage =
                    "PREFAB_OVERRIDE precedes its instance at line " + std::to_string(lineNumber);
                return false;
            }
            instance->overrides.push_back(std::move(value));
            continue;
        }
        std::string entityGuid;
        std::string typeId;
        if (!(row >> std::quoted(entityGuid) >> std::quoted(typeId))) {
            if (errorMessage != nullptr) *errorMessage = "Invalid Scene record at line " + std::to_string(lineNumber);
            return false;
        }
        EditorSceneEntity* entity = decoded.FindEntity(entityGuid);
        if (entity == nullptr) {
            if (errorMessage != nullptr) *errorMessage = "Scene record precedes its ENTITY at line " + std::to_string(lineNumber);
            return false;
        }
        if (kind == "COMPONENT") {
            int enabled = 1;
            if (!(row >> enabled)) {
                if (errorMessage != nullptr) *errorMessage = "Invalid COMPONENT at line " + std::to_string(lineNumber);
                return false;
            }
            entity->components.push_back({typeId, enabled != 0, {}, {}});
        } else if (kind == "PROPERTY") {
            std::string name;
            std::string value;
            EditorSceneComponent* component = decoded.FindComponent(*entity, typeId);
            if (component == nullptr || !(row >> std::quoted(name) >> std::quoted(value))) {
                if (errorMessage != nullptr) *errorMessage = "Invalid PROPERTY at line " + std::to_string(lineNumber);
                return false;
            }
            component->properties.push_back({std::move(name), std::move(value)});
        } else if (kind == "REFERENCE") {
            EditorSceneObjectReference reference{};
            EditorSceneComponent* component = decoded.FindComponent(*entity, typeId);
            if (component == nullptr || !(row >> std::quoted(reference.property) >>
                  std::quoted(reference.entityGuid) >> std::quoted(reference.assetGuid))) {
                if (errorMessage != nullptr) *errorMessage = "Invalid REFERENCE at line " + std::to_string(lineNumber);
                return false;
            }
            component->references.push_back(std::move(reference));
        } else {
            if (errorMessage != nullptr) *errorMessage = "Unknown Scene record at line " + std::to_string(lineNumber);
            return false;
        }
    }
    if (!ended) {
        if (errorMessage != nullptr) *errorMessage = "Scene document is missing END.";
        return false;
    }
    // In-memory schema migration: legacy Patrol Scenes stored the route as a
    // scalar routeEntityGuid property. Convert it to the typed "route"
    // EditorSceneObjectReference before validation and before the Details
    // Entity Picker sees the document.
    for (EditorSceneEntity& entity : decoded.entities) {
        EditorSceneComponent* patrol =
            decoded.FindComponent(
                entity, kEditorPatrolComponentType);
        if (patrol == nullptr) continue;
        EditorPatrolComponent typedPatrol{};
        std::string migrationError;
        if (!EditorPatrolComponent::FromSceneComponent(
                *patrol, typedPatrol, &migrationError) ||
            !typedPatrol.WriteToSceneComponent(
                *patrol, &migrationError)) {
            if (errorMessage != nullptr) {
                *errorMessage =
                    "Could not migrate Patrol Entity Reference: " +
                    migrationError;
            }
            return false;
        }
    }
    const EditorSceneValidationReport validation = decoded.Validate();
    if (!validation.Succeeded()) {
        if (errorMessage != nullptr) *errorMessage = validation.errors.front();
        return false;
    }
    *scene = std::move(decoded);
    return true;
}

} // namespace editor
