#include <Windows.h>

#include "application/AppCommandLineRunner.h"
#include "application/AppMain.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    const AppCommandLineResult commandLineResult = RunAppCommandLineTools();
    if (commandLineResult.handled) {
        return commandLineResult.exitCode;
    }

    AppMain app;
    if (!app.Initialize(hInstance)) {
        return -1;
    }
    return app.Run();
}
