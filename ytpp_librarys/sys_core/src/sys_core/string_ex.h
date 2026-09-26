#pragma once

#include <cstddef>
#include <sal.h>
#include <string>
#include <vector>

namespace ytpp::sys_core::string_ex {

/// @brief 提取左右边界之间的文本。
/// @param[in] source 待搜索的源字符串。
/// @param[in] left 左边界文本。
/// @param[in] right 右边界文本。
/// @param[in] startPosition 搜索起始位置。
/// @param[out] endPosition 可选，接收右边界起始索引；未找到时为std::string::npos。
/// @return 找到时返回边界之间的文本，否则返回空字符串。
std::string ExtractBetween(_In_ const std::string& source, _In_ const std::string& left, _In_ const std::string& right,
                           _In_ std::size_t startPosition = 0, _Out_opt_ std::size_t* endPosition = nullptr);

/// @brief 使用单个字符分隔字符串。
/// @param[in] text 待分割文本。
/// @param[in] delimiter 分隔字符。
/// @return 按原顺序排列的分段列表。
std::vector<std::string> Split(_In_ const std::string& text, _In_ char delimiter);

/// @brief 使用字符串分隔符分割文本。
/// @param[in] text 待分割文本。
/// @param[in] delimiter 分隔文本。
/// @return 按原顺序排列的分段列表。
std::vector<std::string> Split(_In_ const std::string& text, _In_ const std::string& delimiter);

/// @brief 替换源字符串中的全部匹配子串。
/// @param[in] text 源字符串。
/// @param[in] oldSubstring 要替换的子串。
/// @param[in] newSubstring 替换文本。
/// @return 替换后的新字符串。
std::string ReplaceAll(_In_ const std::string& text, _In_ const std::string& oldSubstring,
                       _In_ const std::string& newSubstring);

/// @brief 去除字符串首尾空白字符。
/// @param[in] text 源字符串。
/// @return 去除首尾空白后的新字符串。
std::string Trim(_In_ const std::string& text);

/// @brief 按当前C区域设置将字符串转换为大写。
/// @param[in] text 源字符串。
/// @return 转换后的新字符串。
std::string ToUpper(_In_ const std::string& text);

/// @brief 按当前C区域设置将字符串转换为小写。
/// @param[in] text 源字符串。
/// @return 转换后的新字符串。
std::string ToLower(_In_ const std::string& text);

} // namespace ytpp::sys_core::string_ex
