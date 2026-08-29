














#include "dllmain.h"
#include "logging.h"

#include <atomic>
#include <cstdio>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "detours.h"



void ExecuteInject(const std::string& requestJson, std::string& outResponse);
void ExecuteRemove(const std::string& vid, std::string& outResponse);
bool InstallSubB6D00HookIfNeeded();

void N0vaPluginNotifyResponse(uint32_t id, const std::string& respJson);

namespace {


enum class TaskType { Inject, Remove };
struct Task {
    TaskType type;
    std::string payload;
    uint32_t reqId = 0;
};

std::mutex g_queueMutex;
std::queue<Task> g_pending;




void (__cdecl *TrueSendPostedEvents)(void*, int) = nullptr;





static thread_local bool g_inHook = false;
static volatile LONG g_hookCallCount = 0;   

void HookSendPostedEvents(void* self, int flags) {
    
    {
        LONG c = InterlockedIncrement(&g_hookCallCount);
        if (c % 1000 == 1) {
            size_t pending = 0;
            { std::lock_guard<std::mutex> lk(g_queueMutex); pending = g_pending.size(); }
            Log("[n0va] hook calls=%ld pending=%zu\n", (long)c, pending);
        }
    }

    if (g_inHook) {
        
        TrueSendPostedEvents(self, flags);
        return;
    }
    g_inHook = true;
    
    std::queue<Task> tasks;
    {
        std::lock_guard<std::mutex> lk(g_queueMutex);
        tasks.swap(g_pending);
    }
    
    while (!tasks.empty()) {
        Task t = std::move(tasks.front());
        tasks.pop();
        try {
            switch (t.type) {
                case TaskType::Inject: {
                    std::string resp;
                    Log("[n0va] executing inject task (main thread)...\n");
                    
                    
                    ExecuteInject(t.payload, resp);
                    Log("[n0va] inject done: %s\n", resp.c_str());
                    if (t.reqId) N0vaPluginNotifyResponse(t.reqId, resp);
                    break;
                }
                case TaskType::Remove: {
                    std::string resp;
                    Log("[n0va] executing remove task...\n");
                    ExecuteRemove(t.payload, resp);
                    Log("[n0va] remove done: %s\n", resp.c_str());
                    if (t.reqId) N0vaPluginNotifyResponse(t.reqId, resp);
                    break;
                }
            }
        } catch (...) {
            Log("[n0va] task exception caught\n");
        }
    }
    
    TrueSendPostedEvents(self, flags);
    g_inHook = false;
}





bool (__thiscall *TrueProcessEvents)(void* self, void* flags) = nullptr;

bool HookProcessEvents(void* self, void* flags) {
    
    if (!g_inHook) {
        g_inHook = true;
        std::queue<Task> tasks;
        {
            std::lock_guard<std::mutex> lk(g_queueMutex);
            tasks.swap(g_pending);
        }
        while (!tasks.empty()) {
            Task t = std::move(tasks.front());
            tasks.pop();
            try {
                switch (t.type) {
                    case TaskType::Inject: {
                        std::string resp;
                        Log("[n0va] executing inject task (processEvents)...\n");
                        ExecuteInject(t.payload, resp);
                        Log("[n0va] inject done: %s\n", resp.c_str());
                        if (t.reqId) N0vaPluginNotifyResponse(t.reqId, resp);
                        break;
                    }
                    case TaskType::Remove: {
                        std::string resp;
                        ExecuteRemove(t.payload, resp);
                        if (t.reqId) N0vaPluginNotifyResponse(t.reqId, resp);
                        break;
                    }
                }
            } catch (...) {
                Log("[n0va] task exception (processEvents)\n");
            }
        }
        g_inHook = false;
    }
    return TrueProcessEvents(self, flags);
}


void* FindQtExport(const char* substr) {
    HMODULE qt = GetModuleHandleW(L"Qt5Core.dll");
    if (!qt) return nullptr;
    auto* dos = (IMAGE_DOS_HEADER*)qt;
    auto* nt = (IMAGE_NT_HEADERS*)((uint8_t*)qt + dos->e_lfanew);
    auto& expDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (expDir.VirtualAddress == 0) return nullptr;
    auto* exp = (IMAGE_EXPORT_DIRECTORY*)((uint8_t*)qt + expDir.VirtualAddress);
    for (DWORD i = 0; i < exp->NumberOfNames; i++) {
        const char* name = (const char*)((uint8_t*)qt + ((DWORD*)(
            (uint8_t*)qt + exp->AddressOfNames))[i]);
        if (strstr(name, substr)) {
            WORD ordinal = ((WORD*)((uint8_t*)qt + exp->AddressOfNameOrdinals))[i];
            return (void*)((uint8_t*)qt + ((DWORD*)(
                (uint8_t*)qt + exp->AddressOfFunctions))[ordinal]);
        }
    }
    return nullptr;
}


void* FindSendPostedEvents() {
    HMODULE qt = GetModuleHandleW(L"Qt5Core.dll");
    if (!qt) return nullptr;
    auto* dos = (IMAGE_DOS_HEADER*)qt;
    auto* nt = (IMAGE_NT_HEADERS*)((uint8_t*)qt + dos->e_lfanew);
    auto& expDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (expDir.VirtualAddress == 0) return nullptr;
    auto* exp = (IMAGE_EXPORT_DIRECTORY*)((uint8_t*)qt + expDir.VirtualAddress);
    for (DWORD i = 0; i < exp->NumberOfNames; i++) {
        const char* name = (const char*)((uint8_t*)qt + ((DWORD*)(
            (uint8_t*)qt + exp->AddressOfNames))[i]);
        if (strstr(name, "sendPostedEvents")) {
            WORD ordinal = ((WORD*)((uint8_t*)qt + exp->AddressOfNameOrdinals))[i];
            void* fn = (void*)((uint8_t*)qt + ((DWORD*)(
                (uint8_t*)qt + exp->AddressOfFunctions))[ordinal]);
            Log("[n0va] found sendPostedEvents @ %p (%s)\n", fn, name);
            return fn;
        }
    }
    return nullptr;
}

bool g_hookInstalled = false;
bool g_subB6D00Installed = false;

} 



bool N0vaPluginInitHost() {
    
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    HANDLE h = CreateFileW(exePath, GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD sz = GetFileSize(h, nullptr);
    CloseHandle(h);

    
    DWORD deadline = GetTickCount() + 20000;
    while (!GetModuleHandleW(L"Qt5Core.dll")) {
        if (GetTickCount() > deadline) { Log("[n0va] Qt5Core not loaded\n"); return false; }
        Sleep(100);
    }
    Log("[n0va] Qt5Core loaded\n");

    
    void* spe = FindSendPostedEvents();
    if (!spe) { Log("[n0va] sendPostedEvents not found\n"); return false; }
    TrueSendPostedEvents = (void (__cdecl*)(void*, int))spe;

    
    void* pe = FindQtExport("processEvents@QEventDispatcherWin32@@UEAA");
    if (!pe) { Log("[n0va] processEvents not found (次要, 继续)\n"); }
    else {
        TrueProcessEvents = (bool (__thiscall*)(void*, void*))pe;
        Log("[n0va] found processEvents @ %p\n", pe);
    }

    LONG err = DetourTransactionBegin();
    if (err != NO_ERROR) return false;
    err = DetourUpdateThread(GetCurrentThread());
    if (err != NO_ERROR) { DetourTransactionAbort(); return false; }
    err = DetourAttach(reinterpret_cast<PVOID*>(&TrueSendPostedEvents),
                       HookSendPostedEvents);
    if (err != NO_ERROR) { DetourTransactionAbort(); return false; }
    if (TrueProcessEvents) {
        err = DetourAttach(reinterpret_cast<PVOID*>(&TrueProcessEvents),
                           HookProcessEvents);
        if (err != NO_ERROR) {
            Log("[n0va] processEvents attach failed (次要, 继续)\n");
        }
    }
    err = DetourTransactionCommit();
    if (err != NO_ERROR) return false;
    g_hookInstalled = true;
    Log("[n0va] hooks installed (sendPostedEvents + processEvents)\n");
    return true;
}

void N0vaPluginInstallHook() {
    g_subB6D00Installed = InstallSubB6D00HookIfNeeded();
    if (g_subB6D00Installed)
        Log("[n0va] subB6D00 hook installed\n");
    else
        Log("[n0va] subB6D00 hook NOT installed (milestone 0: likely unnecessary)\n");
}




static void WakeMainThread() {
    struct Ctx { HWND w; DWORD pid; } ctx = {nullptr, GetCurrentProcessId()};
    EnumWindows([](HWND h, LPARAM lp) -> BOOL {
        DWORD pid = 0;
        GetWindowThreadProcessId(h, &pid);
        if (pid != ((Ctx*)lp)->pid) return TRUE;
        wchar_t cls[64] = {0};
        GetClassNameW(h, cls, 64);
        if (wcscmp(cls, L"Qt5QWindowIcon") == 0) {
            ((Ctx*)lp)->w = h;
            return FALSE;   
        }
        return TRUE;
    }, (LPARAM)&ctx);
    if (ctx.w) {
        
        for (int i = 0; i < 8; i++) PostMessageW(ctx.w, WM_NULL, 0, 0);
    }
}

void N0vaPluginQueueInject(const std::string& requestJson, uint32_t reqId) {
    std::lock_guard<std::mutex> lk(g_queueMutex);
    Task t;
    t.type = TaskType::Inject;
    t.payload = requestJson;
    t.reqId = reqId;
    g_pending.push(std::move(t));
    WakeMainThread();
}

void N0vaPluginQueueRemove(const std::string& vid, uint32_t reqId) {
    std::lock_guard<std::mutex> lk(g_queueMutex);
    Task t;
    t.type = TaskType::Remove;
    t.payload = vid;
    t.reqId = reqId;
    g_pending.push(std::move(t));
    WakeMainThread();
}
