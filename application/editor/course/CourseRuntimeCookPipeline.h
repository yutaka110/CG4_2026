#pragma once

#include <string>
#include <vector>

#include "../../course/CourseRuntimeProgramAsset.h"
#include "CourseWaveRuntimeCompiler.h"

namespace editor {

struct CourseRuntimeCookOptions final {
    CourseRuntimeBuildConfiguration configuration =
        CourseRuntimeBuildConfiguration::Debug;
    CourseWaveRuntimeCompileOptions compiler{};
    std::string outputPath;
    bool allowDebugFallbackAssets = true;
    bool failReleaseOnWarnings = false;
    bool verifySerializedOutput = true;
};

struct CourseRuntimeCookResult final {
    bool succeeded = false;
    bool wroteOutput = false;
    CourseRuntimeProgramAsset program{};
    std::vector<CourseRuntimeProgramDiagnostic> diagnostics;
    uint32_t errors = 0;
    uint32_t warnings = 0;
    std::string outputPath;
    std::string message;
};

// Editor/cook boundary that validates the authoring graph, resolves all asset
// dependencies and emits the exact immutable program consumed by gameplay.
class CourseRuntimeCookPipeline final {
public:
    CourseRuntimeCookResult Cook(
        const CourseAsset& source,
        const CourseRuntimeCookOptions& options = {}) const;

    static std::string DefaultOutputPath(std::string_view courseSourcePath);

private:
    static void AppendCompilerDiagnostics(
        CourseRuntimeCookResult& result,
        const CourseWaveRuntimeCompileResult& compiled,
        const CourseRuntimeCookOptions& options);
    static void BuildDependencyTable(
        CourseRuntimeProgramAsset& program,
        const CourseRuntimeCookOptions& options);
    static void ValidateTransitionGraph(
        CourseRuntimeCookResult& result,
        const CourseRuntimeCookOptions& options);
};

} // namespace editor
