#pragma once

#include <string>

namespace n0va {


std::wstring GetPluginDir();


std::wstring LoadHostDir();


bool SetHostDir(const std::wstring& hostDir);


bool IsHostDirValid(const std::wstring& dir);

}
