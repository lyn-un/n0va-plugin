
#include <windows.h>
#include <cstdio>
#include <vector>

#include "fs_helper.h"
#include "plugin_settings.h"
#include "text_conv.h"
#include "wallpaper_db.h"

static std::string JsonEscape(const std::string& s) {
    std::string r;
    for (char c : s) {
        switch (c) {
            case '"':  r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\n': r += "\\n"; break;
            case '\r': r += "\\r"; break;
            case '\t': r += "\\t"; break;
            default: r += c;
        }
    }
    return r;
}

int CmdList(int argc, wchar_t** argv) {
    bool asJson = false;
    for (int i = 2; i < argc; i++) {
        if (wcscmp(argv[i], L"--json") == 0) asJson = true;
    }

    std::wstring hostDir = n0va::LoadHostDir();
    if (hostDir.empty()) {
        if (asJson) printf("{\"error\":\"host_not_found\"}\n");
        else {
            printf("[!] 找不到 N0vaDesktop 安装目录。\n");
            printf("    请运行: n0va_plugin set-host \"D:\\Program Files\\N0vaDesktop\"\n");
        }
        return 1;
    }
    std::wstring dbPath = n0va::GetPluginDir() + L"\\wallpapers.json";
    n0va::WallpaperDb db;
    db.Load(dbPath);

    std::wstring cacheDir = n0va::GetCacheDir(hostDir + L"\\config.ini");
    std::vector<std::string> toRemove;
    for (auto& r : db.all()) {
        std::wstring targetFile = n0va::Utf8ToWide(r.target_file);
        std::wstring targetPath = cacheDir + L"\\game\\" + targetFile;
        bool exists = GetFileAttributesW(targetPath.c_str()) != INVALID_FILE_ATTRIBUTES;
        if (!exists) toRemove.push_back(r.vid);
    }
    if (!toRemove.empty()) {
        for (auto& vid : toRemove) db.remove(vid);
        db.Save();
    }

    auto& recs = db.all();
    if (asJson) {
        printf("[");
        bool first = true;
        for (auto& r : recs) {
            if (r.state != n0va::WallpaperState::Active) continue;
            if (!first) printf(",");
            first = false;
            printf("{\"vid\":\"%s\",\"name\":\"%s\",\"format\":\"%s\"}",
                   JsonEscape(r.vid).c_str(), JsonEscape(r.name).c_str(),
                   JsonEscape(r.format).c_str());
        }
        printf("]\n");
        return 0;
    }

    if (recs.empty()) { printf("(空) 还没有导入过壁纸\n"); return 0; }
    printf("%-26s  %-16s  %s\n", "vid", "name", "target_file");
    printf("%s\n", std::string(90, '-').c_str());
    bool any = false;
    for (auto& r : recs) {
        if (r.state != n0va::WallpaperState::Active) continue;
        printf("%-26s  %-16s  %s\n",
               r.vid.c_str(), r.name.c_str(), r.target_file.c_str());
        any = true;
    }
    if (!any) printf("(空) 还没有导入过壁纸\n");
    return 0;
}
