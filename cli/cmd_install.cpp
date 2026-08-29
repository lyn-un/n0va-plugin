
#include <windows.h>
#include <cstdio>
#include <ctime>
#include <string>

#include "fs_helper.h"
#include "dll_inject.h"
#include "plugin_settings.h"
#include "text_conv.h"
#include "version_profile.h"





static bool DeployDll(const std::wstring& hostDir, std::wstring& outDllPath) {
    wchar_t self[MAX_PATH];
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    std::wstring selfPath(self);
    size_t slash = selfPath.find_last_of(L'\\');
    std::wstring selfDir = selfPath.substr(0, slash + 1);
    std::wstring src = selfDir + L"n0va_plugin.dll";
    if (GetFileAttributesW(src.c_str()) == INVALID_FILE_ATTRIBUTES) {
        printf("[!] 找不到 n0va_plugin.dll（应与 n0va_plugin.exe 同目录）\n");
        return false;
    }
    outDllPath = hostDir + L"\\n0va_plugin.dll";
    
    {
        wchar_t srcFull[MAX_PATH], dstFull[MAX_PATH];
        wchar_t* sp = nullptr, * dp = nullptr;
        GetFullPathNameW(src.c_str(), MAX_PATH, srcFull, &sp);
        GetFullPathNameW(outDllPath.c_str(), MAX_PATH, dstFull, &dp);
        if (_wcsicmp(srcFull, dstFull) == 0) {
            
        } else if (!CopyFileW(src.c_str(), outDllPath.c_str(), FALSE)) {
            DWORD err = GetLastError();
            if (err == ERROR_SHARING_VIOLATION) {
                
                
                printf("[!] 拷贝 DLL 失败: n0va_plugin.dll 正被 N0vaDesktop 占用。\n");
                printf("    请先关闭 N0vaDesktop（taskkill /F /IM N0vaDesktop.exe）再 install。\n");
            } else {
                printf("[!] 拷贝 DLL 失败 (%lu)\n", err);
            }
            return false;
        }
    }
    return true;
}

int CmdInstall(int argc, wchar_t** argv) {
    std::wstring hostDir = n0va::LoadHostDir();
    if (hostDir.empty()) {
        printf("[!] 找不到 N0vaDesktop 安装目录。\n");
        printf("    请运行: n0va_plugin set-host \"D:\\Program Files\\N0vaDesktop\"\n");
        return 1;
    }
    std::wstring exePath = hostDir + L"\\N0vaDesktop.exe";

    
    
    
    
    {
        HANDLE h = CreateFileW(exePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                               OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) return 1;
        DWORD sz = GetFileSize(h, nullptr);
        CloseHandle(h);
        const auto& p = n0va::currentProfile();
        bool ourPatched = n0va::IsPatched(exePath, L"n0va_plugin.dll");
        if ((uint64_t)sz != p.exe_size && !ourPatched) {
            printf("[!] 宿主版本不匹配（size=%lu, 支持=%llu, %s）\n",
                   sz, (unsigned long long)p.exe_size, p.host_version);
            printf("    请在新版本重新验证后再安装\n");
            return 1;
        }
    }

    
    std::wstring dllPath;
    if (!DeployDll(hostDir, dllPath)) return 1;
    printf("[+] DLL 已部署: %ls\n", dllPath.c_str());


    if (n0va::IsPatched(exePath, L"n0va_plugin.dll")) {
        printf("[*] 已打过补丁, 跳过\n");
        std::wstring statePath = exePath + L".n0va_install.json";
        n0va::InstallState st;
        if (n0va::LoadInstallState(statePath, st)) {
            st.plugin_dir = n0va::WideToUtf8(n0va::GetPluginDir());
            st.dll_sha256 = n0va::FileSha256Hex(dllPath);
            n0va::SaveInstallState(statePath, st);
        }
        return 0;
    }

    
    
    std::wstring bak = exePath + L".n0vabak";
    {
        std::string exeHash = n0va::FileSha256Hex(exePath);
        std::string bakHash = n0va::FileSha256Hex(bak);
        if (bakHash == exeHash) {
            
        } else if (bakHash.empty()) {
            if (!CopyFileW(exePath.c_str(), bak.c_str(), TRUE)) {
                printf("[!] 备份失败\n");
                return 1;
            }
            printf("[+] 原 exe 备份: %ls\n", bak.c_str());
        } else {
            if (!CopyFileW(exePath.c_str(), bak.c_str(), FALSE)) {
                printf("[!] 备份刷新失败\n");
                return 1;
            }
            printf("[*] 备份与当前 exe 不一致, 已刷新（宿主可能已更新或被污染）\n");
        }
    }

    n0va::InstallState st;
    st.host_version = n0va::currentProfile().host_version;
    st.dll_sha256 = n0va::FileSha256Hex(dllPath);
    st.plugin_dir = n0va::WideToUtf8(n0va::GetPluginDir());
    st.install_time = (int64_t)time(nullptr);
    if (!n0va::ApplyDllInject(exePath, L"n0va_plugin.dll", L"N0vaPluginBootstrap", st)) {
        printf("[!] 安装失败（未对程序做任何改动, 可重试）\n");
        return 1;
    }

    
    std::wstring statePath = exePath + L".n0va_install.json";
    n0va::SaveInstallState(statePath, st);
    printf("[+] 补丁成功\n");
    printf("    original: %s\n", st.original_sha256.c_str());
    printf("    patched:  %s\n", st.patched_sha256.c_str());
    printf("[*] 若 N0vaDesktop 正在运行, 补丁在下次启动生效\n");
    return 0;
}

int CmdUninstall(int argc, wchar_t** argv) {
    std::wstring hostDir = n0va::LoadHostDir();
    if (hostDir.empty()) {
        printf("[!] 找不到 N0vaDesktop 安装目录。\n");
        printf("    请运行: n0va_plugin set-host \"D:\\Program Files\\N0vaDesktop\"\n");
        return 1;
    }
    std::wstring exePath = hostDir + L"\\N0vaDesktop.exe";
    std::wstring bak = exePath + L".n0vabak";

    if (GetFileAttributesW(bak.c_str()) == INVALID_FILE_ATTRIBUTES) {
        printf("[*] 无备份文件, 可能未打过补丁。仅移除 DLL。\n");
    } else {
        std::string reason;
        if (!n0va::RestoreOriginal(exePath, bak, reason)) {
            if (reason == "host_updated") {
                printf("[!] 宿主已被更新为新版本, 拒绝用旧备份回退。\n");
                printf("    请在新版本重新验证后 install。\n");
            } else {
                printf("[!] 还原失败: %s\n", reason.c_str());
            }
            return 1;
        }
        if (reason == "restored") printf("[+] exe 已还原为原始版本\n");
        else if (reason == "already_original") printf("[*] exe 已是原始版本\n");
    }

    
    std::wstring dllPath = hostDir + L"\\n0va_plugin.dll";
    if (GetFileAttributesW(dllPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        if (DeleteFileW(dllPath.c_str())) printf("[+] DLL 已移除\n");
        else printf("[!] DLL 移除失败（可能被占用, 重启后再试）\n");
    }
    printf("[*] 卸载完成（已导入的壁纸记录保留, 重装后可恢复）\n");
    return 0;
}
