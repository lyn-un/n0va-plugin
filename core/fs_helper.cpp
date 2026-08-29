
#include "fs_helper.h"

#include <windows.h>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <random>

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <objbase.h>
#include <wincodec.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

#include "text_conv.h"

namespace n0va {

MediaFormat DetectFormat(const std::wstring& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return MediaFormat::Unsupported;
    uint8_t head[16] = {0};
    f.read((char*)head, sizeof(head));
    size_t n = (size_t)f.gcount();
    if (n >= 8) {
        
        if (head[0] == 0x89 && head[1] == 0x50 && head[2] == 0x4E && head[3] == 0x47)
            return MediaFormat::Png;
        
        if (head[0] == 0xFF && head[1] == 0xD8 && head[2] == 0xFF)
            return MediaFormat::Jpeg;
    }
    if (n >= 12) {
        
        if (memcmp(head + 4, "ftyp", 4) == 0)
            return MediaFormat::Mp4;
    }
    return MediaFormat::Unsupported;
}

std::wstring GetCacheDir(const std::wstring& configIniPath) {
    
    std::ifstream f(configIniPath, std::ios::binary);
    if (!f) return {};
    std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    size_t p = s.find("cache_dir=");
    if (p == std::string::npos) return {};
    p += 10;
    size_t e = s.find_first_of("\r\n", p);
    std::string dir = s.substr(p, e == std::string::npos ? std::string::npos : e - p);
    
    size_t b = dir.find_first_not_of(" \t");
    size_t en = dir.find_last_not_of(" \t");
    if (b == std::string::npos) return {};
    dir = dir.substr(b, en - b + 1);
    
    
    
    return AnsiToWide(dir);
}

std::wstring GenerateNdfFileName() {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::mt19937 rng((unsigned)ms);
    int rnd = rng() % 1000;
    wchar_t buf[64];
    swprintf(buf, 64, L"%lld_%d.ndf", (long long)ms, rnd);
    return buf;
}

bool CopyToGameDir(const std::wstring& cacheDir, const std::wstring& srcPath,
                   const std::wstring& targetFileName, std::wstring& outPath) {
    std::wstring gameDir = cacheDir;
    if (!gameDir.empty() && gameDir.back() != L'\\') gameDir += L'\\';
    gameDir += L"game";

    CreateDirectoryW(gameDir.c_str(), nullptr);
    outPath = gameDir + L"\\" + targetFileName;

    MediaFormat mf = DetectFormat(srcPath);
    if (mf == MediaFormat::Mp4) {
        std::ifstream fin(srcPath, std::ios::binary);
        if (!fin) return false;
        std::ofstream fout(outPath, std::ios::binary | std::ios::trunc);
        if (!fout) return false;
        const char prefix[2] = {0x00, 0x00};
        fout.write(prefix, 2);
        fout << fin.rdbuf();
        fin.close();
        fout.close();
        return fout.good();
    }
    return CopyFileW(srcPath.c_str(), outPath.c_str(), FALSE) != 0;
}

bool ExtractFirstFramePng(const std::wstring& videoPath, const std::wstring& pngPath) {
    HRESULT hr = S_OK;
    IMFSourceReader* reader = nullptr;
    IWICImagingFactory* wicFactory = nullptr;
    IWICBitmapEncoder* encoder = nullptr;
    IWICStream* stream = nullptr;
    IWICBitmapFrameEncode* frameEncode = nullptr;
    bool ok = false;

    hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;
    hr = S_OK;

    hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) { CoUninitialize(); return false; }

    if (SUCCEEDED(hr)) {
        hr = MFCreateSourceReaderFromURL(videoPath.c_str(), nullptr, &reader);
    }
    if (SUCCEEDED(hr)) {
        hr = reader->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
    }
    if (SUCCEEDED(hr)) {
        hr = reader->SetStreamSelection(
            (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
    }

    IMFMediaType* outType = nullptr;
    if (SUCCEEDED(hr)) {
        hr = MFCreateMediaType(&outType);
    }
    if (SUCCEEDED(hr)) {
        hr = outType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    }
    if (SUCCEEDED(hr)) {
        hr = outType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
    }
    if (SUCCEEDED(hr)) {
        hr = reader->SetCurrentMediaType(
            (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, outType);
    }
    if (outType) outType->Release();

    IMFSample* sample = nullptr;
    IMFMediaBuffer* buffer = nullptr;
    if (SUCCEEDED(hr)) {
        DWORD flags = 0;
        hr = reader->ReadSample(
            (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0,
            nullptr, &flags, nullptr, &sample);
    }

    BYTE* pixelData = nullptr;
    DWORD pixelLen = 0;
    UINT32 width = 0, height = 0;
    if (SUCCEEDED(hr) && sample) {
        IMFMediaType* mt = nullptr;
        hr = reader->GetCurrentMediaType(
            (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &mt);
        if (SUCCEEDED(hr) && mt) {
            MFGetAttributeSize(mt, MF_MT_FRAME_SIZE, &width, &height);
            mt->Release();
        }
        hr = sample->ConvertToContiguousBuffer(&buffer);
        if (SUCCEEDED(hr)) {
            hr = buffer->Lock(&pixelData, nullptr, &pixelLen);
        }
    }

    if (SUCCEEDED(hr) && pixelData && width > 0 && height > 0) {
        hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                              CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory));
    }

    if (SUCCEEDED(hr)) {
        hr = wicFactory->CreateStream(&stream);
    }
    if (SUCCEEDED(hr)) {
        hr = stream->InitializeFromFilename(pngPath.c_str(), GENERIC_WRITE);
    }
    if (SUCCEEDED(hr)) {
        hr = wicFactory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
    }
    if (SUCCEEDED(hr)) {
        hr = encoder->Initialize(stream, WICBitmapEncoderNoCache);
    }
    if (SUCCEEDED(hr)) {
        hr = encoder->CreateNewFrame(&frameEncode, nullptr);
    }
    if (SUCCEEDED(hr)) {
        hr = frameEncode->Initialize(nullptr);
    }
    if (SUCCEEDED(hr)) {
        hr = frameEncode->SetSize(width, height);
    }

    WICPixelFormatGUID pf = GUID_WICPixelFormat24bppBGR;
    if (SUCCEEDED(hr)) {
        hr = frameEncode->SetPixelFormat(&pf);
    }
    if (SUCCEEDED(hr)) {
        UINT stride = width * 3;
        UINT totalSize = stride * height;
        BYTE* converted = new (std::nothrow) BYTE[totalSize];
        if (!converted) hr = E_OUTOFMEMORY;
        if (SUCCEEDED(hr)) {
            memset(converted, 0, totalSize);
            for (UINT y = 0; y < height; y++) {
                for (UINT x = 0; x < width; x++) {
                    UINT yi = y * width + x;
                    UINT ui = width * height + (y / 2) * width + (x & ~1u);
                    BYTE yv = pixelData[yi];
                    BYTE cb = pixelData[ui];
                    BYTE cr = pixelData[ui + 1];
                    int r = yv + ((cr - 128) * 1436 >> 10);
                    int g = yv - ((cb - 128) * 352 >> 10) - ((cr - 128) * 731 >> 10);
                    int b = yv + ((cb - 128) * 1815 >> 10);
                    UINT di = y * stride + x * 3;
                    converted[di]     = (BYTE)(b < 0 ? 0 : (b > 255 ? 255 : b));
                    converted[di + 1] = (BYTE)(g < 0 ? 0 : (g > 255 ? 255 : g));
                    converted[di + 2] = (BYTE)(r < 0 ? 0 : (r > 255 ? 255 : r));
                }
            }
            hr = frameEncode->WritePixels(height, stride, totalSize, converted);
            delete[] converted;
        }
    }
    if (SUCCEEDED(hr)) {
        hr = frameEncode->Commit();
    }
    if (SUCCEEDED(hr)) {
        hr = encoder->Commit();
        ok = true;
    }

    if (buffer) { if (pixelData) buffer->Unlock(); buffer->Release(); }
    if (sample) sample->Release();
    if (frameEncode) frameEncode->Release();
    if (encoder) encoder->Release();
    if (stream) stream->Release();
    if (wicFactory) wicFactory->Release();
    if (reader) reader->Release();
    MFShutdown();
    CoUninitialize();

    if (!ok) {
        wchar_t buf[256];
        swprintf(buf, 256, L"[fs] 抓帧失败 hr=0x%08X (%ux%u)\n",
                 (unsigned)hr, width, height);
        OutputDebugStringW(buf);
    }
    return ok;
}

bool GetMediaDimensions(const std::wstring& path, MediaFormat mf, uint32_t& width, uint32_t& height) {
    width = 0; height = 0;

    if (mf == MediaFormat::Png) {
        std::ifstream f(path, std::ios::binary);
        if (!f) return false;
        uint8_t hdr[24] = {0};
        f.read((char*)hdr, sizeof(hdr));
        if (f.gcount() < (std::streamsize)sizeof(hdr)) return false;
        if (hdr[12] != 'I' || hdr[13] != 'H' || hdr[14] != 'D' || hdr[15] != 'R') return false;
        uint32_t w = ((uint32_t)hdr[16] << 24) | ((uint32_t)hdr[17] << 16) |
                     ((uint32_t)hdr[18] << 8) | hdr[19];
        uint32_t h = ((uint32_t)hdr[20] << 24) | ((uint32_t)hdr[21] << 16) |
                     ((uint32_t)hdr[22] << 8) | hdr[23];
        if (w == 0 || h == 0) return false;
        width = w; height = h;
        return true;
    }

    if (mf == MediaFormat::Jpeg) {
        std::ifstream f(path, std::ios::binary);
        if (!f) return false;
        uint16_t marker = 0;
        auto readU16 = [&f](uint16_t& v) {
            uint8_t b[2];
            f.read((char*)b, 2);
            v = ((uint16_t)b[0] << 8) | b[1];
            return f.gcount() == 2;
        };
        uint8_t b[2];
        f.read((char*)b, 2);
        if (f.gcount() < 2 || b[0] != 0xFF || b[1] != 0xD8) return false;
        for (;;) {
            if (!f.read((char*)b, 1)) return false;
            while (b[0] == 0xFF) {
                if (!f.read((char*)b, 1)) return false;
            }
            marker = 0xFF00u | b[0];
            if (marker == 0xFFD9 || marker == 0xFFDA) return false;
            uint16_t segLen = 0;
            if (!readU16(segLen)) return false;
            if (segLen < 2) return false;
            if (marker >= 0xFFC0 && marker <= 0xFFCF && marker != 0xFFC4 && marker != 0xFFC8 && marker != 0xFFCC) {
                uint8_t precision = 0;
                f.read((char*)&precision, 1);
                uint8_t hh[4] = {0};
                f.read((char*)hh, 4);
                if (f.gcount() < 4) return false;
                uint32_t h = ((uint32_t)hh[0] << 8) | hh[1];
                uint32_t w = ((uint32_t)hh[2] << 8) | hh[3];
                if (w == 0 || h == 0) return false;
                width = w; height = h;
                return true;
            }
            f.seekg(segLen - 2, std::ios::cur);
        }
    }

    if (mf == MediaFormat::Mp4) {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;
        hr = MFStartup(MF_VERSION);
        if (FAILED(hr)) { CoUninitialize(); return false; }

        IMFSourceReader* reader = nullptr;
        if (SUCCEEDED(hr)) {
            hr = MFCreateSourceReaderFromURL(path.c_str(), nullptr, &reader);
        }
        if (SUCCEEDED(hr)) {
            hr = reader->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
        }
        if (SUCCEEDED(hr)) {
            IMFMediaType* mt = nullptr;
            hr = reader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &mt);
            if (SUCCEEDED(hr) && mt) {
                UINT32 w = 0, h = 0;
                hr = MFGetAttributeSize(mt, MF_MT_FRAME_SIZE, &w, &h);
                if (SUCCEEDED(hr) && w > 0 && h > 0) {
                    width = w; height = h;
                } else {
                    hr = E_FAIL;
                }
                mt->Release();
            }
        }
        if (reader) reader->Release();
        MFShutdown();
        CoUninitialize();
        return width > 0 && height > 0;
    }

    return false;
}

namespace {




static std::wstring ScanUninstallKey(HKEY root, const wchar_t* sub) {
    HKEY hk = nullptr;
    if (RegOpenKeyExW(root, sub, 0, KEY_READ, &hk) != ERROR_SUCCESS) return {};
    std::wstring found;
    for (DWORD i = 0;; i++) {
        wchar_t keyName[256];
        DWORD knLen = 256;
        if (RegEnumKeyExW(hk, i, keyName, &knLen, nullptr, nullptr, nullptr, nullptr)
            != ERROR_SUCCESS) break;
        HKEY k = nullptr;
        if (RegOpenKeyExW(hk, keyName, 0, KEY_READ, &k) != ERROR_SUCCESS) continue;
        
        {
            wchar_t v[1024] = {0}; DWORD vs = sizeof(v); DWORD vt = 0;
            if (RegQueryValueExW(k, L"InstallLocation", nullptr, &vt, (LPBYTE)v, &vs)
                    == ERROR_SUCCESS && (vt == REG_SZ || vt == REG_EXPAND_SZ) && v[0]) {
                std::wstring dir = v;
                while (!dir.empty() && dir.back() == L'\\') dir.pop_back();
                if (GetFileAttributesW((dir + L"\\N0vaDesktop.exe").c_str())
                        != INVALID_FILE_ATTRIBUTES) { found = dir; RegCloseKey(k); break; }
            }
        }
        
        if (found.empty()) {
            for (const wchar_t* vn : { L"DisplayIcon", L"UninstallString" }) {
                wchar_t v[1024] = {0}; DWORD vs = sizeof(v); DWORD vt = 0;
                if (RegQueryValueExW(k, vn, nullptr, &vt, (LPBYTE)v, &vs) != ERROR_SUCCESS
                        || (vt != REG_SZ && vt != REG_EXPAND_SZ)) continue;
                std::wstring val = v;
                if (val.find(L"N0vaDesktop") == std::wstring::npos) continue;
                size_t b = val.find_first_not_of(L"\" ");
                size_t e = val.find_last_not_of(L"\" ");
                if (b == std::wstring::npos) continue;
                val = val.substr(b, e - b + 1);
                size_t slash = val.find_last_of(L"\\/");
                if (slash == std::wstring::npos) continue;
                std::wstring dir = val.substr(0, slash);
                if (GetFileAttributesW((dir + L"\\N0vaDesktop.exe").c_str())
                        != INVALID_FILE_ATTRIBUTES) { found = dir; break; }
            }
        }
        RegCloseKey(k);
        if (!found.empty()) break;
    }
    RegCloseKey(hk);
    return found;
}

} 

std::wstring FindN0vaInstallDir() {
    auto exists = [](const std::wstring& dir) {
        return !dir.empty() &&
               GetFileAttributesW((dir + L"\\N0vaDesktop.exe").c_str())
                   != INVALID_FILE_ATTRIBUTES;
    };
    
    {
        wchar_t self[MAX_PATH];
        if (GetModuleFileNameW(nullptr, self, MAX_PATH) > 0) {
            std::wstring p(self);
            size_t sl = p.find_last_of(L'\\');
            if (sl != std::wstring::npos) {
                std::wstring dir = p.substr(0, sl);
                if (exists(dir)) return dir;
            }
        }
    }
    
    {
        std::wstring dir = ScanUninstallKey(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall");
        if (dir.empty())
            dir = ScanUninstallKey(HKEY_LOCAL_MACHINE,
                L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall");
        if (dir.empty())
            dir = ScanUninstallKey(HKEY_CURRENT_USER,
                L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall");
        if (!dir.empty()) return dir;
    }
    
    {
        wchar_t pf[MAX_PATH] = {0};
        if (GetEnvironmentVariableW(L"ProgramFiles", pf, MAX_PATH) > 0 &&
            exists(std::wstring(pf) + L"\\N0vaDesktop"))
            return std::wstring(pf) + L"\\N0vaDesktop";
        wchar_t pfx[MAX_PATH] = {0};
        if (GetEnvironmentVariableW(L"ProgramFiles(x86)", pfx, MAX_PATH) > 0 &&
            exists(std::wstring(pfx) + L"\\N0vaDesktop"))
            return std::wstring(pfx) + L"\\N0vaDesktop";
    }
    return {};   
}

} 
