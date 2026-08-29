
#include <windows.h>
#include <bcrypt.h>
#include <cstdio>
#include <ctime>
#include <string>

#pragma comment(lib, "bcrypt.lib")

#include "fs_helper.h"
#include "dll_inject.h"
#include "pipe_client.h"
#include "plugin_settings.h"
#include "text_conv.h"   
#include "wallpaper_db.h"





struct GameTag { const char* id; const char* zh; const char* en; };
static const GameTag kGameTags[] = {
    {"2",  "崩坏学园2",   "Houkai Gakuen2"},
    {"3",  "yoyo鹿鸣",    "Lumi"},
    {"4",  "崩坏3",       "Honkai Impact 3"},
    {"5",  "原神",        "Genshin Impact"},
    {"6",  "米游社",      "miHoYo Official Forum"},
    {"7",  "HOYO-MiX",    "HOYO-MiX"},
    {"8",  "崩坏星穹铁道", "Honkai: Star Rail"},
    {"9",  "绝区零",      "Zenless Zone Zero"},
    {"10", "预研",        "pre-research"},
};
static const GameTag* FindTagByName(const std::string& zh) {
    for (auto& t : kGameTags) if (zh == t.zh) return &t;
    return nullptr;
}
static const GameTag* FindTagById(const std::string& id) {
    for (auto& t : kGameTags) if (id == t.id) return &t;
    return nullptr;
}






static std::string GenerateVid() {
    ULONGLONG t = GetTickCount64();
    uint32_t rnd = 0;
    BCryptGenRandom(nullptr, (PUCHAR)&rnd, sizeof(rnd), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    char buf[32];
    snprintf(buf, sizeof(buf), "%016llx%08x", t, rnd);
    return buf;   
}

static std::string JsonEscape(const std::string& s) {
    std::string r;
    for (char c : s) {
        switch (c) {
            case '"':  r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            default: r += c;
        }
    }
    return r;
}

int CmdInject(int argc, wchar_t** argv) {
    if (argc < 3) { printf("用法: n0va_plugin inject <文件> [--name ..] [--game-name ..] [--author ..] [--format ..]\n"); return 1; }
    std::wstring srcPath = argv[2];

    
    std::string name, tagName, author, format;
    {
        
        size_t slash = srcPath.find_last_of(L"\\/");
        std::wstring fn = (slash == std::wstring::npos) ? srcPath : srcPath.substr(slash + 1);
        size_t dot = fn.find_last_of(L'.');
        if (dot != std::wstring::npos) fn = fn.substr(0, dot);
        name = n0va::WideToUtf8(fn);
    }
    for (int i = 3; i < argc; i++) {
        if (wcscmp(argv[i], L"--name") == 0 && i + 1 < argc) {
            name = n0va::WideToUtf8(argv[++i]);
        } else if (wcscmp(argv[i], L"--game-name") == 0 && i + 1 < argc) {
            tagName = n0va::WideToUtf8(argv[++i]);
        } else if (wcscmp(argv[i], L"--author") == 0 && i + 1 < argc) {
            author = n0va::WideToUtf8(argv[++i]);
        } else if (wcscmp(argv[i], L"--format") == 0 && i + 1 < argc) {
            format = n0va::WideToUtf8(argv[++i]);
        }
    }
    
    
    
    const GameTag* tag = tagName.empty() ? FindTagById("5") : FindTagByName(tagName);
    std::string tagId, tagZh, tagEn;
    if (tag) {
        tagId = tag->id;
        tagZh = tag->zh;
        tagEn = tag->en;
    } else {
        tagId = "-2";          
        tagZh = tagName;
        tagEn = tagName;
        printf("[*] 提示: 游戏名「%s」不在官方清单中, 按自定义分类处理\n", tagName.c_str());
    }
    if (author.empty()) author = tagZh;

    
    if (GetFileAttributesW(srcPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        printf("[!] 文件不存在: %ls\n", srcPath.c_str());
        return 1;
    }
    n0va::MediaFormat mf = n0va::DetectFormat(srcPath);
    if (mf == n0va::MediaFormat::Unsupported) {
        printf("[!] 不支持的格式（仅支持 PNG/JPG/MP4; WebP/AVIF 请先转换）\n");
        return 1;
    }
    if (format.empty())
        format = (mf == n0va::MediaFormat::Mp4) ? "dynamic" : "static";
    printf("[*] 文件: %ls (格式=%s, 类型=%s)\n", srcPath.c_str(),
           mf == n0va::MediaFormat::Png ? "PNG" :
           mf == n0va::MediaFormat::Jpeg ? "JPEG" : "MP4",
           format.c_str());

    uint32_t mediaW = 0, mediaH = 0;
    n0va::GetMediaDimensions(srcPath, mf, mediaW, mediaH);
    std::string sharpness = "sp1920";
    if (mediaW > 0 && mediaH > 0) {
        uint32_t shortSide = mediaW < mediaH ? mediaW : mediaH;
        if (shortSide > 1440) sharpness = "sp4k";
        else if (shortSide > 1080) sharpness = "sp2k";
        printf("[*] 分辨率: %ux%u (档位=%s)\n", mediaW, mediaH, sharpness.c_str());
    } else {
        printf("[*] 分辨率探测失败, 按 1080P 档位处理\n");
    }

    
    std::wstring hostDir = n0va::LoadHostDir();
    if (hostDir.empty()) {
        printf("[!] 找不到 N0vaDesktop 安装目录。\n");
        printf("    请运行: n0va_plugin set-host \"D:\\Program Files\\N0vaDesktop\"\n");
        return 1;
    }
    std::wstring configIni = hostDir + L"\\config.ini";
    std::wstring cacheDir = n0va::GetCacheDir(configIni);
    if (cacheDir.empty()) {
        printf("[!] 无法定位壁纸缓存目录\n");
        return 1;
    }
    std::wstring ndfName = n0va::GenerateNdfFileName();
    std::wstring targetPath;
    if (!n0va::CopyToGameDir(cacheDir, srcPath, ndfName, targetPath)) {
        printf("[!] 复制壁纸文件失败\n");
        return 1;
    }
    printf("[+] 壁纸文件已就位: %ls\n", targetPath.c_str());

    
    std::string vid = GenerateVid();
    std::string opId = n0va::GenerateOpId();
    std::string targetFile(ndfName.begin(), ndfName.end());
    std::string urlFile = "https://n0va-static.mihoyo.com/desk-portal/2022/01/20/" + targetFile;
    std::string urlCover = urlFile;

    char sizeBuf[32] = "0";
    {
        WIN32_FILE_ATTRIBUTE_DATA fad{};
        if (GetFileAttributesExW(targetPath.c_str(), GetFileExInfoStandard, &fad)) {
            ULONGLONG sz = ((ULONGLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
            if (sz > 0) snprintf(sizeBuf, sizeof(sizeBuf), "%llu", sz);
        }
    }

    if (mf == n0va::MediaFormat::Mp4) {
        std::wstring coverName = n0va::GenerateNdfFileName();
        std::wstring coverPath = cacheDir + L"\\game\\" + coverName;
        if (n0va::ExtractFirstFramePng(srcPath, coverPath)) {
            std::string coverFile(coverName.begin(), coverName.end());
            urlCover = "https://n0va-static.mihoyo.com/desk-portal/2022/01/20/" + coverFile;
            printf("[+] 已生成预览图: %ls\n", coverPath.c_str());
        } else {
            printf("[*] 预览图生成失败, 使用视频文件作为预览（可能显示异常）\n");
        }
    }

    std::string previewVideosJson;
    if (mf == n0va::MediaFormat::Mp4) {
        previewVideosJson =
            "[{\"sharpness\":\"" + sharpness + "\",\"video\":\"" + urlFile + "\","
            "\"sign\":\"00000000000000000000000000000000\",\"size\":\"" + sizeBuf + "\","
            "\"video_suffix\":\"ndf\"}]";
    } else {
        previewVideosJson = "[]";
    }

    std::string json =
        "{\"vid\":\"" + vid + "\","
        "\"lang_name\":[{\"lang\":\"zh-cn\",\"name\":\"" + JsonEscape(name) + "\"},"
        "{\"lang\":\"en-us\",\"name\":\"" + JsonEscape(name) + "\"}],"
        "\"cover\":\"" + urlCover + "\","
        "\"type\":\"videotypesilent\","
        "\"sharpness_videos\":[{\"sharpness\":\"" + sharpness + "\",\"video\":\"" + urlFile + "\","
        "\"sign\":\"00000000000000000000000000000000\",\"size\":\"" + sizeBuf + "\",\"video_suffix\":\"ndf\"}],"
        "\"author\":\"" + JsonEscape(author) + "\","
        "\"desc\":[],"
        "\"hd_cover\":\"" + (mf == n0va::MediaFormat::Mp4 ? urlFile : urlCover) + "\","
        "\"tags\":[{\"id\":\"" + tagId + "\",\"lang_name\":[{\"lang\":\"zh-cn\",\"name\":\""
        + tagZh + "\"},{\"lang\":\"en-us\",\"name\":\"" + tagEn + "\"}]}],"
        "\"format\":\"" + format + "\","
        "\"preview_videos\":" + previewVideosJson + "}";

    
    n0va::WallpaperDb db;
    std::wstring dbPath = n0va::GetPluginDir() + L"\\wallpapers.json";
    db.Load(dbPath);
    n0va::WallpaperRecord rec;
    rec.vid = vid;
    rec.op_id = opId;
    rec.state = n0va::WallpaperState::Pending;
    rec.name = name;
    rec.tag_id = tagId;
    rec.author = author;
    rec.format = format;

    rec.source_sha256 = n0va::FileSha256Hex(srcPath);
    rec.target_file = targetFile;
    rec.target_sha256 = n0va::FileSha256Hex(targetPath);
    rec.updated_at = (int64_t)time(nullptr);
    db.upsert(rec);
    db.Save();

    
    std::string req = "{\"protocol\":1,\"id\":1,\"op_id\":\"" + opId + "\","
                      "\"cmd\":\"inject\",\"args\":" + json + "}";
    n0va::PipeResponse resp;
    bool pipeOk = n0va::PipeRequest(n0va::MakePipeName(), req, resp);
    if (!pipeOk || (!resp.ok && !resp.already_exists)) {
        
        
        
        
        const char* err = pipeOk ? resp.error.c_str() : resp.error.c_str();
        printf("[!] 导入失败: %s（已自动清理本次操作的文件和记录）\n", err);
        DeleteFileW(targetPath.c_str());
        db.remove(vid);
        db.Save();
        return 1;
    }

    
    rec.state = n0va::WallpaperState::Active;
    db.upsert(rec);
    db.Save();

    printf("[+] 导入成功!\n");
    printf("    名称:   %s\n", name.c_str());
    printf("    游戏:   %s\n", tagZh.c_str());
    printf("[*] 打开'我的壁纸' → 点击卡片'设置桌面'即可使用\n");
    return 0;
}
