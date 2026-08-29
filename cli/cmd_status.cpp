
#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>

#include "fs_helper.h"
#include "dll_inject.h"
#include "pipe_client.h"
#include "plugin_settings.h"

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

int CmdStatus(int argc, wchar_t** argv) {
    std::wstring hostDir = n0va::LoadHostDir();
    if (hostDir.empty()) {
        printf("[!] 找不到 N0vaDesktop 安装目录。\n");
        printf("    请运行: n0va_plugin set-host \"D:\\Program Files\\N0vaDesktop\"\n");
        return 1;
    }
    std::wstring exePath = hostDir + L"\\N0vaDesktop.exe";
    std::wstring dllPath = hostDir + L"\\n0va_plugin.dll";

    printf("=== 插件状态 ===\n");
    printf("安装目录: %ls\n\n", hostDir.c_str());

    
    bool patched = n0va::IsPatched(exePath, L"n0va_plugin.dll");
    printf("自动加载:  %s\n", patched ? "已开启（启动人工桌面时自动加载插件）" : "未开启");

    
    bool dll = GetFileAttributesW(dllPath.c_str()) != INVALID_FILE_ATTRIBUTES;
    printf("插件文件:  %s\n", dll ? "正常" : "缺失（请先 install）");

    
    bool host = IsProcessRunning(L"N0vaDesktop.exe");
    printf("人工桌面:  %s\n", host ? "运行中" : "未运行");

    
    if (host && dll) {
        std::string req = "{\"protocol\":1,\"id\":1,\"op_id\":\"status\",\"cmd\":\"ping\",\"args\":{}}";
        n0va::PipeResponse resp;
        if (n0va::PipeRequest(n0va::MakePipeName(), req, resp)) {
            printf("插件连接:  正常\n");
        } else {
            printf("插件连接:  失败（%s）\n", resp.error.c_str());
        }
    } else {
        printf("插件连接:  未检测（请先启动人工桌面）\n");
    }
    return 0;
}
