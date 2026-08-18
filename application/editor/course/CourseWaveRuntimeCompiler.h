#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "../../course/BulletPatternAsset.h"
#include "../../course/CourseActorAsset.h"
#include "../../course/CourseAsset.h"
#include "../../course/CourseRuntimeProgramAsset.h"
#include "../../course/CourseSpawnRuntime.h"
#include "../../course/EnemyProjectileDefinitionAsset.h"

namespace editor {

enum class CourseWaveRuntimeDiagnosticSeverity : uint8_t {
    Info,
    Warning,
    Error,
};

struct CourseWaveRuntimeDiagnostic final {
    CourseWaveRuntimeDiagnosticSeverity severity =
        CourseWaveRuntimeDiagnosticSeverity::Info;
    std::string code;
    std::string objectGuid;
    std::string message;
};

struct CourseWaveRuntimeCompileOptions final {
    std::string actorAssetDirectory = "Resources/courses/actors";
    std::string bulletPatternDirectory = "Resources/courses/bullet_patterns";
    std::string projectileDefinitionDirectory = "Resources/courses/projectiles";
    bool allowFallbackActorAssets = false;
    bool compileDisabledObjects = false;
    std::function<bool(
        std::string_view,
        CourseActorAsset&,
        std::string&)> actorAssetResolver;
    std::function<bool(
        std::string_view,
        BulletPatternAsset&,
        std::string&)> bulletPatternResolver;
    std::function<bool(
        std::string_view,
        EnemyProjectileDefinitionAsset&,
        std::string&)> projectileDefinitionResolver;
};

using CompiledCourseWaveActor = CourseRuntimeActorRecord;
using CompiledCourseWaveNode = CourseRuntimeWaveNode;
using CompiledCourseWaveProgram = CourseRuntimeProgramAsset;

struct CourseWaveRuntimeCompileResult final {
    bool succeeded = false;
    CompiledCourseWaveProgram program{};
    std::vector<CourseWaveRuntimeDiagnostic> diagnostics;
    uint32_t errors = 0;
    uint32_t warnings = 0;
    std::string message;
};

// Converts schema-v7 authoring records into an immutable, index-linked runtime
// program. Asset resolution and graph validation happen once at this boundary.
class CourseWaveRuntimeCompiler final {
public:
    CourseWaveRuntimeCompileResult Compile(
        const CourseAsset& source,
        const CourseWaveRuntimeCompileOptions& options = {}) const;

private:
    static bool ResolveActorAsset(
        std::string_view id,
        const CourseWaveRuntimeCompileOptions& options,
        CourseActorAsset& asset,
        std::string& errorMessage);
    static bool ResolveBulletPattern(
        std::string_view id,
        const CourseWaveRuntimeCompileOptions& options,
        BulletPatternAsset& asset,
        std::string& errorMessage);
    static bool ResolveProjectileDefinition(
        std::string_view id,
        const CourseWaveRuntimeCompileOptions& options,
        EnemyProjectileDefinitionAsset& asset,
        std::string& errorMessage);
};

const char* ToString(CourseWaveRuntimeDiagnosticSeverity severity);

} // namespace editor
