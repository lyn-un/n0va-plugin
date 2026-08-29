#include "plugin_settings.h"

#include <windows.h>
#include <cstdio>
#include <fstream>
#include <sstream>

#include "fs_helper.h"
#include "text_conv.h"

namespace n0va {

namespace {

std::wstring IniPath() {
    return GetPluginDir() + L"\\n0va_plugin.ini";
}

}

std::wstring GetPluginDir() {
    wchar_t self[MAX_PATH];
    if (GetModuleFileNameW(nullptr, self, MAX_PATH) == 0) return {};
    std::wstring p(self);
    size_t sl = p.find_last_of(L'\\');
    if (sl == std::wstring::npos) return {};
    return p.substr(0, sl);
}

bool IsHostDirValid(const std::wstring& dir) {
    if (dir.empty()) return false;
    return GetFileAttributesW((dir + L"\\N0vaDesktop.exe").c_str())
           != INVALID_FILE_ATTRIBUTES;
}

bool SetHostDir(const std::wstring& hostDir) {
    if (!IsHostDirValid(hostDir)) return false;
    std::wstring path = IniPath();
    FILE* f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"w, ccs=UTF-8") != 0 || !f) return false;
    fwprintf(f, L"host_dir=%s\n", hostDir.c_str());
    fclose(f);
    return true;
}

std::wstring LoadHostDir() {
    std::wstring path = IniPath();
    {
        std::ifstream fin(path, std::ios::binary);
        if (fin) {
            std::string s((std::istreambuf_iterator<char>(fin)),
                          std::istreambuf_iterator<char>());
            size_t p = s.find("host_dir=");
            if (p != std::string::npos) {
                p += 9;
                size_t e = s.find_first_of("\r\n", p);
                std::string val = s.substr(p, e == std::string::npos
                                               ? std::string::npos : e - p);
                size_t b = val.find_first_not_of(" \t");
                size_t en = val.find_last_not_of(" \t");
                if (b != std::string::npos) val = val.substr(b, en - b + 1);
                if (!val.empty()) {
                    std::wstring dir = Utf8ToWide(val);
                    if (IsHostDirValid(dir)) return dir;
                }
            }
        }
    }

    std::wstring dir = FindN0vaInstallDir();
    if (!dir.empty()) SetHostDir(dir);
    return dir;
}

}
