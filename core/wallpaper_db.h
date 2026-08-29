#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace n0va {

enum class WallpaperState {
    Pending,    
    Active,     
    Deleting,   
    Removed,    
};

struct WallpaperRecord {
    std::string vid;              
    std::string op_id;            
    WallpaperState state = WallpaperState::Pending;
    std::string name;             
    std::string name_en;
    std::string tag_id;           
    std::string author;
    std::string format;           
    std::string source_path;
    std::string source_sha256;
    std::string target_file;      
    std::string target_sha256;
    std::string host_version;
    int64_t     updated_at = 0;
};

class WallpaperDb {
public:
    bool Load(const std::wstring& path);
    bool Save() const;                     
    const std::wstring& path() const { return path_; }

    std::vector<WallpaperRecord>& all() { return records_; }
    const std::vector<WallpaperRecord>& all() const { return records_; }

    WallpaperRecord* find(const std::string& vid);
    const WallpaperRecord* find(const std::string& vid) const;

    void upsert(const WallpaperRecord& rec);
    bool remove(const std::string& vid);

private:
    std::wstring path_;
    std::vector<WallpaperRecord> records_;
};

std::string StateToString(WallpaperState s);
bool ParseState(const std::string& s, WallpaperState& out);

} 
