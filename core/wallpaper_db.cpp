

#include "wallpaper_db.h"

#include "text_conv.h"   

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <ctime>

namespace n0va {

std::string StateToString(WallpaperState s) {
    switch (s) {
        case WallpaperState::Pending:  return "pending";
        case WallpaperState::Active:   return "active";
        case WallpaperState::Deleting: return "deleting";
        case WallpaperState::Removed:  return "removed";
    }
    return "pending";
}

bool ParseState(const std::string& s, WallpaperState& out) {
    if (s == "pending")  { out = WallpaperState::Pending;  return true; }
    if (s == "active")   { out = WallpaperState::Active;   return true; }
    if (s == "deleting") { out = WallpaperState::Deleting; return true; }
    if (s == "removed")  { out = WallpaperState::Removed;  return true; }
    return false;
}


static std::string JsonEscape(const std::string& s) {
    std::string r;
    for (char c : s) {
        switch (c) {
            case '"':  r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\n': r += "\\n";  break;
            case '\r': r += "\\r";  break;
            case '\t': r += "\\t";  break;
            default:
                if ((unsigned char)c < 0x20) { char b[8]; snprintf(b, 8, "\\u%04x", c); r += b; }
                else r += c;
        }
    }
    return r;
}



bool WallpaperDb::Load(const std::wstring& path) {
    path_ = path;
    records_.clear();
    std::ifstream f(path, std::ios::binary);
    if (!f) return true;  
    std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    
    size_t arr = s.find('[');
    if (arr == std::string::npos) return false;
    size_t cur = arr + 1;
    while (cur < s.size()) {
        size_t ob = s.find('{', cur);
        if (ob == std::string::npos) break;
        size_t oe = s.find('}', ob);
        if (oe == std::string::npos) break;
        std::string obj = s.substr(ob, oe - ob + 1);
        WallpaperRecord r;
        auto grab = [&](const char* key) -> std::string {
            
            std::string pat = std::string("\"") + key + "\"";
            size_t p = obj.find(pat);
            if (p == std::string::npos) return {};
            p += pat.size();
            while (p < obj.size() && (obj[p] == ':' || obj[p] == ' ' || obj[p] == '\t')) p++;
            if (p >= obj.size() || obj[p] != '"') return {};
            p++;
            size_t e = obj.find('"', p);
            if (e == std::string::npos) return {};
            return n0va::JsonUnescape(obj.substr(p, e - p));
        };
        r.vid = grab("vid");
        if (r.vid.empty()) { cur = oe + 1; continue; }
        r.op_id = grab("op_id");
        std::string st = grab("state");
        if (!ParseState(st, r.state)) r.state = WallpaperState::Pending;
        r.name = grab("name");
        r.name_en = grab("name_en");
        r.tag_id = grab("tag_id");
        r.author = grab("author");
        r.format = grab("format");
        r.source_path = grab("source_path");
        r.source_sha256 = grab("source_sha256");
        r.target_file = grab("target_file");
        r.target_sha256 = grab("target_sha256");
        r.host_version = grab("host_version");
        r.updated_at = atoll(grab("updated_at").c_str());
        records_.push_back(std::move(r));
        cur = oe + 1;
    }
    return true;
}

bool WallpaperDb::Save() const {
    std::string s = "{\n  \"wallpapers\": [\n";
    for (size_t i = 0; i < records_.size(); i++) {
        const auto& r = records_[i];
        s += "    {\n";
        s += "      \"vid\":\"" + JsonEscape(r.vid) + "\",\n";
        s += "      \"op_id\":\"" + JsonEscape(r.op_id) + "\",\n";
        s += "      \"state\":\"" + StateToString(r.state) + "\",\n";
        s += "      \"name\":\"" + JsonEscape(r.name) + "\",\n";
        s += "      \"name_en\":\"" + JsonEscape(r.name_en) + "\",\n";
        s += "      \"tag_id\":\"" + JsonEscape(r.tag_id) + "\",\n";
        s += "      \"author\":\"" + JsonEscape(r.author) + "\",\n";
        s += "      \"format\":\"" + JsonEscape(r.format) + "\",\n";
        s += "      \"source_path\":\"" + JsonEscape(r.source_path) + "\",\n";
        s += "      \"source_sha256\":\"" + JsonEscape(r.source_sha256) + "\",\n";
        s += "      \"target_file\":\"" + JsonEscape(r.target_file) + "\",\n";
        s += "      \"target_sha256\":\"" + JsonEscape(r.target_sha256) + "\",\n";
        s += "      \"host_version\":\"" + JsonEscape(r.host_version) + "\",\n";
        s += "      \"updated_at\":" + std::to_string(r.updated_at) + "\n";
        s += i + 1 < records_.size() ? "    },\n" : "    }\n";
    }
    s += "  ]\n}\n";

    
    std::wstring tmp = path_ + L".tmp";
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    bool ok = WriteFile(h, s.data(), (DWORD)s.size(), &written, nullptr) &&
              written == s.size();
    FlushFileBuffers(h);
    CloseHandle(h);
    if (!ok) { DeleteFileW(tmp.c_str()); return false; }
    if (!MoveFileExW(tmp.c_str(), path_.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmp.c_str());
        return false;
    }
    return true;
}

WallpaperRecord* WallpaperDb::find(const std::string& vid) {
    for (auto& r : records_) if (r.vid == vid) return &r;
    return nullptr;
}
const WallpaperRecord* WallpaperDb::find(const std::string& vid) const {
    for (auto& r : records_) if (r.vid == vid) return &r;
    return nullptr;
}

void WallpaperDb::upsert(const WallpaperRecord& rec) {
    if (auto* r = find(rec.vid)) { *r = rec; return; }
    records_.push_back(rec);
}

bool WallpaperDb::remove(const std::string& vid) {
    for (auto it = records_.begin(); it != records_.end(); ++it) {
        if (it->vid == vid) { records_.erase(it); return true; }
    }
    return false;
}

} 
