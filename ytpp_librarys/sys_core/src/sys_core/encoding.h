#pragma once

#include <sal.h>
#include <string>

namespace ytpp::sys_core::encoding {

/// @brief 将当前Windows ANSI代码页字符串转换为UTF-8。
/// @param[in] text ANSI编码文本。
/// @return UTF-8编码文本；输入为空时返回空字符串。
std::string AnsiToUtf8(_In_ const std::string& text);

/// @brief 将UTF-8字符串转换为当前Windows ANSI代码页字符串。
/// @param[in] text UTF-8编码文本。
/// @return ANSI编码文本；输入为空时返回空字符串。
std::string Utf8ToAnsi(_In_ const std::string& text);

/// @brief 将当前Windows ANSI代码页字符串转换为UTF-16宽字符串。
/// @param[in] text ANSI编码文本。
/// @return UTF-16宽字符串。
std::wstring AnsiToWide(_In_ const std::string& text);

/// @brief 将UTF-16宽字符串转换为当前Windows ANSI代码页字符串。
/// @param[in] text UTF-16宽字符串。
/// @return ANSI编码文本。
std::string WideToAnsi(_In_ const std::wstring& text);

/// @brief 将UTF-8字符串转换为UTF-16宽字符串。
/// @param[in] text UTF-8编码文本。
/// @return UTF-16宽字符串。
std::wstring Utf8ToWide(_In_ const std::string& text);

/// @brief 将UTF-16宽字符串转换为UTF-8字符串。
/// @param[in] text UTF-16宽字符串。
/// @return UTF-8编码文本。
std::string WideToUtf8(_In_ const std::wstring& text);

} // namespace ytpp::sys_core::encoding
