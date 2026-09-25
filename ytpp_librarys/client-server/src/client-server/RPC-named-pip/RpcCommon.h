#pragma once

#include <cstdint>
#include <map>
#include <sal.h>
#include <string>
#include <variant>
#include <vector>
#include <windows.h>

namespace ytpp::client_server {

/// @brief RPC动态值的数据类型。
enum class RpcValueType : std::uint8_t {
    Null,
    Bool,
    Int32,
    Int64,
    UInt32,
    UInt64,
    Double,
    String,
    WString,
    Binary,
    Array,
    Object
};

struct RpcValue;
using RpcArray = std::vector<RpcValue>;
using RpcObject = std::map<std::string, RpcValue>;
using RpcBinary = std::vector<std::uint8_t>;

/// @brief 保存RPC协议支持的标量、字符串、二进制、数组和对象值。
struct RpcValue {
    using VariantType = std::variant<std::monostate, bool, std::int32_t, std::int64_t, std::uint32_t, std::uint64_t,
                                     double, std::string, std::wstring, RpcBinary, RpcArray, RpcObject>;

    /// @brief 创建Null值。
    RpcValue();
    /// @brief 从nullptr创建Null值。@param[in] value 空指针标记。
    /// @param value 要读取、写入或处理的值。
    RpcValue(_In_ std::nullptr_t value);
    /// @brief 从布尔值创建。@param[in] value 输入值。
    /// @param value 要读取、写入或处理的值。
    RpcValue(_In_ bool value);
    /// @brief 从32位有符号整数创建。@param[in] value 输入值。
    /// @param value 要读取、写入或处理的值。
    RpcValue(_In_ std::int32_t value);
    /// @brief 从64位有符号整数创建。@param[in] value 输入值。
    /// @param value 要读取、写入或处理的值。
    RpcValue(_In_ std::int64_t value);
    /// @brief 从32位无符号整数创建。@param[in] value 输入值。
    /// @param value 要读取、写入或处理的值。
    RpcValue(_In_ std::uint32_t value);
    /// @brief 从64位无符号整数创建。@param[in] value 输入值。
    /// @param value 要读取、写入或处理的值。
    RpcValue(_In_ std::uint64_t value);
    /// @brief 从双精度浮点数创建。@param[in] value 输入值。
    /// @param value 要读取、写入或处理的值。
    RpcValue(_In_ double value);
    /// @brief 从窄字符串创建。@param[in] value 输入值。
    /// @param value 要读取、写入或处理的值。
    RpcValue(_In_ const std::string& value);
    /// @brief 从空字符结尾窄字符串创建。@param[in] value 输入字符串，可以为空指针。
    /// @param value 要读取、写入或处理的值。
    RpcValue(_In_opt_ const char* value);
    /// @brief 从宽字符串创建。@param[in] value 输入值。
    /// @param value 要读取、写入或处理的值。
    RpcValue(_In_ const std::wstring& value);
    /// @brief 从空字符结尾宽字符串创建。@param[in] value 输入字符串，可以为空指针。
    /// @param value 要读取、写入或处理的值。
    RpcValue(_In_opt_ const wchar_t* value);
    /// @brief 从二进制数据创建。@param[in] value 输入值。
    /// @param value 要读取、写入或处理的值。
    RpcValue(_In_ const RpcBinary& value);
    /// @brief 从数组创建。@param[in] value 输入值。
    /// @param value 要读取、写入或处理的值。
    RpcValue(_In_ const RpcArray& value);
    /// @brief 从对象创建。@param[in] value 输入值。
    /// @param value 要读取、写入或处理的值。
    RpcValue(_In_ const RpcObject& value);

    /// @brief 获取当前动态值类型。@return RPC值类型。
    [[nodiscard]] RpcValueType GetType() const;
    /// @brief 判断是否为Null。@return 匹配时返回true。
    [[nodiscard]] bool IsNull() const;
    /// @brief 判断是否为布尔值。@return 匹配时返回true。
    [[nodiscard]] bool IsBool() const;
    /// @brief 判断是否为32位有符号整数。@return 匹配时返回true。
    [[nodiscard]] bool IsInt32() const;
    /// @brief 判断是否为64位有符号整数。@return 匹配时返回true。
    [[nodiscard]] bool IsInt64() const;
    /// @brief 判断是否为32位无符号整数。@return 匹配时返回true。
    [[nodiscard]] bool IsUInt32() const;
    /// @brief 判断是否为64位无符号整数。@return 匹配时返回true。
    [[nodiscard]] bool IsUInt64() const;
    /// @brief 判断是否为双精度浮点数。@return 匹配时返回true。
    [[nodiscard]] bool IsDouble() const;
    /// @brief 判断是否为窄字符串。@return 匹配时返回true。
    [[nodiscard]] bool IsString() const;
    /// @brief 判断是否为宽字符串。@return 匹配时返回true。
    [[nodiscard]] bool IsWString() const;
    /// @brief 判断是否为二进制数据。@return 匹配时返回true。
    [[nodiscard]] bool IsBinary() const;
    /// @brief 判断是否为数组。@return 匹配时返回true。
    [[nodiscard]] bool IsArray() const;
    /// @brief 判断是否为对象。@return 匹配时返回true。
    [[nodiscard]] bool IsObject() const;

    /// @brief 读取布尔值。@return 保存的值；类型不匹配时抛出std::bad_variant_access。
    [[nodiscard]] bool AsBool() const;
    /// @brief 读取32位有符号整数。@return 保存的值。
    [[nodiscard]] std::int32_t AsInt32() const;
    /// @brief 读取64位有符号整数。@return 保存的值。
    [[nodiscard]] std::int64_t AsInt64() const;
    /// @brief 读取32位无符号整数。@return 保存的值。
    [[nodiscard]] std::uint32_t AsUInt32() const;
    /// @brief 读取64位无符号整数。@return 保存的值。
    [[nodiscard]] std::uint64_t AsUInt64() const;
    /// @brief 读取双精度浮点数。@return 保存的值。
    [[nodiscard]] double AsDouble() const;
    /// @brief 读取窄字符串。@return 保存值的只读引用。
    [[nodiscard]] const std::string& AsString() const;
    /// @brief 读取宽字符串。@return 保存值的只读引用。
    [[nodiscard]] const std::wstring& AsWString() const;
    /// @brief 读取二进制数据。@return 保存值的只读引用。
    [[nodiscard]] const RpcBinary& AsBinary() const;
    /// @brief 读取数组。@return 保存值的只读引用。
    [[nodiscard]] const RpcArray& AsArray() const;
    /// @brief 读取对象。@return 保存值的只读引用。
    [[nodiscard]] const RpcObject& AsObject() const;
    /// @brief 获取可写数组。@return 保存值的可写引用。
    RpcArray& AsArray();
    /// @brief 获取可写对象。@return 保存值的可写引用。
    RpcObject& AsObject();
    /// @brief 获取底层variant。@return 底层值的只读引用。
    [[nodiscard]] const VariantType& GetRaw() const;

  private:
    VariantType value_;
};

/// @brief RPC调用权限级别。
enum class PermissionLevel : std::uint32_t {
    Guest,
    User,
    Admin,
    Super
};

/// @brief RPC请求数据。
struct RpcRequest {
    std::string functionName;
    RpcArray args;
    std::int64_t timestamp = 0;
    std::string nonce;
    std::string signature;
};

/// @brief RPC响应数据。
struct RpcResult {
    bool success = false;
    RpcArray returnValues;
    std::string errorMessage;
};

/// @brief 服务端处理RPC时获得的客户端上下文。
struct RpcCallContext {
    std::uint32_t clientProcessId = 0;
    std::wstring clientProcessPath;
    PermissionLevel clientPermission = PermissionLevel::Guest;
};

/// @brief 按RPC协议格式构造字节缓冲区。
class ByteBufferWriter {
  public:
    /// @brief 写入8位无符号整数。@param[in] value 输入值。
    /// @param value 要读取、写入或处理的值。
    void WriteUInt8(_In_ std::uint8_t value);
    /// @brief 写入32位无符号整数。@param[in] value 输入值。
    /// @param value 要读取、写入或处理的值。
    void WriteUInt32(_In_ std::uint32_t value);
    /// @brief 写入64位无符号整数。@param[in] value 输入值。
    /// @param value 要读取、写入或处理的值。
    void WriteUInt64(_In_ std::uint64_t value);
    /// @brief 写入32位有符号整数。@param[in] value 输入值。
    /// @param value 要读取、写入或处理的值。
    void WriteInt32(_In_ std::int32_t value);
    /// @brief 写入64位有符号整数。@param[in] value 输入值。
    /// @param value 要读取、写入或处理的值。
    void WriteInt64(_In_ std::int64_t value);
    /// @brief 写入布尔值。@param[in] value 输入值。
    /// @param value 要读取、写入或处理的值。
    void WriteBool(_In_ bool value);
    /// @brief 写入双精度浮点数。@param[in] value 输入值。
    /// @param value 要读取、写入或处理的值。
    void WriteDouble(_In_ double value);
    /// @brief 写入窄字符串。@param[in] value 输入值。
    /// @param value 要读取、写入或处理的值。
    void WriteString(_In_ const std::string& value);
    /// @brief 写入宽字符串。@param[in] value 输入值。
    /// @param value 要读取、写入或处理的值。
    void WriteWString(_In_ const std::wstring& value);
    /// @brief 写入二进制数据。@param[in] value 输入值。
    /// @param value 要读取、写入或处理的值。
    void WriteBinary(_In_ const RpcBinary& value);
    /// @brief 写入动态RPC值。@param[in] value 输入值。
    /// @param value 要读取、写入或处理的值。
    void WriteRpcValue(_In_ const RpcValue& value);
    /// @brief 获取已写入的缓冲区。@return 字节缓冲区的只读引用。
    [[nodiscard]] const std::vector<std::uint8_t>& GetBuffer() const;

  private:
    std::vector<std::uint8_t> buffer_;
};

/// @brief 按RPC协议格式读取字节缓冲区。
class ByteBufferReader {
  public:
    /// @brief 从指定缓冲区创建读取器。@param[in] buffer 输入缓冲区，生命周期必须长于读取器。
    /// @param buffer 读写缓冲区。
    explicit ByteBufferReader(_In_ const std::vector<std::uint8_t>& buffer);
    /// @brief 读取8位无符号整数。@param[out] value 接收结果。@return 成功返回true。
    /// @param value 要读取、写入或处理的值。
    bool ReadUInt8(_Out_ std::uint8_t& value);
    /// @brief 读取32位无符号整数。@param[out] value 接收结果。@return 成功返回true。
    /// @param value 要读取、写入或处理的值。
    bool ReadUInt32(_Out_ std::uint32_t& value);
    /// @brief 读取64位无符号整数。@param[out] value 接收结果。@return 成功返回true。
    /// @param value 要读取、写入或处理的值。
    bool ReadUInt64(_Out_ std::uint64_t& value);
    /// @brief 读取32位有符号整数。@param[out] value 接收结果。@return 成功返回true。
    /// @param value 要读取、写入或处理的值。
    bool ReadInt32(_Out_ std::int32_t& value);
    /// @brief 读取64位有符号整数。@param[out] value 接收结果。@return 成功返回true。
    /// @param value 要读取、写入或处理的值。
    bool ReadInt64(_Out_ std::int64_t& value);
    /// @brief 读取布尔值。@param[out] value 接收结果。@return 成功返回true。
    /// @param value 要读取、写入或处理的值。
    bool ReadBool(_Out_ bool& value);
    /// @brief 读取双精度浮点数。@param[out] value 接收结果。@return 成功返回true。
    /// @param value 要读取、写入或处理的值。
    bool ReadDouble(_Out_ double& value);
    /// @brief 读取窄字符串。@param[out] value 接收结果。@return 成功返回true。
    /// @param value 要读取、写入或处理的值。
    bool ReadString(_Out_ std::string& value);
    /// @brief 读取宽字符串。@param[out] value 接收结果。@return 成功返回true。
    /// @param value 要读取、写入或处理的值。
    bool ReadWString(_Out_ std::wstring& value);
    /// @brief 读取二进制数据。@param[out] value 接收结果。@return 成功返回true。
    /// @param value 要读取、写入或处理的值。
    bool ReadBinary(_Out_ RpcBinary& value);
    /// @brief 读取动态RPC值。@param[out] value 接收结果。@return 成功返回true。
    /// @param value 要读取、写入或处理的值。
    bool ReadRpcValue(_Out_ RpcValue& value);

  private:
    const std::vector<std::uint8_t>& buffer_;
    std::size_t offset_ = 0;
};

/// @brief 序列化RPC请求。@param[in] request 请求对象。@return 协议字节流。
/// @param request 请求对象。
std::vector<std::uint8_t> SerializeRequest(_In_ const RpcRequest& request);
/// @brief 反序列化RPC请求。@param[in] data 协议字节流。@param[out] request 接收请求。@return 成功返回true。
/// @param data 输入数据。
/// @param request 请求对象。
bool DeserializeRequest(_In_ const std::vector<std::uint8_t>& data, _Out_ RpcRequest& request);
/// @brief 序列化RPC结果。@param[in] result 结果对象。@return 协议字节流。
/// @param result 接收操作结果。
std::vector<std::uint8_t> SerializeResult(_In_ const RpcResult& result);
/// @brief 反序列化RPC结果。@param[in] data 协议字节流。@param[out] result 接收结果。@return 成功返回true。
/// @param data 输入数据。
/// @param result 接收操作结果。
bool DeserializeResult(_In_ const std::vector<std::uint8_t>& data, _Out_ RpcResult& result);
/// @brief 构造请求签名使用的规范化文本。@param[in] request 请求对象。@return 规范化文本。
/// @param request 请求对象。
std::string BuildCanonicalRequestText(_In_ const RpcRequest& request);
/// @brief 向命名管道写入一条带长度前缀的消息。@param[in] pipe 管道句柄。@param[in] data 消息数据。@return
/// 成功返回true。
/// @param pipe 传递给 WriteMessageToPipe 的 pipe 参数。
/// @param data 输入数据。
bool WriteMessageToPipe(_In_ HANDLE pipe, _In_ const std::vector<std::uint8_t>& data);
/// @brief 从命名管道读取一条带长度前缀的消息。@param[in] pipe 管道句柄。@param[out] data 接收消息。@return
/// 成功返回true。
/// @param pipe 传递给 ReadMessageFromPipe 的 pipe 参数。
/// @param data 输入数据。
bool ReadMessageFromPipe(_In_ HANDLE pipe, _Out_ std::vector<std::uint8_t>& data);
/// @brief 将UTF-8转换为UTF-16。@param[in] text UTF-8文本。@return UTF-16文本。
/// @param text 待处理文本。
std::wstring Utf8ToWide(_In_ const std::string& text);
/// @brief 将UTF-16转换为UTF-8。@param[in] text UTF-16文本。@return UTF-8文本。
/// @param text 待处理文本。
std::string WideToUtf8(_In_ const std::wstring& text);
/// @brief 获取当前Unix时间戳。@return UTC秒级Unix时间戳。
std::int64_t GetCurrentUnixTimestamp();
/// @brief 生成RPC请求Nonce。@return 随机Nonce字符串。
std::string GenerateNonce();
/// @brief 将宽字符串转换为小写。@param[in] text 源文本。@return 小写文本。
/// @param text 待处理文本。
std::wstring ToLower(_In_ const std::wstring& text);

} // namespace ytpp::client_server
