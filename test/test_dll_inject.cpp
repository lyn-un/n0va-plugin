







#include <windows.h>
#include <cstdio>
#include <string>

#include "dll_inject.h"

int wmain(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);
    if (argc < 2) { printf("用法: test_dll_inject <exe副本路径>（不要用真实 N0vaDesktop.exe!）\n"); return 1; }
    std::wstring exe = argv[1];

    
    {
        size_t slash = exe.find_last_of(L'\\');
        std::wstring fn = (slash == std::wstring::npos) ? exe : exe.substr(slash + 1);
        if (_wcsicmp(fn.c_str(), L"N0vaDesktop.exe") == 0) {
            printf("[!] 拒绝: 不要拿真实 N0vaDesktop.exe 测试（会污染备份）。请先拷贝副本。\n");
            return 2;
        }
    }

    printf("=== DLL 注入器离线测试 ===\n");
    printf("目标: %ls\n\n", exe.c_str());

    
    std::string origHash = n0va::FileSha256Hex(exe);
    printf("[1] 原始 SHA256: %.16s...\n", origHash.c_str());

    
    bool alreadyPatched = n0va::IsPatched(exe, L"n0va_plugin.dll");
    printf("[2] 补丁前 IsPatched: %s\n", alreadyPatched ? "是" : "否");
    if (alreadyPatched) {
        printf("[!] 目标已注入过, 测试中止（未改动任何文件）。请用原始副本测试。\n");
        return 2;
    }

    
    std::wstring bak = exe + L".n0vatestbak";
    CopyFileW(exe.c_str(), bak.c_str(), FALSE);

    n0va::InstallState st;
    bool ok = n0va::ApplyDllInject(exe, L"n0va_plugin.dll", L"N0vaPluginBootstrap", st);
    printf("[3] ApplyDllInject: %s\n", ok ? "成功" : "失败");
    if (!ok) { DeleteFileW(bak.c_str()); return 1; }
    printf("    original: %.16s...\n", st.original_sha256.c_str());
    printf("    patched:  %.16s...\n", st.patched_sha256.c_str());

    
    std::string err = n0va::ValidateInjectedPe(exe, L"n0va_plugin.dll");
    printf("[4] ValidateInjectedPe: %s\n", err.empty() ? "通过" : err.c_str());

    
    printf("[5] 补丁后 IsPatched: %s\n", n0va::IsPatched(exe, L"n0va_plugin.dll") ? "是" : "否");

    
    std::wstring statePath = exe + L".n0vatest_install.json";
    n0va::SaveInstallState(statePath, st);
    n0va::InstallState st2;
    bool loaded = n0va::LoadInstallState(statePath, st2);
    printf("[6] 状态文件往返: %s (patched hash %s)\n",
           loaded ? "成功" : "失败",
           loaded && st2.patched_sha256 == st.patched_sha256 ? "一致" : "不一致");

    
    std::string reason;
    bool restored = n0va::RestoreOriginal(exe, bak, reason);
    printf("[7] RestoreOriginal: %s (%s)\n", restored ? "成功" : "失败", reason.c_str());
    std::string finalHash = n0va::FileSha256Hex(exe);
    printf("[8] 还原后 hash == 原始: %s\n", finalHash == origHash ? "是 ✓" : "否 ✗");

    
    restored = n0va::RestoreOriginal(exe, bak, reason);
    printf("[9] 重复还原: %s (%s)\n", restored ? "成功" : "失败", reason.c_str());

    
    DeleteFileW(bak.c_str());
    DeleteFileW(statePath.c_str());

    printf("\n=== 测试%s ===\n",
           ok && err.empty() && finalHash == origHash ? "全部通过 ✓" : "存在失败 ✗");
    return (ok && err.empty() && finalHash == origHash) ? 0 : 1;
}
