
#include <windows.h>
#include <cstdio>
#include <string>

#include "fs_helper.h"
#include "pipe_client.h"
#include "plugin_settings.h"
#include "text_conv.h"
#include "wallpaper_db.h"

int CmdRemove(int argc, wchar_t** argv) {
    if (argc < 3) { printf("用法: n0va_plugin remove <vid>\n"); return 1; }
    std::string vid = n0va::WideToUtf8(argv[2]);

    std::wstring hostDir = n0va::LoadHostDir();
    if (hostDir.empty()) {
        printf("[!] 找不到 N0vaDesktop 安装目录。\n");
        printf("    请运行: n0va_plugin set-host \"D:\\Program Files\\N0vaDesktop\"\n");
        return 1;
    }
    std::wstring dbPath = n0va::GetPluginDir() + L"\\wallpapers.json";
    n0va::WallpaperDb db;
    db.Load(dbPath);
    auto* rec = db.find(vid);
    if (!rec) { printf("[!] vid 不存在: %s（n0va_plugin list 查看）\n", vid.c_str()); return 1; }

    
    n0va::WallpaperState prevState = rec->state;

    
    rec->state = n0va::WallpaperState::Deleting;
    rec->op_id = n0va::GenerateOpId();
    db.Save();

    
    std::string req = "{\"protocol\":1,\"id\":1,\"op_id\":\"" + rec->op_id + "\","
                      "\"cmd\":\"remove\",\"args\":{\"vid\":\"" + vid + "\"}}";
    n0va::PipeResponse resp;
    if (!n0va::PipeRequest(n0va::MakePipeName(), req, resp)) {
        
        rec->state = prevState;
        db.Save();
        printf("[!] 管道通信失败: %s（N0vaDesktop 未运行; 已回滚状态, 壁纸保留）\n", resp.error.c_str());
        return 1;
    }
    if (!resp.ok) {
        rec->state = prevState;
        db.Save();
        printf("[!] 删除失败: %s（已回滚状态, 壁纸保留）\n", resp.error.c_str());
        return 1;
    }

    
    std::wstring cacheDir = n0va::GetCacheDir(hostDir + L"\\config.ini");
    std::wstring targetFile = n0va::Utf8ToWide(rec->target_file);
    std::wstring targetPath = cacheDir + L"\\game\\" + targetFile;
    if (GetFileAttributesW(targetPath.c_str()) != INVALID_FILE_ATTRIBUTES)
        DeleteFileW(targetPath.c_str());

    
    
    
    
    std::string removedName = rec->name;
    db.remove(vid);
    db.Save();

    printf("[+] 已删除壁纸: %s\n", removedName.c_str());
    return 0;
}
