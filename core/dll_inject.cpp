












#include "dll_inject.h"

#include <windows.h>
#include <bcrypt.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

#include "text_conv.h"   

#pragma comment(lib, "bcrypt.lib")

namespace n0va {


std::string FileSha256Hex(const std::wstring& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
    uint8_t hash[32] = {0};
    
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0)
        return {};
    BCRYPT_HASH_HANDLE hh = nullptr;
    NTSTATUS st = BCryptCreateHash(alg, &hh, nullptr, 0, nullptr, 0, 0);
    if (st == 0)
        st = BCryptHashData(hh, data.data(), (ULONG)data.size(), 0);
    if (st == 0)
        st = BCryptFinishHash(hh, hash, sizeof(hash), 0);
    if (hh) BCryptDestroyHash(hh);
    BCryptCloseAlgorithmProvider(alg, 0);
    if (st != 0) return {};
    char hex[65];
    for (int i = 0; i < 32; i++) sprintf(hex + i * 2, "%02x", hash[i]);
    return hex;
}


static std::wstring DefaultStatePath(const std::wstring& exePath) {
    return exePath + L".n0va_install.json";
}

bool LoadInstallState(const std::wstring& statePath, InstallState& st) {
    
    std::ifstream f(statePath, std::ios::binary);
    if (!f) return false;
    std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    auto grab = [&](const char* key) -> std::string {
        std::string pat = std::string("\"") + key + "\":\"";
        size_t p = s.find(pat);
        if (p == std::string::npos) return {};
        p += pat.size();
        size_t e = s.find('"', p);
        if (e == std::string::npos) return {};
        return s.substr(p, e - p);
    };
    st.original_sha256 = grab("original_sha256");
    st.patched_sha256 = grab("patched_sha256");
    st.host_version = grab("host_version");
    st.dll_sha256 = grab("dll_sha256");
    st.plugin_dir = grab("plugin_dir");
    return !st.original_sha256.empty();
}

bool SaveInstallState(const std::wstring& statePath, const InstallState& st) {
    std::string s = "{\n";
    s += "  \"original_sha256\":\"" + st.original_sha256 + "\",\n";
    s += "  \"patched_sha256\":\"" + st.patched_sha256 + "\",\n";
    s += "  \"host_version\":\"" + st.host_version + "\",\n";
    s += "  \"dll_sha256\":\"" + st.dll_sha256 + "\",\n";
    s += "  \"plugin_dir\":\"" + st.plugin_dir + "\",\n";
    s += "  \"install_time\":" + std::to_string(st.install_time) + "\n";
    s += "}\n";
    
    std::wstring tmp = statePath + L".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f.write(s.data(), (std::streamsize)s.size());
        f.flush();
        if (!f) return false;
    }
    if (!MoveFileExW(tmp.c_str(), statePath.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        return false;
    return true;
}


namespace {

struct PeInfo {
    uint8_t* data = nullptr;
    size_t   size = 0;
    IMAGE_DOS_HEADER* dos = nullptr;
    IMAGE_NT_HEADERS64* nt = nullptr;
    IMAGE_SECTION_HEADER* secs = nullptr;
    uint16_t nSecs = 0;
    bool ok = false;
};

bool ParsePe(std::vector<uint8_t>& buf, PeInfo& pi) {
    if (buf.size() < sizeof(IMAGE_DOS_HEADER)) return false;
    pi.data = buf.data();
    pi.size = buf.size();
    pi.dos = (IMAGE_DOS_HEADER*)pi.data;
    if (pi.dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    if (pi.size < pi.dos->e_lfanew + sizeof(IMAGE_NT_HEADERS64)) return false;
    pi.nt = (IMAGE_NT_HEADERS64*)(pi.data + pi.dos->e_lfanew);
    if (pi.nt->Signature != IMAGE_NT_SIGNATURE) return false;
    if (pi.nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) return false;
    if (pi.nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return false;
    pi.nSecs = pi.nt->FileHeader.NumberOfSections;
    size_t secOff = pi.dos->e_lfanew + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) +
                    pi.nt->FileHeader.SizeOfOptionalHeader;
    if (secOff + (size_t)pi.nSecs * sizeof(IMAGE_SECTION_HEADER) > pi.size) return false;
    pi.secs = (IMAGE_SECTION_HEADER*)(pi.data + secOff);
    pi.ok = true;
    return true;
}


bool SectionTableHasRoom(PeInfo& pi) {
    size_t headersEnd = pi.dos->e_lfanew + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) +
                        pi.nt->FileHeader.SizeOfOptionalHeader +
                        (size_t)(pi.nSecs + 1) * sizeof(IMAGE_SECTION_HEADER);
    if (headersEnd > pi.size) return false;
    
    if (pi.nSecs > 0) {
        size_t firstRaw = pi.secs[0].PointerToRawData;
        if (firstRaw != 0 && headersEnd > firstRaw) return false;
    }
    return true;
}

} 


bool IsPatched(const std::wstring& exePath, const std::wstring& dllName) {
    std::ifstream f(exePath, std::ios::binary);
    if (!f) return false;
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(f)),
                             std::istreambuf_iterator<char>());
    PeInfo pi;
    if (!ParsePe(buf, pi)) return false;
    auto& opt = pi.nt->OptionalHeader;
    auto& impDir = opt.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (impDir.VirtualAddress == 0 || impDir.Size == 0) return false;
    uint32_t rva = impDir.VirtualAddress;
    
    auto rvaToOff = [&](uint32_t r) -> uint32_t {
        for (int i = 0; i < pi.nSecs; i++) {
            auto& s = pi.secs[i];
            if (r >= s.VirtualAddress && r < s.VirtualAddress + s.Misc.VirtualSize)
                return r - s.VirtualAddress + s.PointerToRawData;
        }
        return 0;
    };
    uint32_t off = rvaToOff(rva);
    if (off == 0 || off + sizeof(IMAGE_IMPORT_DESCRIPTOR) > pi.size) return false;
    for (auto* d = (IMAGE_IMPORT_DESCRIPTOR*)(pi.data + off);
         d->Name != 0 && d->FirstThunk != 0; d++) {
        uint32_t nameOff = rvaToOff(d->Name);
        if (nameOff == 0 || nameOff >= pi.size) continue;
        const char* name = (const char*)(pi.data + nameOff);
        
        std::string narrow = WideToUtf8(dllName);
        if (_stricmp(name, narrow.c_str()) == 0) return true;
    }
    return false;
}


bool ApplyDllInject(const std::wstring& exePath,
                   const std::wstring& dllName,
                   const std::wstring& exportName,
                   InstallState& outState) {
    
    std::ifstream fin(exePath, std::ios::binary);
    if (!fin) return false;
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(fin)),
                             std::istreambuf_iterator<char>());
    fin.close();

    outState.original_sha256 = FileSha256Hex(exePath);
    if (outState.original_sha256.empty()) return false;

    
    PeInfo pi;
    if (!ParsePe(buf, pi)) return false;

    
    if (!SectionTableHasRoom(pi)) return false;

    auto& fh = pi.nt->FileHeader;
    auto& opt = pi.nt->OptionalHeader;
    uint32_t fileAlign = opt.FileAlignment;
    uint32_t sectAlign = opt.SectionAlignment;
    if (fileAlign == 0 || sectAlign == 0) return false;  

    
    
    
    
    auto& oldImp = opt.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    auto rvaToOffOld = [&](uint32_t r) -> uint32_t {
        for (int i = 0; i < pi.nSecs; i++) {
            auto& s = pi.secs[i];
            if (r >= s.VirtualAddress && r < s.VirtualAddress + s.Misc.VirtualSize)
                return r - s.VirtualAddress + s.PointerToRawData;
        }
        return 0;
    };
    std::vector<IMAGE_IMPORT_DESCRIPTOR> oldDescs;
    {
        uint32_t off = rvaToOffOld(oldImp.VirtualAddress);
        if (off == 0) return false;
        size_t maxDesc = oldImp.Size / sizeof(IMAGE_IMPORT_DESCRIPTOR);
        for (size_t i = 0; i < maxDesc; i++) {
            IMAGE_IMPORT_DESCRIPTOR d;
            memcpy(&d, buf.data() + off + i * sizeof(IMAGE_IMPORT_DESCRIPTOR), sizeof(d));
            if (d.Name == 0 && d.FirstThunk == 0) break;   
            if (d.Name == 0 || d.FirstThunk == 0) return false;  
            oldDescs.push_back(d);
        }
        if (oldDescs.empty()) return false;
    }

    
    
    
    
    
    
    
    
    std::string narrowDll = WideToUtf8(dllName);
    std::string narrowExp = WideToUtf8(exportName);
    if (narrowDll.empty() || narrowExp.empty()) return false;

    size_t descSize = (oldDescs.size() + 2) * sizeof(IMAGE_IMPORT_DESCRIPTOR);  
    size_t oftSize  = 2 * sizeof(uint64_t);                   
    size_t ftSize   = 2 * sizeof(uint64_t);                   
    size_t hintNameSize = 2 + narrowExp.size() + 1;           
    hintNameSize = (hintNameSize + 1) & ~size_t(1);           
    size_t dllNameSize = narrowDll.size() + 1;

    
    size_t rawSize = descSize + oftSize + ftSize + hintNameSize + dllNameSize;
    rawSize = (rawSize + fileAlign - 1) / fileAlign * fileAlign;
    size_t virtSize = (rawSize + sectAlign - 1) / sectAlign * sectAlign;  

    
    uint32_t newRva = 0;
    for (int i = 0; i < pi.nSecs; i++) {
        uint32_t end = pi.secs[i].VirtualAddress +
                       ((pi.secs[i].Misc.VirtualSize + sectAlign - 1) / sectAlign * sectAlign);
        if (end > newRva) newRva = end;
    }
    
    uint32_t newRaw = (uint32_t)buf.size();
    newRaw = (newRaw + fileAlign - 1) / fileAlign * fileAlign;

    
    uint32_t newSizeOfImage = newRva + (uint32_t)virtSize;
    if (newSizeOfImage < opt.SizeOfImage) newSizeOfImage = opt.SizeOfImage;

    
    buf.resize(newRaw + rawSize, 0);
    uint8_t* secData = buf.data() + newRaw;

    uint32_t offDesc  = 0;
    uint32_t offOft   = offDesc + (uint32_t)descSize;
    uint32_t offFt    = offOft + (uint32_t)oftSize;
    uint32_t offHint  = offFt + (uint32_t)ftSize;
    uint32_t offDll   = offHint + (uint32_t)hintNameSize;
    (void)offDll; 

    
    auto* descArr = (IMAGE_IMPORT_DESCRIPTOR*)(secData + offDesc);
    memcpy(descArr, oldDescs.data(), oldDescs.size() * sizeof(IMAGE_IMPORT_DESCRIPTOR));
    auto* desc = descArr + oldDescs.size();   
    auto* descNull = desc + 1;
    memset(descNull, 0, sizeof(*descNull));

    auto* oft = (uint64_t*)(secData + offOft);
    auto* ft  = (uint64_t*)(secData + offFt);
    oft[0] = newRva + offHint;   
    oft[1] = 0;
    ft[0]  = newRva + offHint;   
    ft[1]  = 0;

    auto* hintName = (IMAGE_IMPORT_BY_NAME*)(secData + offHint);
    hintName->Hint = 0;
    memcpy(hintName->Name, narrowExp.c_str(), narrowExp.size() + 1);

    char* dllStr = (char*)(secData + offDll);
    memcpy(dllStr, narrowDll.c_str(), narrowDll.size() + 1);

    desc->OriginalFirstThunk = newRva + (uint32_t)offOft;
    desc->TimeDateStamp = 0;
    desc->ForwarderChain = 0;
    desc->Name = newRva + (uint32_t)offDll;
    desc->FirstThunk = newRva + (uint32_t)offFt;

    
    auto& s = pi.secs[pi.nSecs];   
    memset(&s, 0, sizeof(s));
    memcpy(s.Name, ".n0va", 5);
    s.Misc.VirtualSize = (uint32_t)virtSize;
    s.VirtualAddress = newRva;
    s.SizeOfRawData = (uint32_t)rawSize;
    s.PointerToRawData = newRaw;
    
    
    
    s.Characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE |
                        IMAGE_SCN_CNT_INITIALIZED_DATA;

    fh.NumberOfSections = pi.nSecs + 1;          
    opt.SizeOfImage = newSizeOfImage;            
    opt.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress = newRva + (uint32_t)offDesc;
    opt.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size = (uint32_t)descSize;

    
    

    
    std::wstring tmpPath = exePath + L".n0vatmp";
    {
        std::ofstream fout(tmpPath, std::ios::binary | std::ios::trunc);
        if (!fout) return false;
        fout.write((const char*)buf.data(), (std::streamsize)buf.size());
        fout.flush();
        if (!fout) return false;
    }

    
    std::string err = ValidateInjectedPe(tmpPath, dllName);
    if (!err.empty()) {
        
        DeleteFileW(tmpPath.c_str());
        return false;
    }

    
    outState.patched_sha256 = FileSha256Hex(tmpPath);
    if (outState.patched_sha256.empty()) { DeleteFileW(tmpPath.c_str()); return false; }

    
    if (!MoveFileExW(tmpPath.c_str(), exePath.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmpPath.c_str());
        return false;
    }
    return true;
}


bool RestoreOriginal(const std::wstring& exePath,
                     const std::wstring& backupPath,
                     std::string& reason) {
    std::wstring statePath = DefaultStatePath(exePath);
    InstallState st;
    if (!LoadInstallState(statePath, st)) {
        reason = "install state missing";
        return false;
    }
    std::string cur = FileSha256Hex(exePath);
    if (cur == st.original_sha256) { reason = "already_original"; return true; }
    if (cur != st.patched_sha256) {
        reason = "host_updated";   
        return false;
    }
    
    std::ifstream fin(backupPath, std::ios::binary);
    if (!fin) { reason = "backup_missing"; return false; }
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(fin)),
                             std::istreambuf_iterator<char>());
    fin.close();
    if (buf.empty()) { reason = "backup_empty"; return false; }
    std::wstring tmpPath = exePath + L".n0vatmp";
    {
        std::ofstream fout(tmpPath, std::ios::binary | std::ios::trunc);
        if (!fout) { reason = "write_failed"; return false; }
        fout.write((const char*)buf.data(), (std::streamsize)buf.size());
        fout.flush();
        if (!fout) { reason = "write_failed"; return false; }
    }
    if (!MoveFileExW(tmpPath.c_str(), exePath.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmpPath.c_str());
        reason = "replace_failed";
        return false;
    }
    reason = "restored";
    return true;
}


std::string ValidateInjectedPe(const std::wstring& exePath, const std::wstring& dllName) {
    std::ifstream f(exePath, std::ios::binary);
    if (!f) return "open failed";
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(f)),
                             std::istreambuf_iterator<char>());
    PeInfo pi;
    if (!ParsePe(buf, pi)) return "PE parse failed";

    auto& opt = pi.nt->OptionalHeader;
    auto& impDir = opt.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (impDir.VirtualAddress == 0) return "no import directory";

    
    const IMAGE_SECTION_HEADER* n0va = nullptr;
    for (int i = 0; i < pi.nt->FileHeader.NumberOfSections; i++) {
        if (memcmp(pi.secs[i].Name, ".n0va", 5) == 0) { n0va = &pi.secs[i]; break; }
    }
    if (!n0va) return "no .n0va section";

    
    auto inSec = [&](uint32_t rva) {
        return rva >= n0va->VirtualAddress &&
               rva < n0va->VirtualAddress + n0va->Misc.VirtualSize;
    };
    if (!inSec(impDir.VirtualAddress)) return "import dir not in .n0va";
    if (impDir.Size < 2 * sizeof(IMAGE_IMPORT_DESCRIPTOR)) return "import dir too small";

    auto rvaToOff = [&](uint32_t r) -> uint32_t {
        for (int i = 0; i < pi.nt->FileHeader.NumberOfSections; i++) {
            auto& s = pi.secs[i];
            if (r >= s.VirtualAddress && r < s.VirtualAddress + s.Misc.VirtualSize)
                return r - s.VirtualAddress + s.PointerToRawData;
        }
        return 0;
    };

    
    
    
    uint32_t off = rvaToOff(impDir.VirtualAddress);
    auto* dArr = (IMAGE_IMPORT_DESCRIPTOR*)(pi.data + off);
    size_t maxDesc = impDir.Size / sizeof(IMAGE_IMPORT_DESCRIPTOR);
    const IMAGE_IMPORT_DESCRIPTOR* ours = nullptr;
    size_t ourIdx = 0, total = 0;
    for (size_t i = 0; i < maxDesc; i++) {
        auto& dd = dArr[i];
        if (dd.Name == 0 && dd.FirstThunk == 0) { total = i; break; }  
        if (dd.Name == 0 || dd.FirstThunk == 0) return "descriptor malformed";
        total = i + 1;
        uint32_t nameOff = rvaToOff(dd.Name);
        if (nameOff == 0 || nameOff >= pi.size) return "descriptor name invalid";
        std::string narrow = WideToUtf8(dllName);
        if (_stricmp((const char*)(pi.data + nameOff), narrow.c_str()) == 0) {
            ours = &dd; ourIdx = i;
        }
    }
    if (total == 0) return "no descriptors";
    if (total > maxDesc) return "no null terminator descriptor";
    if (!ours) return "plugin descriptor not found";
    if (ourIdx + 1 != total) return "plugin descriptor not last";   

    
    if (!inSec(ours->Name) || !inSec(ours->FirstThunk) || !inSec(ours->OriginalFirstThunk))
        return "plugin descriptor fields outside .n0va";
    uint32_t ftOff = rvaToOff(ours->FirstThunk);
    auto* ft = (uint64_t*)(pi.data + ftOff);
    if (ft[0] == 0 || ft[1] != 0) return "thunk invalid";
    uint32_t oftOff = rvaToOff(ours->OriginalFirstThunk);
    auto* oft = (uint64_t*)(pi.data + oftOff);
    if (oft[0] != ft[0] || oft[1] != 0) return "oft/ft mismatch";
    
    if (n0va->SizeOfRawData % opt.FileAlignment != 0) return "raw size misaligned";
    if (n0va->Misc.VirtualSize % opt.SectionAlignment != 0) return "virtual size misaligned";
    uint32_t secEnd = n0va->VirtualAddress + n0va->Misc.VirtualSize;
    if (secEnd > opt.SizeOfImage) return "SizeOfImage too small";
    
    
    if (!(n0va->Characteristics & IMAGE_SCN_MEM_WRITE))
        return "no MEM_WRITE on .n0va (IAT snap would crash loader)";
    
    uint32_t hnOff = rvaToOff(ft[0]);
    if (hnOff == 0 || hnOff + 3 >= pi.size) return "hint/name invalid";
    return {};
}

} 
