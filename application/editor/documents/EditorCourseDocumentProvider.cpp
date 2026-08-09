#include "EditorCourseDocumentProvider.h"

#include "../../course/CourseAsset.h"
#include "../world/CourseWorldIdentity.h"
#include "../course/CourseRailAuthoringModel.h"
#include "../course/CourseEnemyAuthoringModel.h"
#include "../course/CourseWaveAuthoringModel.h"

#include <fstream>
#include <iterator>

namespace editor {
namespace {

bool ReadFile(
    const std::filesystem::path& path,
    std::vector<uint8_t>* bytes,
    std::string* errorMessage) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        if (errorMessage != nullptr) *errorMessage = "Could not open course document: " + path.string();
        return false;
    }
    bytes->assign(
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>());
    if (!stream.good() && !stream.eof()) {
        if (errorMessage != nullptr) *errorMessage = "Could not read course document: " + path.string();
        return false;
    }
    return true;
}

std::string ToText(const EditorDocumentContent& content) {
    return std::string(content.bytes.begin(), content.bytes.end());
}

uint32_t ReadSchemaVersion(const std::vector<uint8_t>& bytes) {
    constexpr std::string_view marker = "# editor-schema:";
    const std::string text(bytes.begin(), bytes.end());
    const std::size_t markerOffset = text.find(marker);
    if (markerOffset == std::string::npos) return 1;
    const std::size_t valueOffset = markerOffset + marker.size();
    std::size_t valueEnd = valueOffset;
    while (valueEnd < text.size() && text[valueEnd] >= '0' && text[valueEnd] <= '9') ++valueEnd;
    if (valueEnd == valueOffset) return 1;
    try {
        return static_cast<uint32_t>(std::stoul(text.substr(valueOffset, valueEnd - valueOffset)));
    } catch (...) {
        return 1;
    }
}

} // namespace

bool EditorCourseDocumentProvider::SupportsPath(const std::filesystem::path& path) const {
    return path.extension() == ".course";
}

bool EditorCourseDocumentProvider::ReadSource(
    const std::filesystem::path& path,
    EditorDocumentContent* content,
    std::string* errorMessage) const {
    if (content == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Course document content output is null.";
        return false;
    }
    if (!ReadFile(path, &content->bytes, errorMessage)) return false;
    content->schemaVersion = ReadSchemaVersion(content->bytes);
    return true;
}

bool EditorCourseDocumentProvider::Serialize(
    const EditorDocumentId& id,
    EditorDocumentContent* content,
    std::string* errorMessage) const {
    if (course_ == nullptr || content == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Live Course authoring model is unavailable.";
        return false;
    }
    // LoadFromString supplies a default camera when a legacy/minimal Course has
    // none. Persist that default here so schema-v7 validation never observes a
    // synthesized camera without a stable editor identity after round-trip.
    if (course_->cameraKeys.empty()) course_->cameraKeys.push_back({});
    EnsureCourseWorldObjectGuids(*course_, id.assetGuid);
    CourseRailAuthoringModel::EnsureStableIdentity(*course_, id.assetGuid);
    CourseEnemyAuthoringModel::EnsureStableIdentity(*course_, id.assetGuid);
    CourseWaveAuthoringModel::UpgradeLegacyWaveGroups(*course_, id.assetGuid);
    std::string text;
    if (!course_->SaveToString(&text, errorMessage)) return false;
    content->schemaVersion = CurrentSchemaVersion();
    content->bytes.assign(text.begin(), text.end());
    return true;
}

bool EditorCourseDocumentProvider::Deserialize(
    const EditorDocumentId& id,
    const EditorDocumentContent& content,
    std::string* errorMessage) {
    if (course_ == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Live Course authoring model is unavailable.";
        return false;
    }
    CourseAsset loaded{};
    if (!loaded.LoadFromString(ToText(content), errorMessage)) return false;
    EnsureCourseWorldObjectGuids(loaded, id.assetGuid);
    CourseRailAuthoringModel::EnsureStableIdentity(loaded, id.assetGuid);
    CourseEnemyAuthoringModel::EnsureStableIdentity(loaded, id.assetGuid);
    if (content.schemaVersion < CurrentSchemaVersion()) {
        CourseWaveAuthoringModel::UpgradeLegacyWaveGroups(loaded, id.assetGuid);
    } else {
        CourseWaveAuthoringModel::EnsureStableIdentity(loaded, id.assetGuid);
    }
    std::vector<std::string> identityDiagnostics;
    if (!ValidateCourseWorldObjectGuids(loaded, &identityDiagnostics)) {
        if (errorMessage != nullptr) {
            *errorMessage = identityDiagnostics.empty()
                ? "Course world object identity validation failed."
                : identityDiagnostics.front();
        }
        return false;
    }
    const CourseEnemyAuthoringModel enemies(loaded);
    if (!enemies.IsValid()) {
        if (errorMessage != nullptr) *errorMessage = enemies.ValidationError();
        return false;
    }
    const CourseWaveAuthoringModel waves(loaded);
    if (!waves.IsValid()) {
        if (errorMessage != nullptr) *errorMessage = waves.ValidationError();
        return false;
    }
    *course_ = std::move(loaded);
    return true;
}

EditorDocumentValidationReport EditorCourseDocumentProvider::Validate(
    const EditorDocumentContent& content) const {
    EditorDocumentValidationReport report{};
    if (content.schemaVersion != CurrentSchemaVersion()) {
        report.issues.push_back({
            EditorDocumentIssueSeverity::Error,
            "course.schema",
            "Course document schema version is unsupported."});
        return report;
    }
    CourseAsset parsed{};
    std::string error;
    if (!parsed.LoadFromString(ToText(content), &error)) {
        report.issues.push_back({
            EditorDocumentIssueSeverity::Error,
            "course.parse",
            error.empty() ? "Course document could not be parsed." : error});
        return report;
    }
    std::vector<std::string> identityDiagnostics;
    if (!ValidateCourseWorldObjectGuids(parsed, &identityDiagnostics)) {
        for (const std::string& diagnostic : identityDiagnostics) {
            report.issues.push_back({
                EditorDocumentIssueSeverity::Error,
                "course.world_identity",
                diagnostic});
        }
    }
    const CourseEnemyAuthoringModel enemies(parsed);
    if (!enemies.IsValid()) {
        report.issues.push_back({
            EditorDocumentIssueSeverity::Error,
            "course.enemy_placement",
            enemies.ValidationError()});
    }
    const CourseWaveAuthoringModel waves(parsed);
    if (!waves.IsValid()) {
        report.issues.push_back({
            EditorDocumentIssueSeverity::Error,
            "course.wave_definition",
            waves.ValidationError()});
    }
    return report;
}

bool EditorCourseDocumentProvider::Migrate(
    const EditorDocumentContent& source,
    EditorDocumentContent* migrated,
    EditorDocumentMigrationReport* report,
    std::string* errorMessage) const {
    if (migrated == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Course migration output is null.";
        return false;
    }
    if (source.schemaVersion == CurrentSchemaVersion()) {
        *migrated = source;
        if (report != nullptr) {
            report->sourceSchemaVersion = CurrentSchemaVersion();
            report->targetSchemaVersion = CurrentSchemaVersion();
        }
        return true;
    }
    if (source.schemaVersion != 1 && source.schemaVersion != 2 &&
        source.schemaVersion != 3 && source.schemaVersion != 4 &&
        source.schemaVersion != 5 && source.schemaVersion != 6) {
        if (errorMessage != nullptr) *errorMessage = "No Course schema migration path is registered.";
        return false;
    }
    CourseAsset parsed{};
    if (!parsed.LoadFromString(ToText(source), errorMessage)) return false;
    const std::size_t assigned = EnsureCourseWorldObjectGuids(
        parsed,
        std::string("course-migration:") + parsed.name);
    const std::size_t railAssigned = CourseRailAuthoringModel::EnsureStableIdentity(
        parsed,
        std::string("course-migration:") + parsed.name);
    const std::size_t enemyAssigned = CourseEnemyAuthoringModel::EnsureStableIdentity(
        parsed,
        std::string("course-migration:") + parsed.name);
    const CourseWaveLegacyUpgradeResult waveUpgrade =
        CourseWaveAuthoringModel::UpgradeLegacyWaveGroups(
            parsed,
            std::string("course-migration:") + parsed.name);
    const CourseWaveAuthoringModel waves(parsed);
    if (!waves.IsValid()) {
        if (errorMessage != nullptr) *errorMessage = waves.ValidationError();
        return false;
    }
    std::string text;
    if (!parsed.SaveToString(&text, errorMessage)) return false;
    migrated->schemaVersion = CurrentSchemaVersion();
    migrated->bytes.assign(text.begin(), text.end());
    if (report != nullptr) {
        report->migrated = true;
        report->sourceSchemaVersion = source.schemaVersion;
        report->targetSchemaVersion = CurrentSchemaVersion();
        report->notes.push_back(
            "Assigned persistent editor GUIDs to " + std::to_string(assigned) +
            " Course world objects and " + std::to_string(railAssigned) +
            " rail control points, and " + std::to_string(enemyAssigned) +
            " enemy placements; created " +
            std::to_string(waveUpgrade.createdWaveDefinitions) +
            " schema-v7 wave definitions and remapped " +
            std::to_string(waveUpgrade.remappedEnemyReferences) +
            " enemy wave references.");
    }
    return true;
}

} // namespace editor
