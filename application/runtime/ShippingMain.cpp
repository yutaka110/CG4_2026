#include <Windows.h>
#include <shellapi.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string_view>

#include "runtime/RuntimeHost.h"

#if !defined(GE3_TARGET_SHIPPING) || GE3_TARGET_SHIPPING != 1
#error ShippingMain.cpp must only be compiled by the Shipping target.
#endif
#if !defined(GE3_BUILD_EDITOR) || GE3_BUILD_EDITOR != 0
#error The Shipping target must compile with GE3_BUILD_EDITOR=0.
#endif
#if !defined(GE3_ENABLE_IMGUI) || GE3_ENABLE_IMGUI != 0
#error The Shipping target must compile with GE3_ENABLE_IMGUI=0.
#endif

namespace {
constexpr uint32_t kVerificationFrameCount = 3;
constexpr wchar_t kVerificationArgument[] = L"--shipping-verify";

bool HasArgument(std::wstring_view target) {
    int argumentCount = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    if (!arguments) {
        return false;
    }
    bool found = false;
    for (int index = 1; index < argumentCount; ++index) {
        if (target == arguments[index]) {
            found = true;
            break;
        }
    }
    LocalFree(arguments);
    return found;
}

bool HasEditorOnlyArgument() {
    int argumentCount = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    if (!arguments) {
        return false;
    }
    bool found = false;
    constexpr std::wstring_view prefix = L"--editor-";
    for (int index = 1; index < argumentCount; ++index) {
        if (std::wstring_view(arguments[index]).starts_with(prefix)) {
            found = true;
            break;
        }
    }
    LocalFree(arguments);
    return found;
}

void WriteVerificationReport(const ge3::runtime::RuntimeResult& result) {
    std::error_code directoryError;
    std::filesystem::create_directories("logs", directoryError);
    std::ofstream report("logs/shipping_verification.json", std::ios::trunc);
    if (!report) {
        return;
    }
    report
        << "{\n"
        << "  \"schema\": \"ge3.shippingVerification.v2\",\n"
        << "  \"target\": \"Shipping\",\n"
        << "  \"runtimeApi\": \"ge3.runtimeHost.v1\",\n"
        << "  \"passed\": " << (result.succeeded ? "true" : "false") << ",\n"
        << "  \"editorCompiled\": false,\n"
        << "  \"imguiCompiled\": false,\n"
        << "  \"sourceAssetsRequired\": false,\n"
        << "  \"framesPresented\": " << result.framesPresented << ",\n"
        << "  \"failureStage\": \""
        << ge3::runtime::RuntimeFailureStageName(result.failureStage) << "\"\n"
        << "}\n";
}
} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    if (HasEditorOnlyArgument()) {
        return 64;
    }

    const bool verificationMode = HasArgument(kVerificationArgument);
    ge3::runtime::RuntimeConfig config;
    config.title = u"GE3 Shipping Runtime";
    config.hidden = verificationMode;
    config.verticalSync = !verificationMode;
    config.maximumFrames = verificationMode ? kVerificationFrameCount : 0;

    const ge3::runtime::RuntimeResult result = ge3::runtime::RuntimeHost{}.Run(config);
    WriteVerificationReport(result);
    return result.exitCode;
}
