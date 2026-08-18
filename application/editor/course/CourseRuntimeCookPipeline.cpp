#include "CourseRuntimeCookPipeline.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <unordered_map>

namespace editor {
namespace {

CourseRuntimeProgramDiagnosticSeverity ConvertSeverity(
    CourseWaveRuntimeDiagnosticSeverity severity) {
    switch (severity) {
    case CourseWaveRuntimeDiagnosticSeverity::Info:
        return CourseRuntimeProgramDiagnosticSeverity::Info;
    case CourseWaveRuntimeDiagnosticSeverity::Warning:
        return CourseRuntimeProgramDiagnosticSeverity::Warning;
    case CourseWaveRuntimeDiagnosticSeverity::Error:
        return CourseRuntimeProgramDiagnosticSeverity::Error;
    }
    return CourseRuntimeProgramDiagnosticSeverity::Error;
}

bool IsReleaseFatalDiagnostic(std::string_view code) {
    return code == "actor.asset_fallback" ||
        code == "actor.pattern_fallback" ||
        code == "actor.projectile_fallback" ||
        code == "actor.wave_unassigned" ||
        code == "actor.wave_disabled";
}

void AddDiagnostic(
    CourseRuntimeCookResult& result,
    CourseRuntimeProgramDiagnosticSeverity severity,
    std::string code,
    std::string objectGuid,
    std::string message) {
    if (severity == CourseRuntimeProgramDiagnosticSeverity::Error) ++result.errors;
    if (severity == CourseRuntimeProgramDiagnosticSeverity::Warning) ++result.warnings;
    result.diagnostics.push_back({
        severity, std::move(code), std::move(objectGuid), std::move(message)});
}

uint64_t HashFileIfPresent(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return 0;
    const std::string bytes{
        std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    return (file.good() || file.eof()) ? ComputeCourseRuntimeFileHash(bytes) : 0;
}

} // namespace

CourseRuntimeCookResult CourseRuntimeCookPipeline::Cook(
    const CourseAsset& source,
    const CourseRuntimeCookOptions& options) const {
    CourseRuntimeCookResult result{};
    result.outputPath = options.outputPath;

    CourseWaveRuntimeCompileOptions compilerOptions = options.compiler;
    compilerOptions.allowFallbackActorAssets =
        options.configuration == CourseRuntimeBuildConfiguration::Debug &&
        options.allowDebugFallbackAssets;
    const CourseWaveRuntimeCompileResult compiled =
        CourseWaveRuntimeCompiler{}.Compile(source, compilerOptions);
    result.program = compiled.program;
    result.program.buildConfiguration = options.configuration;
    result.program.sourceAssetHash = ComputeCourseAssetSourceHash(source);
    AppendCompilerDiagnostics(result, compiled, options);
    if (result.program.sourceAssetHash == 0) {
        AddDiagnostic(result, CourseRuntimeProgramDiagnosticSeverity::Error,
            "course.source_hash_failed", {},
            "CourseAsset could not be canonicalized for stale-cook detection.");
    }
    if (!compiled.succeeded) {
        result.message = "Course runtime cook stopped after compiler validation.";
        result.program.diagnostics = result.diagnostics;
        return result;
    }

    BuildDependencyTable(result.program, options);
    ValidateTransitionGraph(result, options);
    if (options.configuration == CourseRuntimeBuildConfiguration::Release &&
        options.failReleaseOnWarnings && result.warnings != 0) {
        AddDiagnostic(result, CourseRuntimeProgramDiagnosticSeverity::Error,
            "release.warning_policy", {},
            "Release cook treats all remaining warnings as errors.");
    }
    result.program.diagnostics = result.diagnostics;
    std::string validationError;
    if (!result.program.Validate(&validationError)) {
        AddDiagnostic(result, CourseRuntimeProgramDiagnosticSeverity::Error,
            "program.invalid", {}, validationError);
        result.program.diagnostics = result.diagnostics;
    }
    if (result.errors != 0) {
        result.message = "Course runtime cook failed with " +
            std::to_string(result.errors) + " error(s).";
        return result;
    }

    if (!options.outputPath.empty()) {
        const std::filesystem::path output(options.outputPath);
        std::error_code filesystemError;
        if (!output.parent_path().empty()) {
            std::filesystem::create_directories(output.parent_path(), filesystemError);
        }
        if (filesystemError ||
            !result.program.SaveToFile(options.outputPath, &validationError)) {
            AddDiagnostic(result, CourseRuntimeProgramDiagnosticSeverity::Error,
                "program.write_failed", {}, filesystemError
                    ? "Could not create runtime output directory: " + filesystemError.message()
                    : validationError);
            result.message = "Course runtime cook could not write its output.";
            result.program.diagnostics = result.diagnostics;
            return result;
        }
        result.wroteOutput = true;
        if (options.verifySerializedOutput) {
            CourseRuntimeProgramAsset verified{};
            if (!verified.LoadFromFile(options.outputPath, &validationError) ||
                verified.sourceAssetHash != result.program.sourceAssetHash ||
                verified.sourceFingerprint != result.program.sourceFingerprint ||
                verified.waves.size() != result.program.waves.size() ||
                verified.actors.size() != result.program.actors.size()) {
                AddDiagnostic(result, CourseRuntimeProgramDiagnosticSeverity::Error,
                    "program.verify_failed", {}, validationError.empty()
                        ? "Serialized runtime program did not round-trip exactly."
                        : validationError);
                result.message = "Course runtime cook output verification failed.";
                result.program.diagnostics = result.diagnostics;
                return result;
            }
        }
    }

    result.succeeded = true;
    result.message = "Cooked " + std::to_string(result.program.waves.size()) +
        " Waves and " + std::to_string(result.program.actors.size()) +
        " Actors for " + ToString(options.configuration) + ".";
    return result;
}

std::string CourseRuntimeCookPipeline::DefaultOutputPath(
    std::string_view courseSourcePath) {
    std::filesystem::path path{std::string(courseSourcePath)};
    path += ".runtime";
    return path.generic_string();
}

void CourseRuntimeCookPipeline::AppendCompilerDiagnostics(
    CourseRuntimeCookResult& result,
    const CourseWaveRuntimeCompileResult& compiled,
    const CourseRuntimeCookOptions& options) {
    for (const CourseWaveRuntimeDiagnostic& diagnostic : compiled.diagnostics) {
        CourseRuntimeProgramDiagnosticSeverity severity = ConvertSeverity(diagnostic.severity);
        if (options.configuration == CourseRuntimeBuildConfiguration::Release &&
            IsReleaseFatalDiagnostic(diagnostic.code)) {
            severity = CourseRuntimeProgramDiagnosticSeverity::Error;
        }
        AddDiagnostic(result, severity, diagnostic.code,
            diagnostic.objectGuid, diagnostic.message);
    }
}

void CourseRuntimeCookPipeline::BuildDependencyTable(
    CourseRuntimeProgramAsset& program,
    const CourseRuntimeCookOptions& options) {
    std::unordered_map<std::string, CourseRuntimeProgramDependency> unique;
    const auto add = [&unique](CourseRuntimeProgramDependency dependency) {
        const std::string key = std::to_string(static_cast<uint32_t>(dependency.kind)) +
            ":" + dependency.id;
        unique.emplace(key, std::move(dependency));
    };
    for (const CourseRuntimeActorRecord& record : program.actors) {
        const std::filesystem::path actorPath =
            std::filesystem::path(options.compiler.actorAssetDirectory) /
            (record.actor.actorAssetId + ".actor");
        add({CourseRuntimeDependencyKind::ActorAsset, record.actor.actorAssetId,
            actorPath.generic_string(), HashFileIfPresent(actorPath)});
        if (!record.actor.bulletPatternId.empty()) {
            const std::filesystem::path patternPath =
                std::filesystem::path(options.compiler.bulletPatternDirectory) /
                (record.actor.bulletPatternId + ".pattern");
            add({CourseRuntimeDependencyKind::BulletPattern,
                record.actor.bulletPatternId, patternPath.generic_string(),
                HashFileIfPresent(patternPath)});
        }
        if (!record.actor.projectileDefinitionId.empty()) {
            const std::filesystem::path projectilePath =
                std::filesystem::path(
                    options.compiler.projectileDefinitionDirectory) /
                (record.actor.projectileDefinitionId + ".projectile");
            add({CourseRuntimeDependencyKind::EnemyProjectile,
                record.actor.projectileDefinitionId,
                projectilePath.generic_string(),
                HashFileIfPresent(projectilePath)});
        }
    }
    program.dependencies.clear();
    program.dependencies.reserve(unique.size());
    for (auto& [key, dependency] : unique) {
        (void)key;
        program.dependencies.push_back(std::move(dependency));
    }
    std::sort(program.dependencies.begin(), program.dependencies.end(),
        [](const auto& a, const auto& b) {
            if (a.kind != b.kind) return a.kind < b.kind;
            return a.id < b.id;
        });
}

void CourseRuntimeCookPipeline::ValidateTransitionGraph(
    CourseRuntimeCookResult& result,
    const CourseRuntimeCookOptions& options) {
    enum class Visit : uint8_t { Unvisited, Visiting, Complete };
    std::vector<Visit> visits(result.program.waves.size(), Visit::Unvisited);
    std::vector<std::size_t> stack;
    std::function<void(std::size_t)> visit = [&](std::size_t index) {
        if (index >= result.program.waves.size() || visits[index] == Visit::Complete) return;
        if (visits[index] == Visit::Visiting) {
            const CourseRuntimeProgramDiagnosticSeverity severity =
                options.configuration == CourseRuntimeBuildConfiguration::Release
                    ? CourseRuntimeProgramDiagnosticSeverity::Error
                    : CourseRuntimeProgramDiagnosticSeverity::Warning;
            AddDiagnostic(result, severity, "wave.transition_cycle",
                result.program.waves[index].waveGuid,
                "Wave next-transition graph contains a cycle.");
            return;
        }
        visits[index] = Visit::Visiting;
        stack.push_back(index);
        const int32_t next = result.program.waves[index].nextWaveIndex;
        if (next >= 0) visit(static_cast<std::size_t>(next));
        stack.pop_back();
        visits[index] = Visit::Complete;
    };
    for (std::size_t index = 0; index < result.program.waves.size(); ++index) visit(index);
}

} // namespace editor
