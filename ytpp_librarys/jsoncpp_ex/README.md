# jsoncpp_ex 使用手册

`jsoncpp_ex` 是对 JsonCpp 常用序列化与解析流程的轻量封装。它不替代 JsonCpp 数据模型；调用方仍使用 `Json::Value` 构造和访问 JSON，只把文本读写交给本库。

## 引入方式

```cmake
find_package(ytpp_cpp_lib CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE ytpp::jsoncpp_ex)
```

```cpp
#include <jsoncpp_ex/jsoncpp_ex.h>
```

全部接口位于 `ytpp::json_ex`。本文使用完整限定名以避免命名空间污染。

## 快速开始

```cpp
Json::Value root;
root["name"] = "ytpp";
root["enabled"] = true;

const std::string text = ytpp::json_ex::Serialize(root);

Json::Value parsed;
if (!ytpp::json_ex::Parse(text, parsed)) {
    // 处理解析失败。
}
```

## 序列化

### `Serialize(const Json::Value& value)`

将 `value` 序列化为紧凑 JSON 文本，适合网络传输、日志字段和配置持久化。

### `Serialize(const Json::Value& value, bool formatted)`

- `value`：要序列化的 JsonCpp 值。
- `formatted`：`true` 输出带缩进的可读文本；`false` 输出紧凑文本。
- 返回值：UTF-8 JSON 文本。

```cpp
const std::string pretty = ytpp::json_ex::Serialize(root, true);
const std::string compact = ytpp::json_ex::Serialize(root, false);
```

### 完整配置重载

```cpp
std::string Serialize(
    const Json::Value& value,
    std::string commentStyle = "All",
    std::string indentation = "\t",
    bool enableYamlCompatibility = false,
    bool dropNullPlaceholders = false,
    bool useSpecialFloats = false,
    int precision = 5,
    std::string precisionType = "significant",
    bool emitUtf8 = false);
```

| 参数 | 作用 |
|---|---|
| `value` | 要序列化的 JSON 值 |
| `commentStyle` | 注释策略，常用值为 `"All"` 或 `"None"` |
| `indentation` | 每层缩进文本；空字符串产生紧凑输出 |
| `enableYamlCompatibility` | 调整冒号周围空白以兼容 YAML 风格 |
| `dropNullPlaceholders` | 省略空值占位文本 |
| `useSpecialFloats` | 允许输出 `NaN`、`Infinity` 等特殊浮点值 |
| `precision` | 浮点数输出精度 |
| `precisionType` | `"significant"` 或 `"decimal"` |
| `emitUtf8` | 直接输出 UTF-8 字符，而不是 Unicode 转义 |

```cpp
const std::string text = ytpp::json_ex::Serialize(
    root, "None", "  ", false, false, false, 10, "significant", true);
```

上述字符串选项由 JsonCpp 解释，建议只使用文档列出的合法值。

## 解析

### `Parse(const std::string& jsonText, Json::Value& value)`

使用默认配置解析 JSON，并收集输入中的注释。`jsonText` 是 UTF-8 文本；`value` 在成功时接收解析结果；返回值表示是否成功。

### `Parse(..., bool collectComments)`

`collectComments` 控制是否把注释保存到 `Json::Value`，不等价于禁止输入中出现注释。

### 完整配置重载

```cpp
bool Parse(
    const std::string& jsonText,
    Json::Value& value,
    Json::String& error,
    bool collectComments = true,
    bool allowComments = true,
    bool allowTrailingCommas = true,
    bool strictRoot = false,
    bool allowDroppedNullPlaceholders = true,
    bool allowNumericKeys = true,
    bool allowSingleQuotes = true,
    int stackLimit = 1024,
    bool failIfExtra = false,
    bool rejectDuplicateKeys = false,
    bool allowSpecialFloats = true,
    bool skipBom = true);
```

| 参数 | 作用 |
|---|---|
| `jsonText` | 待解析的 UTF-8 文本 |
| `value` | 成功时接收根节点 |
| `error` | 失败时接收 JsonCpp 格式化错误信息 |
| `collectComments` | 将注释附加到解析结果 |
| `allowComments` | 允许 C/C++ 风格注释 |
| `allowTrailingCommas` | 允许对象或数组尾随逗号 |
| `strictRoot` | 要求根节点为对象或数组 |
| `allowDroppedNullPlaceholders` | 允许数组中省略的空值 |
| `allowNumericKeys` | 允许数字形式的对象键 |
| `allowSingleQuotes` | 允许单引号字符串 |
| `stackLimit` | 最大解析嵌套深度 |
| `failIfExtra` | 根值后存在非空白内容时失败 |
| `rejectDuplicateKeys` | 对象中出现重复键时失败 |
| `allowSpecialFloats` | 允许特殊浮点值 |
| `skipBom` | 跳过 UTF-8 BOM |

对外部或安全敏感输入建议显式使用严格配置：

```cpp
Json::Value value;
Json::String error;
const bool ok = ytpp::json_ex::Parse(
    input, value, error,
    false, false, false, true, false, false, false,
    256, true, true, false, true);
```

## 错误处理与建议

- 普通 `Parse` 只返回状态；需要诊断时使用带 `error` 的完整重载。
- 解析失败后不要读取 `value`，只在返回 `true` 时使用结果。
- 成功解析只说明语法有效，字段类型、必填字段和业务范围仍需调用方检查。
- 本库按 UTF-8 处理 JSON 文本，不负责 ANSI/UTF-16 转换，也不提供 JSON Schema 校验。
- JsonCpp 的宽松默认值适合受信配置；网络输入、权限配置和签名数据应收紧选项。

## 旧接口迁移

| 旧名称 | 当前名称 |
|---|---|
| `json_toString` / `json_toStringEx` | `Serialize` |
| `json_fromString` / `json_fromStringEx` | `Parse` |

