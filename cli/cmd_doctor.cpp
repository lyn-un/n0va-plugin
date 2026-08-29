
#include <windows.h>
#include <cstdio>

#include "fs_helper.h"
#include "dll_inject.h"
#include "plugin_settings.h"
#include "text_conv.h"
#include "wallpaper_db.h"

int CmdDoctor(int argc, wchar_t** argv) {
    bool fixSafe = false, fixForce = false;
    for (int i = 2; i < argc; i++) {
        if (wcscmp(argv[i], L"--fix-safe") == 0) fixSafe = true;
        if (wcscmp(argv[i], L"--fix-force") == 0) fixForce = true;
    }

    std::wstring hostDir = n0va::LoadHostDir();
    if (hostDir.empty()) {
        printf("[!] 找不到 N0vaDesktop 安装目录。\n");
        printf("    请运行: n0va_plugin set-host \"D:\\Program Files\\N0vaDesktop\"\n");
        return 1;
    }
    std::wstring cacheDir = n0va::GetCacheDir(hostDir + L"\\config.ini");
    std::wstring dbPath = n0va::GetPluginDir() + L"\\wallpapers.json";
    n0va::WallpaperDb db;
    db.Load(dbPath);

    printf("=== 一致性检查 ===\n");
    int issues = 0;

    for (auto& r : db.all()) {
        std::wstring targetFile = n0va::Utf8ToWide(r.target_file);
        std::wstring targetPath = cacheDir + L"\\game\\" + targetFile;
        bool fileExists = GetFileAttributesW(targetPath.c_str()) != INVALID_FILE_ATTRIBUTES;

        
        if (r.state == n0va::WallpaperState::Pending) {
            printf("[WARN] 壁纸「%s」导入未完成, 可重试导入\n", r.name.c_str());
            issues++;
            if (fixSafe) {
                r.state = n0va::WallpaperState::Removed;
                printf("       --fix-safe: 已清理该记录\n");
            }
        }
        
        if (r.state == n0va::WallpaperState::Active && !fileExists) {
            printf("[WARN] 壁纸「%s」的文件已丢失: %ls（重启程序可自动恢复）\n",
                   r.name.c_str(), targetPath.c_str());
            issues++;
        }
    }

    
    {
        std::wstring gameDir = cacheDir + L"\\game\\*.ndf";
        WIN32_FIND_DATAW fd{};
        HANDLE hf = FindFirstFileW(gameDir.c_str(), &fd);
        if (hf != INVALID_HANDLE_VALUE) {
            do {
                std::string fn = n0va::WideToUtf8(fd.cFileName);
                bool known = false;
                for (auto& r : db.all()) if (r.target_file == fn) { known = true; break; }
                if (!known) {
                    bool looksPlugin = true;
                    for (char c : fn) {
                        if (!((c >= '0' && c <= '9') || c == '_' || c == '.'))
                            { looksPlugin = false; break; }
                    }
                    if (looksPlugin) {
                        printf("[WARN] 发现残留文件: %s\n", fn.c_str());
                        issues++;
                    }
                }
            } while (FindNextFileW(hf, &fd));
            FindClose(hf);
        }
    }

    if (fixSafe) db.Save();
    if (fixForce) {
        printf("[*] --fix-force 深度修复需人工确认, 暂未实现自动执行\n");
        printf("    请根据上述报告手动处理\n");
    }

    if (issues == 0) printf("[OK] 未发现问题\n");
    else printf("共 %d 项问题\n", issues);
    return 0;
}
