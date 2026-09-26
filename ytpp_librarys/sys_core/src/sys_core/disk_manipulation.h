#pragma once

#include <ShlObj.h>
#include <filesystem>
#include <sal.h>
#include <string>
#include <vector>
#include <windows.h>

namespace ytpp::sys_core::disk_manipulation {

/// @brief 获取Windows已知文件夹的UTF-16路径。
/// @param[in] folderId `FOLDERID_*`已知文件夹标识。
/// @param[in] trailingSlash 是否在结果末尾添加反斜杠。
/// @return 文件夹路径；获取失败时返回空字符串。
std::wstring GetKnownFolderPathW(_In_ REFKNOWNFOLDERID folderId, _In_ bool trailingSlash = true);

/// @brief 获取Windows已知文件夹的UTF-8路径。
/// @param[in] folderId `FOLDERID_*`已知文件夹标识。
/// @param[in] trailingSlash 是否在结果末尾添加反斜杠。
/// @return UTF-8文件夹路径；获取失败时返回空字符串。
std::string GetKnownFolderPathUtf8(_In_ REFKNOWNFOLDERID folderId, _In_ bool trailingSlash = true);

/// @brief 获取当前可执行文件的完整路径。
/// @return 当前可执行文件路径，可通过`parent_path()`获取运行目录。
std::filesystem::path GetExePath();

/// @brief 获取当前可执行文件所在目录的UTF-8路径。
/// @param[in] withSlash 是否在结果末尾保留反斜杠。
/// @return UTF-8目录路径。
[[deprecated("Use GetExePath() instead")]]
std::string GetExecutableDirectoryUtf8(_In_ bool withSlash = true);

/// @brief 获取当前可执行文件所在目录的ANSI路径。
/// @param[in] withSlash 是否在结果末尾保留反斜杠。
/// @return 当前ANSI代码页目录路径。
[[deprecated("Use GetExePath() instead")]]
std::string GetExecutableDirectoryA(_In_ bool withSlash = true);

/// @brief 获取当前可执行文件所在目录的UTF-16路径。
/// @param[in] withSlash 是否在结果末尾保留反斜杠。
/// @return UTF-16目录路径。
[[deprecated("Use GetExePath() instead")]]
std::wstring GetExecutableDirectoryW(_In_ bool withSlash = true);

/// @brief 判断ANSI或UTF-8文件路径是否存在。
/// @param[in] fileName 要检查的文件路径。
/// @return 文件存在时返回true。
bool FileExists(_In_ const std::string& fileName);

/// @brief 判断UTF-16文件路径是否存在。
/// @param[in] fileName 要检查的文件路径。
/// @return 文件存在时返回true。
bool FileExists(_In_ const std::wstring& fileName);

/// @brief 将字节数组覆盖写入ANSI或UTF-8路径文件。
/// @param[in] fileName 目标文件路径。
/// @param[in] data 要写入的数据。
/// @return 写入成功时返回true。
bool WriteDataToFile(_In_ const std::string& fileName, _In_ const std::vector<char>& data);

/// @brief 将字节数组覆盖写入UTF-16路径文件。
/// @param[in] fileName 目标文件路径。
/// @param[in] data 要写入的数据。
/// @return 写入成功时返回true。
bool WriteDataToFile(_In_ const std::wstring& fileName, _In_ const std::vector<char>& data);

/// @brief 将原始字节覆盖写入ANSI或UTF-8路径文件。
/// @param[in] fileName 目标文件路径。
/// @param[in] data 数据缓冲区；size为0时可以为空。
/// @param[in] size 写入字节数。
/// @return 写入成功时返回true。
bool WriteDataToFile(_In_ const std::string& fileName, _In_reads_bytes_opt_(size) const char* data,
                     _In_ std::size_t size);

/// @brief 将原始字节覆盖写入UTF-16路径文件。
/// @param[in] fileName 目标文件路径。
/// @param[in] data 数据缓冲区；size为0时可以为空。
/// @param[in] size 写入字节数。
/// @return 写入成功时返回true。
bool WriteDataToFile(_In_ const std::wstring& fileName, _In_reads_bytes_opt_(size) const char* data,
                     _In_ std::size_t size);

/// @brief 将窄字符串覆盖写入ANSI或UTF-8路径文件。
/// @param[in] fileName 目标文件路径。
/// @param[in] data 要写入的窄字符串字节。
/// @return 写入成功时返回true。
bool WriteDataToFile(_In_ const std::string& fileName, _In_ const std::string& data);

/// @brief 将窄字符串覆盖写入UTF-16路径文件。
/// @param[in] fileName 目标文件路径。
/// @param[in] data 要写入的窄字符串字节。
/// @return 写入成功时返回true。
bool WriteDataToFile(_In_ const std::wstring& fileName, _In_ const std::string& data);

/// @brief 将宽字符串覆盖写入ANSI或UTF-8路径文件。
/// @param[in] fileName 目标文件路径。
/// @param[in] data 要写入的UTF-16字符串。
/// @return 写入成功时返回true。
bool WriteDataToFile(_In_ const std::string& fileName, _In_ const std::wstring& data);

/// @brief 将宽字符串覆盖写入UTF-16路径文件。
/// @param[in] fileName 目标文件路径。
/// @param[in] data 要写入的UTF-16字符串。
/// @return 写入成功时返回true。
bool WriteDataToFile(_In_ const std::wstring& fileName, _In_ const std::wstring& data);

/// @brief 从模块提取窄字符名称资源并写入文件。
/// @param[in] module 包含资源的模块句柄。
/// @param[in] resourceName 资源名称或整数资源标识。
/// @param[in] resourceType 资源类型。
/// @param[in] outputPath 输出文件路径。
/// @return 提取并写入成功时返回true。
bool WriteResourceToFileA(_In_ HMODULE module, _In_ LPCSTR resourceName, _In_ LPCSTR resourceType,
                          _In_ const std::string& outputPath);

/// @brief 从模块提取宽字符名称资源并写入文件。
/// @param[in] module 包含资源的模块句柄。
/// @param[in] resourceName 资源名称或整数资源标识。
/// @param[in] resourceType 资源类型。
/// @param[in] outputPath 输出文件路径。
/// @return 提取并写入成功时返回true。
bool WriteResourceToFileW(_In_ HMODULE module, _In_ LPCWSTR resourceName, _In_ LPCWSTR resourceType,
                          _In_ const std::wstring& outputPath);

/// @brief 写入ANSI或UTF-8路径INI配置项。
/// @param[in] fileName INI文件路径。
/// @param[in] section 配置节名称。
/// @param[in] key 配置项名称。
/// @param[in] value 配置项值。
/// @return 写入成功时返回true。
bool WriteProfileValueA(_In_ const std::string& fileName, _In_ const std::string& section, _In_ const std::string& key,
                        _In_ const std::string& value);

/// @brief 写入UTF-16路径INI配置项。
/// @param[in] fileName INI文件路径。
/// @param[in] section 配置节名称。
/// @param[in] key 配置项名称。
/// @param[in] value 配置项值。
/// @return 写入成功时返回true。
bool WriteProfileValueW(_In_ const std::wstring& fileName, _In_ const std::wstring& section,
                        _In_ const std::wstring& key, _In_ const std::wstring& value);

/// @brief 读取ANSI或UTF-8路径INI配置项。
/// @param[in] fileName INI文件路径。
/// @param[in] section 配置节名称。
/// @param[in] key 配置项名称。
/// @param[in] defaultValue 配置项不存在时使用的默认值。
/// @param[in] initialBufferSize 初始读取缓冲区大小。
/// @return 配置项值。
std::string ReadProfileValueA(_In_ const std::string& fileName, _In_ const std::string& section,
                              _In_ const std::string& key, _In_ const std::string& defaultValue,
                              _In_ DWORD initialBufferSize = 256);

/// @brief 读取UTF-16路径INI配置项。
/// @param[in] fileName INI文件路径。
/// @param[in] section 配置节名称。
/// @param[in] key 配置项名称。
/// @param[in] defaultValue 配置项不存在时使用的默认值。
/// @param[in] initialBufferSize 初始读取缓冲区大小。
/// @return 配置项值。
std::wstring ReadProfileValueW(_In_ const std::wstring& fileName, _In_ const std::wstring& section,
                               _In_ const std::wstring& key, _In_ const std::wstring& defaultValue,
                               _In_ DWORD initialBufferSize = 256);

/// @brief 将结构体数据写入ANSI或UTF-8路径INI文件。
/// @param[in] fileName INI文件路径。
/// @param[in] section 配置节名称。
/// @param[in] key 配置项名称。
/// @param[in] structure 结构体数据地址。
/// @param[in] structureSize 结构体字节数。
/// @return 写入成功时返回true。
bool WriteProfileStructA(_In_ const std::string& fileName, _In_ const std::string& section, _In_ const std::string& key,
                         _In_reads_bytes_(structureSize) const void* structure, _In_ UINT structureSize);

/// @brief 将结构体数据写入UTF-16路径INI文件。
/// @param[in] fileName INI文件路径。
/// @param[in] section 配置节名称。
/// @param[in] key 配置项名称。
/// @param[in] structure 结构体数据地址。
/// @param[in] structureSize 结构体字节数。
/// @return 写入成功时返回true。
bool WriteProfileStructW(_In_ const std::wstring& fileName, _In_ const std::wstring& section,
                         _In_ const std::wstring& key, _In_reads_bytes_(structureSize) const void* structure,
                         _In_ UINT structureSize);

/// @brief 从ANSI或UTF-8路径INI文件读取结构体数据。
/// @param[in] fileName INI文件路径。
/// @param[in] section 配置节名称。
/// @param[in] key 配置项名称。
/// @param[out] structure 接收结构体数据的缓冲区。
/// @param[in] structureSize 缓冲区字节数。
/// @return 读取成功时返回true。
bool ReadProfileStructA(_In_ const std::string& fileName, _In_ const std::string& section, _In_ const std::string& key,
                        _Out_writes_bytes_(structureSize) void* structure, _In_ UINT structureSize);

/// @brief 从UTF-16路径INI文件读取结构体数据。
/// @param[in] fileName INI文件路径。
/// @param[in] section 配置节名称。
/// @param[in] key 配置项名称。
/// @param[out] structure 接收结构体数据的缓冲区。
/// @param[in] structureSize 缓冲区字节数。
/// @return 读取成功时返回true。
bool ReadProfileStructW(_In_ const std::wstring& fileName, _In_ const std::wstring& section,
                        _In_ const std::wstring& key, _Out_writes_bytes_(structureSize) void* structure,
                        _In_ UINT structureSize);

} // namespace ytpp::sys_core::disk_manipulation
