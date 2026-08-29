
#include "dllmain.h"
#include "logging.h"

#include <cstdio>
#include <sddl.h>
#include <string>
#include <vector>


void N0vaPluginQueueInject(const std::string& requestJson, uint32_t reqId);
void N0vaPluginQueueRemove(const std::string& vid, uint32_t reqId);

bool N0vaPluginListStoreVids(std::string& outVidsJson);




#include <condition_variable>
#include <deque>
#include <mutex>

namespace {

struct PendingResult {
    std::string resp;
    bool done = false;
};
std::mutex g_respMutex;
std::condition_variable g_respCv;
std::deque<std::pair<uint32_t, std::string>> g_respQueue;  





void PushResponse(uint32_t id, const std::string& resp) {
    std::lock_guard<std::mutex> lk(g_respMutex);
    g_respQueue.push_back({id, resp});
    g_respCv.notify_all();
}


bool WaitResponse(uint32_t id, std::string& resp) {
    std::unique_lock<std::mutex> lk(g_respMutex);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (g_respCv.wait_until(lk, deadline) != std::cv_status::timeout) {
        for (auto it = g_respQueue.begin(); it != g_respQueue.end(); ++it) {
            if (it->first == id) {
                resp = it->second;
                g_respQueue.erase(it);
                return true;
            }
        }
    }
    return false;
}


struct ParsedRequest {
    uint32_t id = 0;
    std::string cmd;
    std::string vid;
    std::string raw;
};
bool ParseRequest(const std::string& line, ParsedRequest& pr) {
    pr.raw = line;
    auto grabNum = [&](const char* key) -> uint32_t {
        std::string pat = std::string("\"") + key + "\":";
        size_t p = line.find(pat);
        if (p == std::string::npos) return 0;
        p += pat.size();
        return (uint32_t)strtoul(line.c_str() + p, nullptr, 10);
    };
    auto grabStr = [&](const char* key) -> std::string {
        std::string pat = std::string("\"") + key + "\":\"";
        size_t p = line.find(pat);
        if (p == std::string::npos) return {};
        p += pat.size();
        size_t e = line.find('"', p);
        if (e == std::string::npos) return {};
        return line.substr(p, e - p);
    };
    pr.id = grabNum("id");
    pr.cmd = grabStr("cmd");
    
    {
        std::string pat = "\"vid\":\"";
        size_t p = line.find(pat);
        if (p != std::string::npos) {
            p += pat.size();
            size_t e = line.find('"', p);
            if (e != std::string::npos) pr.vid = line.substr(p, e - p);
        }
    }
    return pr.id != 0 || !pr.cmd.empty();
}

} 


void N0vaPluginNotifyResponse(uint32_t id, const std::string& respJson) {
    PushResponse(id, respJson);
}


bool N0vaPluginStartPipeServer() {
    
    
    
    std::wstring sidStr;
    {
        HANDLE tok = nullptr;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) {
            DWORD len = 0;
            GetTokenInformation(tok, TokenUser, nullptr, 0, &len);
            std::vector<uint8_t> buf(len);
            if (GetTokenInformation(tok, TokenUser, buf.data(), len, &len)) {
                auto* tu = (TOKEN_USER*)buf.data();
                LPWSTR str = nullptr;
                if (ConvertSidToStringSidW(tu->User.Sid, &str)) {
                    sidStr = str;
                    LocalFree(str);
                }
            }
            CloseHandle(tok);
        }
        for (auto& c : sidStr) if (c == L'\\') c = L'-';  
    }
    std::wstring pipeName = L"\\\\.\\pipe\\n0va_plugin_" + sidStr;

    
    
    
    
    
    std::wstring sddlStr;
    {
        HANDLE tok = nullptr;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) {
            DWORD len = 0;
            GetTokenInformation(tok, TokenUser, nullptr, 0, &len);
            std::vector<uint8_t> ubuf(len);
            if (GetTokenInformation(tok, TokenUser, ubuf.data(), len, &len)) {
                auto* tu = (TOKEN_USER*)ubuf.data();
                LPWSTR usid = nullptr;
                if (ConvertSidToStringSidW(tu->User.Sid, &usid)) {
                    sddlStr = std::wstring(L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;") +
                              usid + L")";
                    LocalFree(usid);
                }
            }
            CloseHandle(tok);
        }
    }
    SECURITY_ATTRIBUTES sa{};
    PSECURITY_DESCRIPTOR psd = nullptr;
    if (sddlStr.empty() ||
        !ConvertStringSecurityDescriptorToSecurityDescriptorW(sddlStr.c_str(), SDDL_REVISION_1,
                                                               &psd, nullptr)) {
        Log("[n0va] SDDL 转换失败, 用默认描述符\n");
        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = nullptr;
        sa.bInheritHandle = FALSE;
    } else {
        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = psd;
        sa.bInheritHandle = FALSE;
    }

    
    
    HANDLE pipe = CreateNamedPipeW(
        pipeName.c_str(),
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_REJECT_REMOTE_CLIENTS,
        PIPE_UNLIMITED_INSTANCES, 4096, 4096, 0, &sa);
    if (psd) LocalFree(psd);   
    if (pipe == INVALID_HANDLE_VALUE) {
        Log("[n0va] CreateNamedPipe failed %lu\n", GetLastError());
        return false;
    }
    for (;;) {
        if (!ConnectNamedPipe(pipe, nullptr)) {
            DWORD err = GetLastError();
            if (err != ERROR_PIPE_CONNECTED) {
                Log("[n0va] ConnectNamedPipe err=%lu (retry)\n", err);
                Sleep(500);
                continue;
            }
        }
        Log("[n0va] client connected\n");

        
        
        char chunk[4096];
        DWORD nread = 0;
        std::string line;
        
        {
            DWORD avail = 0, dl = GetTickCount() + 5000;
            while (GetTickCount() < dl) {
                if (PeekNamedPipe(pipe, nullptr, 0, nullptr, &avail, nullptr) && avail > 0)
                    break;
                Sleep(10);
            }
        }
        if (!ReadFile(pipe, chunk, sizeof(chunk) - 1, &nread, nullptr) || nread == 0) {
            Log("[n0va] read failed (err=%lu)\n", GetLastError());
            DisconnectNamedPipe(pipe);
            continue;
        }
        line.assign(chunk, nread);
        if (line.size() > 1024 * 1024) line.clear();   
        size_t nl = line.find('\n');
        if (nl != std::string::npos) line.resize(nl);
        Log("[n0va] recv: %s\n", line.c_str());

        ParsedRequest pr;
        std::string resp;
        if (!ParseRequest(line, pr)) {
            resp = "{\"ok\":false,\"error\":\"E_BAD_FORMAT\"}";
        } else if (pr.cmd == "ping") {
            resp = "{\"protocol\":1,\"ok\":true,\"data\":{\"plugin\":\"0.1.0\",\"host\":\"2.2.1.4\"}}";
        } else if (pr.cmd == "inject") {
            
            
            
            Log("[n0va] queueing inject to main thread hook...\n");
            N0vaPluginQueueInject(line, pr.id);
            if (!WaitResponse(pr.id, resp)) {
                resp = "{\"ok\":false,\"error\":\"E_TIMEOUT\"}";
            }
        } else if (pr.cmd == "remove") {
            Log("[n0va] queueing remove to main thread hook...\n");
            N0vaPluginQueueRemove(pr.vid, pr.id);
            if (!WaitResponse(pr.id, resp)) {
                resp = "{\"ok\":false,\"error\":\"E_TIMEOUT\"}";
            }
        } else if (pr.cmd == "list") {
            
            std::string vidsJson;
            if (N0vaPluginListStoreVids(vidsJson))
                resp = "{\"ok\":true,\"data\":{\"vids\":" + vidsJson + "}}";
            else
                resp = "{\"ok\":true,\"data\":{\"vids\":[]}}";
        } else {
            resp = "{\"ok\":false,\"error\":\"E_UNKNOWN_CMD\"}";
        }

        
        resp += "\n";
        DWORD written = 0;
        WriteFile(pipe, resp.data(), (DWORD)resp.size(), &written, nullptr);
        FlushFileBuffers(pipe);
        DisconnectNamedPipe(pipe);   
    }
    return true;
}
