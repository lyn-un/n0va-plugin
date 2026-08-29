#include "logging.h"

#include <windows.h>
#include <cstdio>
#include <string>

#include "dll_inject.h"
#include "text_conv.h"

static std::wstring g_logPath;

void PluginLogInit() {
    std::wstring dir;

    {
        wchar_t self[MAX_PATH];
        if (GetModuleFileNameW((HMODULE)&PluginLogInit, self, MAX_PATH) > 0) {
            std::wstring p(self);
            size_t sl = p.find_last_of(L'\\');
            if (sl != std::wstring::npos) dir = p.substr(0, sl);
        }
    }

    {
        wchar_t exe[MAX_PATH];
        if (GetModuleFileNameW(nullptr, exe, MAX_PATH) > 0) {
            std::wstring statePath = std::wstring(exe) + L".n0va_install.json";
            n0va::InstallState st;
            if (n0va::LoadInstallState(statePath, st) && !st.plugin_dir.empty()) {
                dir = n0va::Utf8ToWide(st.plugin_dir);
            }
        }
    }

    if (dir.empty()) dir = L".";
    g_logPath = dir + L"\\n0va_plugin.log";
}

void Log(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    OutputDebugStringA(buf);
    if (g_logPath.empty()) return;
    FILE* f = nullptr;
    if (_wfopen_s(&f, g_logPath.c_str(), L"ab") == 0 && f) {
        fprintf(f, "%s\n", buf);
        fclose(f);
    }
}
