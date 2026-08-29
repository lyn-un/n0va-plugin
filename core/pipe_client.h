#pragma once


#include <cstdint>
#include <string>
#include <vector>

namespace n0va {

struct PipeResponse {
    bool ok = false;
    std::string error;
    std::string raw;   
    bool already_exists = false;
};



bool PipeRequest(const std::wstring& pipeName,
                 const std::string& requestJson,
                 PipeResponse& resp,
                 uint32_t connectTimeoutMs = 2000,
                 uint32_t responseTimeoutMs = 5000);


std::wstring MakePipeName();   


std::string GenerateOpId();

} 
