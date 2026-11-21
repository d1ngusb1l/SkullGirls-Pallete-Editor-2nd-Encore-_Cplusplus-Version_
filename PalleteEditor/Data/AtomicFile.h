#pragma once
#include <Windows.h>
#include <filesystem>
#include <string>
#include <string_view>
#include <stdexcept>
#include <vector>

namespace PaletteData {
inline std::filesystem::path PathFromText(std::string_view text) {
    return std::filesystem::path(std::u8string((const char8_t*)text.data(),text.size()));
}
inline std::string PathText(const std::filesystem::path& path) {
    auto text=path.u8string(); return std::string((const char*)text.data(),text.size());
}
inline void AtomicWrite(const std::filesystem::path& path,const std::vector<unsigned char>& bytes) {
    auto destination=std::filesystem::absolute(path);
    auto temporary=destination;
    temporary+=L".tmp-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
    HANDLE file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) throw std::runtime_error("Could not create a temporary file beside the destination.");
    bool closed=false;
    try {
        DWORD written=0;
        if(bytes.size()>MAXDWORD || !WriteFile(file,bytes.data(),(DWORD)bytes.size(),&written,nullptr) || written!=bytes.size() || !FlushFileBuffers(file))
            throw std::runtime_error("Could not write the complete file.");
        CloseHandle(file); closed=true;
        if(!MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Could not replace the destination file. Its previous contents were preserved.");
    } catch(...) { if(!closed) CloseHandle(file); DeleteFileW(temporary.c_str()); throw; }
}
inline void AtomicWriteText(const std::filesystem::path& path,const std::string& text) {
    AtomicWrite(path,std::vector<unsigned char>(text.begin(),text.end()));
}
}
