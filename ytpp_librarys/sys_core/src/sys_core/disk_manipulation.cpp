#include "sys_core/disk_manipulation.h"
#include "sys_core/encoding.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace ytpp::sys_core::disk_manipulation {
std::wstring GetKnownFolderPathW(_In_ REFKNOWNFOLDERID folderId, _In_ bool trailingSlash) {
    PWSTR path = nullptr;

    const HRESULT hr = ::SHGetKnownFolderPath(folderId, KF_FLAG_DEFAULT, nullptr, &path);

    if (FAILED(hr) || path == nullptr)
        return {};

    std::wstring result(path);

    ::CoTaskMemFree(path);

    if (trailingSlash) {
        if (!result.empty() && result.back() != L'\\' && result.back() != L'/') {
            result.push_back(L'\\');
        }
    } else {
        while (result.size() > 1 && (result.back() == L'\\' || result.back() == L'/')) {
            result.pop_back();
        }
    }

    return result;
}

std::string GetKnownFolderPathUtf8(_In_ REFKNOWNFOLDERID folderId, _In_ bool trailingSlash) {
    const std::wstring widePath = GetKnownFolderPathW(folderId, trailingSlash);

    if (widePath.empty())
        return {};

    const int requiredSize = ::WideCharToMultiByte(CP_UTF8, 0, widePath.data(), static_cast<int>(widePath.size()),
                                                   nullptr, 0, nullptr, nullptr);

    if (requiredSize <= 0)
        return {};

    std::string result(static_cast<std::size_t>(requiredSize), '\0');

    ::WideCharToMultiByte(CP_UTF8, 0, widePath.data(), static_cast<int>(widePath.size()), result.data(), requiredSize,
                          nullptr, nullptr);

    return result;
}

std::filesystem::path GetExePath() {
    wchar_t buffer[MAX_PATH]{};

    DWORD length = ::GetModuleFileNameW(nullptr, buffer, MAX_PATH);

    if (length == 0)
        return {};

    return std::filesystem::path(buffer);
}

std::string GetExecutableDirectoryUtf8(_In_ bool withSlash) {
    std::vector<wchar_t> buffer(1024);

    DWORD len = ::GetModuleFileNameW(nullptr, buffer.data(), (DWORD)buffer.size());
    if (len == 0)
        return "";

    std::wstring fullPath(buffer.data(), len);

    std::size_t pos = fullPath.find_last_of(L"\\/");
    if (pos == std::wstring::npos)
        return "";

    std::wstring dir;
    if (withSlash)
        dir = fullPath.substr(0, pos + 1);
    else
        dir = fullPath.substr(0, pos);

    return encoding::WideToUtf8(dir);
}

std::string GetExecutableDirectoryA(_In_ bool withSlash) {
    char path[MAX_PATH] = {0};
    ::GetModuleFileNameA(NULL, path, MAX_PATH);

    std::string fullPath = path;

    std::size_t pos = fullPath.find_last_of("\\/");
    if (pos == std::string::npos)
        return "";

    if (withSlash)
        return fullPath.substr(0, pos + 1);
    else
        return fullPath.substr(0, pos);
}

std::wstring GetExecutableDirectoryW(_In_ bool withSlash) {
    std::vector<wchar_t> buffer(1024);
    DWORD len = ::GetModuleFileNameW(nullptr, buffer.data(), (DWORD)buffer.size());

    if (len == 0)
        return L"";

    std::wstring fullPath(buffer.data(), len);

    std::size_t pos = fullPath.find_last_of(L"\\/");
    if (pos == std::wstring::npos)
        return L"";

    if (withSlash)
        return fullPath.substr(0, pos + 1);
    else
        return fullPath.substr(0, pos);
}

bool FileExists(_In_ const std::string& fileName) {
    HANDLE hFind = 0;
    WIN32_FIND_DATAA wfd = {0};
    hFind = ::FindFirstFileA(fileName.c_str(), &wfd);
    if (hFind == INVALID_HANDLE_VALUE) {
        return false;
    }
    ::FindClose(hFind);
    return ((wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

bool FileExists(_In_ const std::wstring& fileName) {
    HANDLE hFind = 0;
    WIN32_FIND_DATAW wfd = {0};
    hFind = ::FindFirstFileW(fileName.c_str(), &wfd);
    if (hFind == INVALID_HANDLE_VALUE) {
        return false;
    }
    ::FindClose(hFind);
    return ((wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

bool WriteDataToFile(_In_ const std::string& fileName, _In_ const std::vector<char>& data) {
    std::ofstream out(fileName, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    if (!data.empty())
        out.write(data.data(), static_cast<std::streamsize>(data.size()));
    out.close();
    return !out.fail();
}

bool WriteDataToFile(_In_ const std::wstring& fileName, _In_ const std::vector<char>& data) {
    std::ofstream out(fileName, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    if (!data.empty())
        out.write(data.data(), static_cast<std::streamsize>(data.size()));
    out.close();
    return !out.fail();
}

bool WriteDataToFile(_In_ const std::string& fileName, _In_reads_bytes_opt_(size) const char* data,
                     _In_ std::size_t size) {
    if (data == nullptr && size != 0)
        return false;
    std::ofstream out(fileName, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    if (size != 0)
        out.write(data, static_cast<std::streamsize>(size));
    out.close();
    return !out.fail();
}

bool WriteDataToFile(_In_ const std::wstring& fileName, _In_reads_bytes_opt_(size) const char* data,
                     _In_ std::size_t size) {
    if (data == nullptr && size != 0)
        return false;
    std::ofstream out(fileName, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    if (size != 0)
        out.write(data, static_cast<std::streamsize>(size));
    out.close();
    return !out.fail();
}

bool WriteDataToFile(_In_ const std::string& fileName, _In_ const std::string& data) {
    std::ofstream out(fileName, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    if (!data.empty())
        out.write(data.data(), static_cast<std::streamsize>(data.size()));
    out.close();
    return !out.fail();
}

bool WriteDataToFile(_In_ const std::wstring& fileName, _In_ const std::string& data) {
    std::ofstream out(fileName, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    if (!data.empty())
        out.write(data.data(), static_cast<std::streamsize>(data.size()));
    out.close();
    return !out.fail();
}

bool WriteDataToFile(_In_ const std::string& fileName, _In_ const std::wstring& data) {
    std::ofstream out(fileName, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    const std::size_t bufferSize = data.size() * sizeof(wchar_t);
    if (bufferSize != 0)
        out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(bufferSize));
    out.close();
    return !out.fail();
}

bool WriteDataToFile(_In_ const std::wstring& fileName, _In_ const std::wstring& data) {
    std::ofstream out(fileName, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    const std::size_t bufferSize = data.size() * sizeof(wchar_t);
    if (bufferSize != 0)
        out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(bufferSize));
    out.close();
    return !out.fail();
}

bool WriteResourceToFileA(_In_ HMODULE hModule, _In_ LPCSTR resName, _In_ LPCSTR resType,
                          _In_ const std::string& outPath) {
    HRSRC hResource = ::FindResourceA(hModule, resName, resType);
    if (!hResource)
        return false;

    DWORD size = ::SizeofResource(hModule, hResource);
    if (!size)
        return false;

    HGLOBAL hGlobal = ::LoadResource(hModule, hResource);
    if (!hGlobal)
        return false;

    const void* data = ::LockResource(hGlobal);
    if (!data)
        return false;

    FILE* file = nullptr;
    if (fopen_s(&file, outPath.c_str(), "wb") != 0 || !file)
        return false;

    std::size_t written = std::fwrite(data, 1, size, file);
    std::fclose(file);
    return written == size;
}

bool WriteResourceToFileW(_In_ HMODULE hModule, _In_ LPCWSTR resName, _In_ LPCWSTR resType,
                          _In_ const std::wstring& outPath) {
    HRSRC hResource = ::FindResourceW(hModule, resName, resType);
    if (!hResource)
        return false;

    DWORD size = ::SizeofResource(hModule, hResource);
    if (!size)
        return false;

    HGLOBAL hGlobal = ::LoadResource(hModule, hResource);
    if (!hGlobal)
        return false;

    const void* data = ::LockResource(hGlobal);
    if (!data)
        return false;

    FILE* file = nullptr;
    if (_wfopen_s(&file, outPath.c_str(), L"wb") != 0 || !file)
        return false;

    std::size_t written = std::fwrite(data, 1, size, file);
    std::fclose(file);
    return written == size;
}

bool WriteProfileValueA(_In_ const std::string& fileName, _In_ const std::string& section, _In_ const std::string& key,
                        _In_ const std::string& value) {
    return ::WritePrivateProfileStringA(section.c_str(), key.c_str(), value.c_str(), fileName.c_str());
}

bool WriteProfileValueW(_In_ const std::wstring& fileName, _In_ const std::wstring& section,
                        _In_ const std::wstring& key, _In_ const std::wstring& value) {
    return ::WritePrivateProfileStringW(section.c_str(), key.c_str(), value.c_str(), fileName.c_str());
}

std::string ReadProfileValueA(_In_ const std::string& fileName, _In_ const std::string& section,
                              _In_ const std::string& key, _In_ const std::string& defaultValue,
                              _In_ DWORD defaultBufferSize /* = 256 */) {
    std::string rtn;
    DWORD currentBufferSize = defaultBufferSize;
    char* buffer = new char[currentBufferSize];
    // ZeroMemory( buffer, currentBufferSize );
    DWORD result = 0;
    do {
        if (result != 0) {                        // 说明已经取过一次了，但是缓冲区不够
            currentBufferSize += 256;             // 扩容
            delete[] buffer;                      // 释放旧的缓冲区
            buffer = new char[currentBufferSize]; // 重新分配扩容
            // ZeroMemory( buffer, currentBufferSize );
        }
        result = ::GetPrivateProfileStringA(section.c_str(), key.c_str(), defaultValue.c_str(), buffer,
                                            currentBufferSize, fileName.c_str());
    } while (result == currentBufferSize - 1 && buffer[result] == '\0');
    rtn.assign(buffer);
    delete[] buffer;
    return rtn;
}

std::wstring ReadProfileValueW(_In_ const std::wstring& fileName, _In_ const std::wstring& section,
                               _In_ const std::wstring& key, _In_ const std::wstring& defaultValue,
                               _In_ DWORD defaultBufferSize /* = 256 */) {
    std::wstring rtn;
    DWORD currentBufferSize = defaultBufferSize;
    wchar_t* buffer = new wchar_t[currentBufferSize];
    // ZeroMemory( buffer, currentBufferSize );
    DWORD result = 0;
    do {
        if (result != 0) {                           // 说明已经取过一次了，但是缓冲区不够
            currentBufferSize += 256;                // 扩容
            delete[] buffer;                         // 释放旧的缓冲区
            buffer = new wchar_t[currentBufferSize]; // 重新分配扩容
            // ZeroMemory( buffer, currentBufferSize );
        }
        result = ::GetPrivateProfileStringW(section.c_str(), key.c_str(), defaultValue.c_str(), buffer,
                                            currentBufferSize, fileName.c_str());
    } while (result == currentBufferSize - 1 && buffer[result] == '\0');
    rtn.assign(buffer);
    delete[] buffer;
    return rtn;
}

bool WriteProfileStructA(_In_ const std::string& fileName, _In_ const std::string& section, _In_ const std::string& key,
                         _In_reads_bytes_(structureSize) const void* structure, _In_ UINT structureSize) {
    return ::WritePrivateProfileStructA(section.c_str(), key.c_str(), const_cast<void*>(structure), structureSize,
                                        fileName.c_str());
}

bool WriteProfileStructW(_In_ const std::wstring& fileName, _In_ const std::wstring& section,
                         _In_ const std::wstring& key, _In_reads_bytes_(structureSize) const void* structure,
                         _In_ UINT structureSize) {
    return ::WritePrivateProfileStructW(section.c_str(), key.c_str(), const_cast<void*>(structure), structureSize,
                                        fileName.c_str());
}

bool ReadProfileStructA(_In_ const std::string& fileName, _In_ const std::string& section, _In_ const std::string& key,
                        _Out_writes_bytes_(structureSize) void* structure, _In_ UINT structureSize) {
    return ::GetPrivateProfileStructA(section.c_str(), key.c_str(), structure, structureSize, fileName.c_str());
}

bool ReadProfileStructW(_In_ const std::wstring& fileName, _In_ const std::wstring& section,
                        _In_ const std::wstring& key, _Out_writes_bytes_(structureSize) void* structure,
                        _In_ UINT structureSize) {
    return ::GetPrivateProfileStructW(section.c_str(), key.c_str(), structure, structureSize, fileName.c_str());
}

} // namespace ytpp::sys_core::disk_manipulation
