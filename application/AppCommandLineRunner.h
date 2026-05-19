#pragma once

struct AppCommandLineResult {
    bool handled = false;
    int exitCode = 0;
};

AppCommandLineResult RunAppCommandLineTools();
