#include "utils/Logger.h"
#include <Windows.h>

#if !defined(GE3_ENGINE_CORE) || GE3_ENGINE_CORE != 1
#error Logger.cpp must only be compiled by the EngineCore module.
#endif

void Logger::Info(const std::string& msg) {
    OutputDebugStringA(("[INFO] " + msg + "\n").c_str());
}

void Logger::Error(const std::string& msg) {
    OutputDebugStringA(("[ERROR] " + msg + "\n").c_str());
}
