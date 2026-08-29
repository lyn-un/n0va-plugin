#pragma once

#include <cstdint>
#include <string>

namespace n0va {

enum class MediaFormat {
    Png,
    Jpeg,
    Mp4,
    Unsupported,   
};


MediaFormat DetectFormat(const std::wstring& path);


std::wstring GetCacheDir(const std::wstring& configIniPath);


std::wstring GenerateNdfFileName();


bool CopyToGameDir(const std::wstring& cacheDir, const std::wstring& srcPath,
                   const std::wstring& targetFileName, std::wstring& outPath);


bool ExtractFirstFramePng(const std::wstring& videoPath, const std::wstring& pngPath);


bool GetMediaDimensions(const std::wstring& path, MediaFormat mf, uint32_t& width, uint32_t& height);


std::wstring FindN0vaInstallDir();

} 
