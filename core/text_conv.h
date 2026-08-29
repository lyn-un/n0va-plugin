








#pragma once
#include <string>
#include <windows.h>

namespace n0va {


inline std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.c_str(),
                                (int)s.size(), nullptr, 0);
    if (n <= 0) return {};   
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}


inline std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                                nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}


inline std::wstring AnsiToWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_ACP, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (n <= 0) return {};
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}


inline std::string JsonUnescape(const std::string& s) {
    std::string r;
    r.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char c = s[++i];
            switch (c) {
                case '"':  r += '"';  break;
                case '\\': r += '\\'; break;
                case 'n':  r += '\n'; break;
                case 'r':  r += '\r'; break;
                case 't':  r += '\t'; break;
                case 'u': {
                    
                    unsigned v = 0;
                    bool ok = i + 4 < s.size();
                    for (int k = 1; ok && k <= 4; k++) {
                        char h = s[i + k];
                        v <<= 4;
                        if (h >= '0' && h <= '9') v |= h - '0';
                        else if (h >= 'a' && h <= 'f') v |= h - 'a' + 10;
                        else if (h >= 'A' && h <= 'F') v |= h - 'A' + 10;
                        else ok = false;
                    }
                    if (!ok) { r += c; break; }
                    i += 4;
                    
                    if (v < 0x80) {
                        r += (char)v;
                    } else if (v < 0x800) {
                        r += (char)(0xC0 | (v >> 6));
                        r += (char)(0x80 | (v & 0x3F));
                    } else {
                        r += (char)(0xE0 | (v >> 12));
                        r += (char)(0x80 | ((v >> 6) & 0x3F));
                        r += (char)(0x80 | (v & 0x3F));
                    }
                    break;
                }
                default: r += c;
            }
        } else r += s[i];
    }
    return r;
}

} 
