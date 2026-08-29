#include "dllmain.h"
#include "logging.h"

#include <cstdio>

bool N0vaPluginInitHost();
bool N0vaPluginStartPipeServer();
void N0vaPluginInstallHook();
void N0vaPluginStartWorker();

namespace {


}

DWORD WINAPI BootstrapThread(LPVOID)
{
    PluginLogInit();
    Log("[n0va] bootstrap start...");

    if (!N0vaPluginInitHost()) {
        Log("[n0va] host version check failed, clean exit (no injection)");
        return 0;
    }

    N0vaPluginInstallHook();
    N0vaPluginStartWorker();

    Log("[n0va] starting pipe server...");
    N0vaPluginStartPipeServer();

    return 0;
}
