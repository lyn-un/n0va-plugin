#include <windows.h>
#include <cstdio>
#include <string>

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "ole32.lib")

#include "fs_helper.h"

int wmain(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);
    if (argc < 2) { printf("用法: test_firstframe <视频路径>\n"); return 1; }
    std::wstring video = argv[1];

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    printf("CoInitializeEx: 0x%08X\n", (unsigned)hr);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return 1;

    hr = MFStartup(MF_VERSION);
    printf("MFStartup: 0x%08X\n", (unsigned)hr);

    IMFSourceReader* reader = nullptr;
    hr = MFCreateSourceReaderFromURL(video.c_str(), nullptr, &reader);
    printf("MFCreateSourceReaderFromURL: 0x%08X\n", (unsigned)hr);

    IMFMediaType* outType = nullptr;
    hr = MFCreateMediaType(&outType);
    printf("MFCreateMediaType: 0x%08X\n", (unsigned)hr);
    if (SUCCEEDED(hr)) {
        hr = outType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        printf("SetGUID(Major): 0x%08X\n", (unsigned)hr);
        hr = outType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        printf("SetGUID(RGB32): 0x%08X\n", (unsigned)hr);
        hr = reader->SetCurrentMediaType(
            (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, outType);
        printf("SetCurrentMediaType(RGB32): 0x%08X\n", (unsigned)hr);
        outType->Release();
    }

    IMFSample* sample = nullptr;
    DWORD flags = 0;
    LONGLONG ts = 0;
    DWORD streamIdx = 0;
    hr = reader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0,
                            &streamIdx, &flags, &ts, &sample);
    printf("ReadSample: 0x%08X stream=%lu flags=0x%lX sample=%p\n",
           (unsigned)hr, streamIdx, flags, (void*)sample);

    if (SUCCEEDED(hr) && sample) {
        IMFMediaType* mt = nullptr;
        hr = reader->GetCurrentMediaType(
            (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &mt);
        printf("GetCurrentMediaType: 0x%08X\n", (unsigned)hr);
        if (SUCCEEDED(hr) && mt) {
            GUID sub = {0};
            mt->GetGUID(MF_MT_SUBTYPE, &sub);
            UINT32 w = 0, h = 0;
            MFGetAttributeSize(mt, MF_MT_FRAME_SIZE, &w, &h);
            printf("  subtype: {%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}\n",
                   sub.Data1, sub.Data2, sub.Data3,
                   sub.Data4[0], sub.Data4[1], sub.Data4[2], sub.Data4[3],
                   sub.Data4[4], sub.Data4[5], sub.Data4[6], sub.Data4[7]);
            printf("  size: %u x %u\n", w, h);
            mt->Release();
        }
        sample->Release();
    }

    reader->Release();
    MFShutdown();
    CoUninitialize();

    std::wstring png = video + L".frame.png";
    bool ok = n0va::ExtractFirstFramePng(video, png);
    printf("ExtractFirstFramePng: %s\n", ok ? "成功" : "失败");
    if (ok) {
        DWORD sz = GetFileAttributesW(png.c_str()) != INVALID_FILE_ATTRIBUTES ? 1 : 0;
        printf("文件存在: %s\n", sz ? "是" : "否");
    }
    return 0;
}
