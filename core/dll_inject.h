#pragma once

#include <cstdint>
#include <string>

namespace n0va {

struct InstallState {
    std::string original_sha256;
    std::string patched_sha256;
    std::string host_version;
    std::string dll_sha256;
    std::string plugin_dir;
    int64_t     install_time = 0;
};


enum class ExeState {
    Original,   
    Patched,    
    Unknown,    
    NotFound,
};


bool IsPatched(const std::wstring& exePath, const std::wstring& dllName);



bool ApplyDllInject(const std::wstring& exePath,
                   const std::wstring& dllName,
                   const std::wstring& exportName,
                   InstallState& outState);



bool RestoreOriginal(const std::wstring& exePath,
                     const std::wstring& backupPath,
                     std::string& reason);


bool LoadInstallState(const std::wstring& statePath, InstallState& st);
bool SaveInstallState(const std::wstring& statePath, const InstallState& st);


std::string FileSha256Hex(const std::wstring& path);



std::string ValidateInjectedPe(const std::wstring& exePath, const std::wstring& dllName);

} 
