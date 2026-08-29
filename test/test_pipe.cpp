

#include <windows.h>
#include <cstdio>
#include <string>
#include <thread>

#include "pipe_client.h"

static volatile bool g_stop = false;




static void MockServer() {
    std::wstring full = L"\\\\.\\pipe\\" + n0va::MakePipeName();
    int served = 0;
    HANDLE pipe = CreateNamedPipeW(full.c_str(),
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_REJECT_REMOTE_CLIENTS,
        PIPE_UNLIMITED_INSTANCES, 4096, 4096, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) { printf("[mock] create failed\n"); return; }
    while (!g_stop && served < 4) {
        if (!ConnectNamedPipe(pipe, nullptr) &&
            GetLastError() != ERROR_PIPE_CONNECTED) { Sleep(50); continue; }
        printf("[mock] client connected (served=%d)\n", served);
        char buf[4096];
        DWORD n = 0;
        if (!ReadFile(pipe, buf, sizeof(buf) - 1, &n, nullptr) || n == 0) {
            printf("[mock] read failed (err=%lu)\n", GetLastError());
            DisconnectNamedPipe(pipe);
            continue;
        }
        std::string line(buf, n);
        size_t nl = line.find('\n');
        if (nl != std::string::npos) line.resize(nl);

        std::string resp;
        if (line.find("\"cmd\":\"ping\"") != std::string::npos)
            resp = "{\"protocol\":1,\"ok\":true,\"data\":{\"plugin\":\"0.1.0\",\"host\":\"2.2.1.4\"}}";
        else if (line.find("\"vid\":\"b\"") != std::string::npos)
            resp = "{\"ok\":true,\"data\":{\"already_exists\":true}}";
        else if (line.find("\"cmd\":\"fail\"") != std::string::npos)
            resp = "{\"ok\":false,\"error\":\"E_INJECT_FAILED\"}";
        else
            resp = "{\"ok\":true,\"data\":{\"entry_status\":3}}";
        resp += "\n";
        DWORD w = 0;
        WriteFile(pipe, resp.data(), (DWORD)resp.size(), &w, nullptr);
        FlushFileBuffers(pipe);
        DisconnectNamedPipe(pipe);   
        served++;
    }
    CloseHandle(pipe);
}

int wmain() {
    SetConsoleOutputCP(CP_UTF8);
    printf("=== 管道协议集成测试 ===\n");
    std::thread srv(MockServer);
    Sleep(300);  

    int fail = 0;
    
    {
        n0va::PipeResponse r;
        bool ok = n0va::PipeRequest(n0va::MakePipeName(),
            "{\"protocol\":1,\"id\":1,\"op_id\":\"t1\",\"cmd\":\"ping\",\"args\":{}}", r);
        printf("[1] ping: %s raw=%s\n", ok && r.ok ? "通过" : "失败", r.raw.c_str());
        if (!ok || !r.ok) fail++;
    }
    
    {
        n0va::PipeResponse r;
        bool ok = n0va::PipeRequest(n0va::MakePipeName(),
            "{\"protocol\":1,\"id\":2,\"op_id\":\"t2\",\"cmd\":\"inject\",\"args\":{\"vid\":\"a\"}}", r);
        printf("[2] inject: %s raw=%s err=%s\n",
               ok && r.ok ? "通过" : "失败", r.raw.c_str(), r.error.c_str());
        if (!ok || !r.ok) fail++;
    }
    
    {
        n0va::PipeResponse r;
        bool ok = n0va::PipeRequest(n0va::MakePipeName(),
            "{\"protocol\":1,\"id\":3,\"op_id\":\"t3\",\"cmd\":\"inject\",\"args\":{\"vid\":\"b\"}}", r);
        printf("[3] already_exists: %s (flag=%d)\n",
               ok && r.ok && r.already_exists ? "通过" : "失败", (int)r.already_exists);
        if (!ok || !r.already_exists) fail++;
    }
    
    {
        n0va::PipeResponse r;
        bool ok = n0va::PipeRequest(n0va::MakePipeName(),
            "{\"protocol\":1,\"id\":4,\"op_id\":\"t4\",\"cmd\":\"fail\",\"args\":{}}", r);
        printf("[4] 失败解析: %s (error=%s)\n",
               ok && !r.ok && r.error == "E_INJECT_FAILED" ? "通过" : "失败", r.error.c_str());
        if (!ok || r.ok) fail++;
    }
    
    {
        n0va::PipeResponse r;
        bool ok = n0va::PipeRequest(L"n0va_plugin_no_such_pipe",
            "{\"protocol\":1,\"id\":5,\"cmd\":\"ping\"}", r, 300, 500);
        printf("[5] 无管道超时: %s (error=%s)\n",
               !ok && r.error == "E_NO_PIPE" ? "通过" : "失败", r.error.c_str());
        if (ok) fail++;
    }

    g_stop = true;
    srv.join();
    printf("=== %s ===\n", fail == 0 ? "全部通过 ✓" : "存在失败 ✗");
    return fail == 0 ? 0 : 1;
}
