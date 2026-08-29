#include <windows.h>
#include <cstdio>
#include <string>

#include "fs_helper.h"

int wmain(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);
    if (argc < 2) { printf("用法: test_dimensions <文件>\n"); return 1; }
    std::wstring path = argv[1];

    n0va::MediaFormat mf = n0va::DetectFormat(path);
    const char* fmt = mf == n0va::MediaFormat::Png ? "PNG" :
                      mf == n0va::MediaFormat::Jpeg ? "JPEG" :
                      mf == n0va::MediaFormat::Mp4 ? "MP4" : "UNSUPPORTED";
    printf("格式: %s\n", fmt);

    uint32_t w = 0, h = 0;
    bool ok = n0va::GetMediaDimensions(path, mf, w, h);
    if (ok) {
        uint32_t s = w < h ? w : h;
        const char* sharp = s > 1440 ? "sp4k" : (s > 1080 ? "sp2k" : "sp1920");
        printf("分辨率: %ux%u -> 档位 %s\n", w, h, sharp);
        return 0;
    }
    printf("探测失败\n");
    return 1;
}
