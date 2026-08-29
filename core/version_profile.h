#pragma once


#include <cstdint>

namespace n0va {

struct VersionProfile {
    
    const char* host_version = "2.2.1.4";
    
    const char* exe_sha256_hex = "38632e96b3ffdf8bc5065a9d45c0b4adbe633ba8c4ffd3150734ca0b2e8dce19";
    uint64_t    exe_size = 14429184;
    
    const char* exe_md5_hex = "8c1b812aa5180eceabcf679141d89af5";

    
    const char* qtcore_version_min = "5";

    
    uint16_t machine = 0x8664;            
    uint32_t protocol_min_version = 1;

    
    uint32_t rva_jsonToVideoItem = 0x8F9B0;  
    uint32_t rva_getModelManager = 0x95EB0;  
    uint32_t rva_addDownloaded  = 0x951E0;   
    uint32_t rva_getVDC         = 0xAC820;   
    uint32_t rva_syncDl         = 0xAFFB0;   
    uint32_t rva_rsmInsert      = 0xC97D0;   
    uint32_t rva_rsmFill        = 0xCD4B0;   
    uint32_t rva_rsmFindByKey   = 0xC76A0;   
    uint32_t rva_rsmGetInstance = 0xC4CF0;   
    uint32_t rva_sig12          = 0x1B8F50;  
    uint32_t rva_copyVideoItem  = 0x899F0;
    uint32_t rva_destroyVideoItem = 0x89EA0;
    uint32_t rva_removeResource = 0xCA660;


    uint32_t rva_hookTarget = 0xB6D00;       
    
    uint8_t hook_expected_bytes[16] = {
        0x48, 0x8B, 0xC4, 0x55, 0x41, 0x56, 0x41, 0x57,
        0x48, 0x8D, 0x68, 0xB8, 0x48, 0x81, 0xEC, 0x30};

    
    uint32_t sizeof_video_item  = 0x68;  
    uint32_t alignof_video_item = 8;
    uint32_t off_rsm_status     = 0x94;  
    uint32_t off_rsm_isCurrent  = 0x60;  
    uint32_t off_videoitem_vid  = 0x08;  
    uint32_t off_videoitem_tags = 0x48;  

    
    int injected_success_status = 3;      
};


inline const VersionProfile& currentProfile() {
    static const VersionProfile p;
    return p;
}

} 
