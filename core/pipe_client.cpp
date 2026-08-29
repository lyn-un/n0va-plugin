
#include "pipe_client.h"

#include <windows.h>
#include <sddl.h>
#include <objbase.h>
#include <algorithm>
#include <chrono>
#include <random>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ole32.lib")

namespace n0va {

static std::wstring CurrentUserSidString() {
    HANDLE tok = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) return L"0";
    DWORD len = 0;
    GetTokenInformation(tok, TokenUser, nullptr, 0, &len);
    std::vector<uint8_t> buf(len);
    std::wstring sid;
    if (GetTokenInformation(tok, TokenUser, buf.data(), len, &len)) {
        auto* tu = (TOKEN_USER*)buf.data();
        LPWSTR str = nullptr;
        if (ConvertSidToStringSidW(tu->User.Sid, &str)) {
            sid = str;
            LocalFree(str);
        }
    }
    CloseHandle(tok);
    
    for (auto& c : sid) if (c == L'\\') c = L'-';
    return sid;
}

std::wstring MakePipeName() {
    return L"n0va_plugin_" + CurrentUserSidString();
}

std::string GenerateOpId() {
    GUID g;
    if (FAILED(CoCreateGuid(&g))) {
        
        auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return "op_" + std::to_string(now) + "_" + std::to_string(rand());
    }
    char s[40];
    snprintf(s, sizeof(s),
             "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             g.Data1, g.Data2, g.Data3,
             g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3],
             g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]);
    return s;
}

bool PipeRequest(const std::wstring& pipeName,
                 const std::string& requestJson,
                 PipeResponse& resp,
                 uint32_t connectTimeoutMs,
                 uint32_t responseTimeoutMs) {
    std::wstring full = L"\\\\.\\pipe\\" + pipeName;

    
    HANDLE pipe = INVALID_HANDLE_VALUE;
    {
        DWORD deadline = GetTickCount() + connectTimeoutMs;
        for (;;) {
            if (WaitNamedPipeW(full.c_str(), 200)) break;
            if (GetTickCount() > deadline) {
                resp.ok = false;
                resp.error = "E_NO_PIPE";
                return false;
            }
            Sleep(50);
        }
    }
    pipe = CreateFileW(full.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                       OPEN_EXISTING, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
        resp.ok = false;
        resp.error = "E_NO_PIPE";
        return false;
    }
    DWORD mode = PIPE_READMODE_MESSAGE;
    SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr);

    
    std::string line = requestJson + "\n";
    DWORD written = 0;
    if (!WriteFile(pipe, line.data(), (DWORD)line.size(), &written, nullptr) ||
        written != line.size()) {
        CloseHandle(pipe);
        resp.ok = false;
        resp.error = "E_WRITE_FAILED";
        return false;
    }

    
    std::string buf;
    char chunk[4096];
    DWORD deadline = GetTickCount() + responseTimeoutMs;
    for (;;) {
        DWORD avail = 0;
        if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &avail, nullptr)) break;
        if (avail > 0) {
            DWORD n = 0;
            DWORD toRead = (avail < sizeof(chunk)) ? avail : (DWORD)sizeof(chunk);
            if (!ReadFile(pipe, chunk, toRead, &n, nullptr) || n == 0)
                break;
            buf.append(chunk, n);
            if (buf.size() > 1024 * 1024) { buf.clear(); break; }  
            if (buf.find('\n') != std::string::npos) break;
        } else {
            if (GetTickCount() > deadline) break;
            Sleep(10);
        }
    }
    CloseHandle(pipe);

    
    size_t nl = buf.find('\n');
    if (nl != std::string::npos) buf.resize(nl);
    if (buf.empty()) {
        resp.ok = false;
        resp.error = "E_RESPONSE_EMPTY";
        return false;
    }
    resp.raw = buf;

    
    auto has = [&](const char* s) { return buf.find(s) != std::string::npos; };
    resp.ok = has("\"ok\":true") || has("\"ok\": true");
    resp.already_exists = has("already_exists");
    if (!resp.ok) {
        size_t p = buf.find("\"error\"");
        if (p != std::string::npos) {
            p = buf.find('"', p + 7);
            if (p != std::string::npos) {
                size_t e = buf.find('"', p + 1);
                if (e != std::string::npos) resp.error = buf.substr(p + 1, e - p - 1);
            }
        }
        if (resp.error.empty()) resp.error = "E_UNKNOWN";
    }
    return true;
}

} 
