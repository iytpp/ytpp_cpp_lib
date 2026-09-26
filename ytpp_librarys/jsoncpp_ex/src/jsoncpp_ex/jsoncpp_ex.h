#pragma once

#include <json/json.h>
#include <sal.h>
#include <string>

namespace ytpp::json_ex {

/// @brief 将JSON值序列化为紧凑UTF-8文本。
/// @param[in] value 要序列化的JSON值。
/// @return UTF-8 JSON文本。
std::string Serialize(_In_ const Json::Value& value);

/// @brief 将JSON值序列化为UTF-8文本并控制是否格式化。
/// @param[in] value 要序列化的JSON值。
/// @param[in] formatted 为true时保留格式和注释，为false时输出紧凑文本。
/// @return UTF-8 JSON文本。
std::string Serialize(_In_ const Json::Value& value, _In_ bool formatted);

/// @brief 使用JsonCpp全部常用写入选项序列化JSON值。
/// @param[in] value 要序列化的JSON值。
/// @param[in] commentStyle 注释输出策略，例如`All`或`None`。
/// @param[in] indentation 每层缩进文本。
/// @param[in] enableYamlCompatibility 是否启用YAML兼容冒号空白。
/// @param[in] dropNullPlaceholders 是否省略空值占位文本。
/// @param[in] useSpecialFloats 是否输出NaN和Infinity。
/// @param[in] precision 浮点数精度。
/// @param[in] precisionType 精度解释方式，例如`significant`或`decimal`。
/// @param[in] emitUtf8 是否直接输出UTF-8字符。
/// @return 按给定选项生成的JSON文本。
std::string Serialize(_In_ const Json::Value& value, _In_ std::string commentStyle = "All",
                      _In_ std::string indentation = "\t", _In_ bool enableYamlCompatibility = false,
                      _In_ bool dropNullPlaceholders = false, _In_ bool useSpecialFloats = false,
                      _In_ int precision = 5, _In_ std::string precisionType = "significant",
                      _In_ bool emitUtf8 = false);

/// @brief 解析UTF-8 JSON文本并默认收集注释。
/// @param[in] jsonText 要解析的JSON文本。
/// @param[out] value 接收解析后的JSON值。
/// @return 解析成功时返回true。
bool Parse(_In_ const std::string& jsonText, _Out_ Json::Value& value);

/// @brief 解析UTF-8 JSON文本并控制是否收集注释。
/// @param[in] jsonText 要解析的JSON文本。
/// @param[out] value 接收解析后的JSON值。
/// @param[in] collectComments 是否收集JSON注释。
/// @return 解析成功时返回true。
bool Parse(_In_ const std::string& jsonText, _Out_ Json::Value& value, _In_ bool collectComments);

/// @brief 使用JsonCpp全部常用读取选项解析UTF-8 JSON文本。
/// @param[in] jsonText 要解析的JSON文本。
/// @param[out] value 接收解析后的JSON值。
/// @param[out] error 接收解析错误信息。
/// @param[in] collectComments 是否收集注释。
/// @param[in] allowComments 是否允许注释。
/// @param[in] allowTrailingCommas 是否允许尾随逗号。
/// @param[in] strictRoot 是否要求根节点为数组或对象。
/// @param[in] allowDroppedNullPlaceholders 是否允许省略空值占位。
/// @param[in] allowNumericKeys 是否允许数字键。
/// @param[in] allowSingleQuotes 是否允许单引号字符串。
/// @param[in] stackLimit 最大解析栈深度。
/// @param[in] failIfExtra 是否拒绝根值后的非空白内容。
/// @param[in] rejectDuplicateKeys 是否拒绝重复键。
/// @param[in] allowSpecialFloats 是否允许NaN和Infinity。
/// @param[in] skipBom 是否跳过UTF-8 BOM。
/// @return 解析成功时返回true。
bool Parse(_In_ const std::string& jsonText, _Out_ Json::Value& value, _Out_ Json::String& error,
           _In_ bool collectComments = true, _In_ bool allowComments = true, _In_ bool allowTrailingCommas = true,
           _In_ bool strictRoot = false, _In_ bool allowDroppedNullPlaceholders = true,
           _In_ bool allowNumericKeys = true, _In_ bool allowSingleQuotes = true, _In_ int stackLimit = 1024,
           _In_ bool failIfExtra = false, _In_ bool rejectDuplicateKeys = false, _In_ bool allowSpecialFloats = true,
           _In_ bool skipBom = true);

} // namespace ytpp::json_ex
