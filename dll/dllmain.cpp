


#include "dllmain.h"

#include <windows.h>
#include <tlhelp32.h>

#include "detours.h"


DWORD WINAPI BootstrapThread(LPVOID param);







static bool IsMainN0vaProcess() {
    DWORD myPid = GetCurrentProcessId();
    DWORD parentPid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe{ sizeof(pe) };
        if (Process32FirstW(snap, &pe)) {
            do {
                if (pe.th32ProcessID == myPid) {
                    parentPid = pe.th32ParentProcessID;
                    break;
                }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
    }
    if (parentPid == 0) return true;   
    
    HANDLE hp = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, parentPid);
    if (!hp) return true;              
    wchar_t name[MAX_PATH] = {0};
    DWORD len = MAX_PATH;
    bool parentIsHost = false;
    if (QueryFullProcessImageNameW(hp, 0, name, &len)) {
        const wchar_t* base = wcsrchr(name, L'\\');
        base = base ? base + 1 : name;
        parentIsHost = (_wcsicmp(base, L"N0vaDesktop.exe") == 0);
    }
    CloseHandle(hp);
    return !parentIsHost;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        
        DetourRestoreAfterWith();
        
        if (DetourIsHelperProcess()) return TRUE;

        
        if (!IsMainN0vaProcess()) return TRUE;

        DisableThreadLibraryCalls(hModule);
        HANDLE h = CreateThread(nullptr, 0, BootstrapThread, hModule, 0, nullptr);
        if (h) CloseHandle(h);
    }
    return TRUE;
}



extern "C" __declspec(dllexport) void N0vaPluginBootstrap(void) {}

