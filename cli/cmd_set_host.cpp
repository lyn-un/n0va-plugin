
#include <windows.h>
#include <cstdio>

#include "plugin_settings.h"
#include "text_conv.h"

int CmdSetHost(int argc, wchar_t** argv) {
    if (argc < 3) {
        std::wstring cur = n0va::LoadHostDir();
        if (cur.empty()) {
            printf("not_set\n");
            printf("[*] 尚未设置人工桌面路径（首次运行其他命令时会自动检测）\n");
            printf("用法: n0va_plugin set-host \"<N0vaDesktop 安装目录>\"\n");
        } else {
            printf("%ls\n", cur.c_str());
        }
        return 0;
    }

    std::wstring dir = argv[2];
    while (!dir.empty() && dir.back() == L'\\') dir.pop_back();
    if (!n0va::SetHostDir(dir)) {
        printf("[!] 目录无效: %ls（需包含 N0vaDesktop.exe）\n", dir.c_str());
        return 1;
    }
    printf("%ls\n", dir.c_str());
    return 0;
}
