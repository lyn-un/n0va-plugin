
#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <string>

#include "detours.h"
#include "fs_helper.h"
#include "plugin_settings.h"
#include "text_conv.h"

static bool IsProcessRunning(const wchar_t* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe{ sizeof(pe) };
    bool found = false;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, name) == 0) { found = true; break; }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return found;
}

int CmdLaunch(int argc, wchar_t** argv) {
    std::wstring hostDir = n0va::LoadHostDir();
    std::wstring exePath = (argc >= 3) ? argv[2] : hostDir + L"\\N0vaDesktop.exe";
    if (GetFileAttributesW(exePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        printf("[!] 找不到 exe: %ls\n", exePath.c_str());
        return 1;
    }



    if (IsProcessRunning(L"N0vaDesktop.exe")) {
        printf("[*] N0vaDesktop 已在运行, 无需重复 launch。\n");
        printf("    如需重启, 请先 taskkill /F /IM N0vaDesktop.exe。\n");
        return 0;
    }


    std::wstring dllPath = n0va::GetPluginDir() + L"\\n0va_plugin.dll";
    if (GetFileAttributesW(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        printf("[!] n0va_plugin.dll 不存在（应与 n0va_plugin.exe 同目录）\n");
        return 1;
    }

    printf("[*] 启动人工桌面（带插件）: %ls\n", exePath.c_str());
    printf("[*] 插件: %ls\n", dllPath.c_str());

    STARTUPINFOW si{ sizeof(si) };
    PROCESS_INFORMATION pi{};
    wchar_t cmdline[1024];
    swprintf(cmdline, 1024, L"\"%s\"", exePath.c_str());

    
    
    std::string dllNarrow = n0va::WideToUtf8(dllPath);
    BOOL ok = DetourCreateProcessWithDllExW(
        exePath.c_str(), cmdline, nullptr, nullptr, FALSE,
        CREATE_DEFAULT_ERROR_MODE, nullptr,
        (LPWSTR)hostDir.c_str(),   
        &si, &pi, dllNarrow.c_str(), nullptr);
    if (!ok) {
        printf("[!] 启动失败: %lu\n", GetLastError());
        return 1;
    }
    printf("[+] 已启动\n");
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}
