













#include "dllmain.h"
#include "logging.h"

#include <cstdio>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>

#include "detours.h"
#include "version_profile.h"
#include "text_conv.h"   


void N0vaPluginQueueInject(const std::string& requestJson, uint32_t reqId);

namespace {




uintptr_t Base() { return (uintptr_t)GetModuleHandleW(nullptr); }
const auto& P() { return n0va::currentProfile(); }





using FnJsonToVideoItem = void (*)(void* item, void* variant, uint8_t* success);

using FnGetModelManager = void* (*)();

using FnAddDownloaded = char (*)(void* mm, void* item);

using FnGetVDC = void* (*)();

using FnSyncDl = void (*)(void* vdc);

using FnGetRSM = void* (*)();

using FnRsmInsert = void (*)(void* rsm, void* item);

using FnRsmFill = void (*)(void* rsm, void* item, uint8_t flag, void* a4);

using FnFindByKey = void* (*)(void* rsm, void* vidQStr, uint32_t sharpness);

using FnSig12 = void (*)(void* vdc, void* listPtr);

using FnCopyVideoItem = void* (*)(void* dst, void* src);

using FnDestroyVideoItem = void (*)(void* item);






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

struct QtApi {
    
    void* (__cdecl *qbaFromRawData)(void* ret, const char* data, int size) = nullptr;
    
    void (__cdecl *jsonFromJson)(void* ret, void* qba, void* parseErr) = nullptr;
    
    
    
    void (__cdecl *jsonToVariant)(void* jsonDoc, void* variantOut) = nullptr;
    
    void (__thiscall *qbaDestroy)(void* self) = nullptr;      
    void (__thiscall *jsonDestroy)(void* self) = nullptr;     
    void (__thiscall *variantDestroy)(void* self) = nullptr;  
    bool ok = false;
    bool resolved = false;   

    bool init() {
        if (resolved) return ok;
        resolved = true;
        
        qbaFromRawData = (decltype(qbaFromRawData))FindQtExport("fromRawData@QByteArray@@SA");
        jsonFromJson   = (decltype(jsonFromJson))FindQtExport("fromJson@QJsonDocument@@SA");
        jsonToVariant  = (decltype(jsonToVariant))FindQtExport("toVariant@QJsonDocument@@QEBA");
        
        qbaDestroy     = (decltype(qbaDestroy))FindQtExport("??1QByteArray@@QEAA");
        jsonDestroy    = (decltype(jsonDestroy))FindQtExport("??1QJsonDocument@@QEAA");
        variantDestroy = (decltype(variantDestroy))FindQtExport("??1QVariant@@QEAA");
        ok = qbaFromRawData && jsonFromJson && jsonToVariant &&
             qbaDestroy && jsonDestroy && variantDestroy;
        Log("[n0va] Qt api: fromRawData=%p fromJson=%p toVariant=%p\n",
            qbaFromRawData, jsonFromJson, jsonToVariant);
        if (!ok) Log("[n0va] Qt api resolve failed: %p %p %p / %p %p %p\n",
                     qbaFromRawData, jsonFromJson, jsonToVariant,
                     qbaDestroy, jsonDestroy, variantDestroy);
        return ok;
    }
};
QtApi g_qt;



std::mutex g_storeMutex;

struct OwnedItem {
    void* p = nullptr;
    void acquire(void* src) {
        release();
        p = operator new(P().sizeof_video_item);
        memset(p, 0, P().sizeof_video_item);
        ((FnCopyVideoItem)(Base() + P().rva_copyVideoItem))(p, src);
    }
    void release() {
        if (p) {
            ((FnDestroyVideoItem)(Base() + P().rva_destroyVideoItem))(p);
            operator delete(p);
            p = nullptr;
        }
    }
};
std::unordered_map<std::string, OwnedItem> g_store;

std::unordered_map<std::string, std::string> g_opResults;
std::vector<std::string> g_opOrder;




static bool CallAddDownloadedSafe(void* mm, void* item, char& outRet) {
    __try {
        outRet = ((FnAddDownloaded)(Base() + P().rva_addDownloaded))(mm, item);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("[n0va] addDownloaded 异常 (code=0x%lx)\n", GetExceptionCode());
        return false;
    }
}


void* BuildVideoItemFromJson(const std::string& json, bool& ok) {
    if (!g_qt.init()) { ok = false; return nullptr; }
    
    uint8_t qba[0x10] = {0};
    g_qt.qbaFromRawData(qba, json.data(), (int)json.size());
    
    uint8_t jsonDoc[0x10] = {0};
    uint8_t parseErr[0x10] = {0};
    g_qt.jsonFromJson(jsonDoc, qba, parseErr);
    
    int parseCode = *(int*)(parseErr + 4);
    Log("[n0va] json parse err code=%d (0=NoError)\n", parseCode);
    if (parseCode != 0) {
        g_qt.jsonDestroy(jsonDoc);
        g_qt.qbaDestroy(qba);
        ok = false; return nullptr;
    }
    
    
    
    uint8_t variant[0x40] = {0};
    g_qt.jsonToVariant(jsonDoc, variant);
    Log("[n0va] variant[0]=%llx variant[1]=%llx\n",
        *(unsigned long long*)(variant), *(unsigned long long*)(variant + 8));
    
    
    
    void* item = operator new(0x100);
    memset(item, 0, 0x100);
    uint8_t success = 0;
    Log("[n0va] calling jsonToVideoItem(item=%p, variant=%p)...\n", item, variant);
    ((FnJsonToVideoItem)(Base() + P().rva_jsonToVideoItem))(item, variant, &success);
    Log("[n0va] jsonToVideoItem success=%d\n", success);
    
    {
        void* vidD = *(void**)((uint8_t*)item + 8);
        int vlen = vidD ? *(int*)((uint8_t*)vidD + 4) : -1;
        void* listD = *(void**)((uint8_t*)item + 0x28);
        int vcount = listD ? (*(int*)((uint8_t*)listD + 0xC) - *(int*)((uint8_t*)listD + 8)) : -1;
        Log("[n0va] item vid len=%d 变体数=%d\n", vlen, vcount);
    }
    
    g_qt.variantDestroy(variant);
    g_qt.jsonDestroy(jsonDoc);
    g_qt.qbaDestroy(qba);
    if (!success) { operator delete(item); ok = false; return nullptr; }
    ok = true;
    return item;
}

bool DoInject(const std::string& json, std::string& outResp) {
    
    std::string vid;
    {
        size_t p = json.find("\"vid\"");
        if (p != std::string::npos) {
            p = json.find('"', p + 5);
            if (p != std::string::npos) {
                size_t e = json.find('"', p + 1);
                if (e != std::string::npos) vid = json.substr(p + 1, e - p - 1);
            }
        }
    }
    if (vid.empty()) { outResp = "{\"ok\":false,\"error\":\"E_BAD_FORMAT\"}"; return false; }

    
    {
        std::lock_guard<std::mutex> lk(g_storeMutex);
        if (g_store.count(vid)) {
            outResp = "{\"ok\":true,\"data\":{\"already_exists\":true}}";
            return true;
        }
    }

    
    bool ok = false;
    void* item = BuildVideoItemFromJson(json, ok);
    if (!ok || !item) { outResp = "{\"ok\":false,\"error\":\"E_JSON_PARSE\"}"; return false; }

    
    auto* mm = ((FnGetModelManager)(Base() + P().rva_getModelManager))();
    if (!mm) { outResp = "{\"ok\":false,\"error\":\"E_MM_NULL\"}"; return false; }
    Log("[n0va] calling addDownloaded(mm=%p, item=%p)...\n", mm, item);
    char r = 0;
    if (!CallAddDownloadedSafe(mm, item, r)) {
        outResp = "{\"ok\":false,\"error\":\"E_ADD_DL_CRASH\"}";
        return false;
    }
    Log("[n0va] addDownloaded ret=%d\n", r);

    
    auto* vdc = ((FnGetVDC)(Base() + P().rva_getVDC))();
    if (!vdc) { outResp = "{\"ok\":false,\"error\":\"E_VDC_NULL\"}"; return false; }
    ((FnSyncDl)(Base() + P().rva_syncDl))(vdc);

    
    auto* rsm = ((FnGetRSM)(Base() + P().rva_rsmGetInstance))();
    if (!rsm) { outResp = "{\"ok\":false,\"error\":\"E_RSM_NULL\"}"; return false; }
    ((FnRsmInsert)(Base() + P().rva_rsmInsert))(rsm, item);
    ((FnRsmFill)(Base() + P().rva_rsmFill))(rsm, item, 0, nullptr);

    
    
    void* entry = ((FnFindByKey)(Base() + P().rva_rsmFindByKey))(
        rsm, (uint8_t*)item + P().off_videoitem_vid, 0);
    int status = entry ? *(int*)((uint8_t*)entry + P().off_rsm_status) : -1;
    if (entry) {
        *(uint8_t*)((uint8_t*)entry + P().off_rsm_isCurrent) = 0;  
        Log("[n0va] entry=%p status=%d (expect %d)\n", entry, status,
            P().injected_success_status);
    }

    
    void* dm = *(void**)((uint8_t*)vdc + 240);
    if (dm) {
        ((FnSig12)(Base() + P().rva_sig12))(vdc, (uint8_t*)dm + 8);
    }

    
    {
        std::lock_guard<std::mutex> lk(g_storeMutex);
        g_store[vid].acquire(item);
    }
    
    ((FnDestroyVideoItem)(Base() + P().rva_destroyVideoItem))(item);
    operator delete(item);

    char resp[256];
    snprintf(resp, sizeof(resp),
             "{\"ok\":true,\"data\":{\"entry_status\":%d,\"vid\":\"%s\"}}", status, vid.c_str());
    outResp = resp;
    return status == P().injected_success_status;
}








using FnRemoveResource = void (*)(void* entry);

bool DoRemove(const std::string& vid, std::string& outResp) {
    
    {
        auto* rsm = ((FnGetRSM)(Base() + P().rva_rsmGetInstance))();
        if (rsm) {
            bool ok = false;
            void* tmpItem = BuildVideoItemFromJson(
                "{\"vid\":\"" + vid + "\",\"lang_name\":[],\"cover\":\"\",\"type\":\"videotypesilent\","
                "\"sharpness_videos\":[],\"author\":\"\",\"desc\":[],\"hd_cover\":\"\","
                "\"tags\":[],\"format\":\"static\",\"preview_videos\":[]}", ok);
            if (ok && tmpItem) {
                void* entry = ((FnFindByKey)(Base() + P().rva_rsmFindByKey))(
                    rsm, (uint8_t*)tmpItem + P().off_videoitem_vid, 0);
                if (entry) {
                    
                    ((FnRemoveResource)(Base() + P().rva_removeResource))(entry);
                    Log("[n0va] remove: 官方删除链已执行 vid=%s\n", vid.c_str());
                }
                ((FnDestroyVideoItem)(Base() + P().rva_destroyVideoItem))(tmpItem);
                operator delete(tmpItem);
            }
        }
    }
    
    {
        std::lock_guard<std::mutex> lk(g_storeMutex);
        auto it = g_store.find(vid);
        if (it != g_store.end()) {
            it->second.release();
            g_store.erase(it);
        }
    }
    outResp = "{\"ok\":true,\"data\":{\"removed\":true}}";
    return true;
}

















} 



namespace {

using FnB6D00 = void* (*)(void* self, void* out, void* vidQStr);
FnB6D00 g_trueB6D00 = nullptr;



static bool ReadVidSafe(void* vidQStr, char (&vid)[64]) {
    __try {
        void* d = *(void**)vidQStr;
        if (!d) return false;
        int size = *(int*)((uint8_t*)d + 4);
        if (size <= 0 || size >= 64) return false;
        const wchar_t* data = (const wchar_t*)((uint8_t*)d + 24);
        for (int i = 0; i < size; i++) vid[i] = (char)data[i];
        vid[size] = 0;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void* HookB6D00(void* self, void* out, void* vidQStr) {
    void* result = g_trueB6D00(self, out, vidQStr);
    char vid[64];
    if (ReadVidSafe(vidQStr, vid)) {
        std::lock_guard<std::mutex> lk(g_storeMutex);
        auto it = g_store.find(vid);
        if (it != g_store.end() && it->second.p) {
            ((FnCopyVideoItem)(Base() + P().rva_copyVideoItem))(out, it->second.p);
            Log("[n0va] subB6D00 hook: copied injected item vid=%s\n", vid);
        }
    }
    return result;
}

bool g_b6d00Installed = false;
} 

bool InstallSubB6D00HookIfNeeded() {
    
    
    
    
    
    const bool ENABLE_SUBB6D00_HOOK = true;
    if (!ENABLE_SUBB6D00_HOOK) {
        Log("[n0va] subB6D00 hook disabled (milestone 0 experiment mode)\n");
        return false;
    }
    g_trueB6D00 = (FnB6D00)(Base() + P().rva_hookTarget);
    LONG err = DetourTransactionBegin();
    if (err != NO_ERROR) return false;
    err = DetourUpdateThread(GetCurrentThread());
    if (err != NO_ERROR) { DetourTransactionAbort(); return false; }
    err = DetourAttach(reinterpret_cast<PVOID*>(&g_trueB6D00), HookB6D00);
    if (err != NO_ERROR) { DetourTransactionAbort(); return false; }
    err = DetourTransactionCommit();
    g_b6d00Installed = (err == NO_ERROR);
    return g_b6d00Installed;
}



void ExecuteInject(const std::string& requestJson, std::string& outResponse) {
    
    std::string opId;
    {
        size_t p = requestJson.find("\"op_id\"");
        if (p != std::string::npos) {
            p = requestJson.find('"', p + 7);
            if (p != std::string::npos) {
                size_t e = requestJson.find('"', p + 1);
                if (e != std::string::npos) opId = requestJson.substr(p + 1, e - p - 1);
            }
        }
    }
    if (!opId.empty()) {
        std::lock_guard<std::mutex> lk(g_storeMutex);
        auto it = g_opResults.find(opId);
        if (it != g_opResults.end()) {
            outResponse = it->second;
            return;
        }
    }
    
    
    std::string wallpaperJson = requestJson;
    {
        size_t p = requestJson.find("\"args\"");
        if (p != std::string::npos) {
            p = requestJson.find('{', p);
            if (p != std::string::npos) {
                
                int depth = 0;
                size_t end = std::string::npos;
                for (size_t i = p; i < requestJson.size(); i++) {
                    if (requestJson[i] == '{') depth++;
                    else if (requestJson[i] == '}') { depth--; if (depth == 0) { end = i; break; } }
                }
                if (end != std::string::npos)
                    wallpaperJson = requestJson.substr(p, end - p + 1);
            }
        }
    }
    Log("[n0va] extracted wallpaper json (%zu bytes)\n", wallpaperJson.size());
    DoInject(wallpaperJson, outResponse);
    if (!opId.empty()) {
        std::lock_guard<std::mutex> lk(g_storeMutex);
        g_opResults[opId] = outResponse;
        g_opOrder.push_back(opId);
        while (g_opOrder.size() > 64) {
            g_opResults.erase(g_opOrder.front());
            g_opOrder.erase(g_opOrder.begin());
        }
    }
}

void ExecuteRemove(const std::string& vid, std::string& outResponse) {
    DoRemove(vid, outResponse);
}







void N0vaPluginNotifyResponse(uint32_t id, const std::string& respJson);

namespace {

struct WorkerTask {
    bool isInject = true;
    std::string payload;   
    uint32_t reqId = 0;
};

std::mutex g_workerMutex;
std::condition_variable g_workerCv;
std::queue<WorkerTask> g_workerQueue;
bool g_workerStop = false;

void WorkerThreadMain() {
    Log("[n0va] worker thread started\n");
    for (;;) {
        WorkerTask t;
        {
            std::unique_lock<std::mutex> lk(g_workerMutex);
            g_workerCv.wait(lk, [] { return g_workerStop || !g_workerQueue.empty(); });
            if (g_workerStop) break;
            t = std::move(g_workerQueue.front());
            g_workerQueue.pop();
        }
        std::string resp;
        if (t.isInject) {
            Log("[n0va] worker executing inject...\n");
            ExecuteInject(t.payload, resp);
        } else {
            Log("[n0va] worker executing remove...\n");
            ExecuteRemove(t.payload, resp);
        }
        Log("[n0va] worker response: %s\n", resp.c_str());
        
        N0vaPluginNotifyResponse(t.reqId, resp);
    }
}

} 

void N0vaPluginQueueInjectWorker(const std::string& requestJson, uint32_t reqId) {
    {
        std::lock_guard<std::mutex> lk(g_workerMutex);
        WorkerTask t;
        t.isInject = true;
        t.payload = requestJson;
        t.reqId = reqId;
        g_workerQueue.push(std::move(t));
    }
    g_workerCv.notify_one();
}

void N0vaPluginQueueRemoveWorker(const std::string& vid, uint32_t reqId) {
    {
        std::lock_guard<std::mutex> lk(g_workerMutex);
        WorkerTask t;
        t.isInject = false;
        t.payload = vid;
        t.reqId = reqId;
        g_workerQueue.push(std::move(t));
    }
    g_workerCv.notify_one();
}


void N0vaPluginStartWorker() {
    std::thread(WorkerThreadMain).detach();
}


bool N0vaPluginListStoreVids(std::string& outVidsJson) {
    std::lock_guard<std::mutex> lk(g_storeMutex);
    std::string s = "[";
    bool first = true;
    for (auto& kv : g_store) {
        if (!first) s += ",";
        s += "\"" + kv.first + "\"";
        first = false;
    }
    s += "]";
    outVidsJson = s;
    return true;
}
