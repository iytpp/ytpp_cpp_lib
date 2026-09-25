#pragma once

#include <cstddef>
#include <cstdint>
#include <sal.h>
#include <string>
#include <vector>

namespace ytpp::sys_core::hash {

/// @brief 指定摘要算法。
enum class HashType {
    Md5,
    Sha1,
    Sha256,
    Sha512
};

/// @brief 计算字符串的十六进制摘要。
/// @param[in] input 要计算摘要的字节字符串。
/// @param[in] hashType 摘要算法。
/// @return 小写十六进制摘要；OpenSSL操作失败时返回空字符串。
/// @param input 输入数据。
/// @param hashType 控制对应功能是否启用。
std::string ComputeHash(_In_ const std::string& input, _In_ HashType hashType = HashType::Sha256);

/// @brief 生成非密码学用途的伪随机字节。
/// @param[in] dataLength 要生成的字节数。
/// @return 包含指定字节数的伪随机数据。
/// @warning 不得用于密钥、令牌、Nonce或其他密码学场景。
/// @param dataLength 对应的数量或限制值。
std::vector<std::uint8_t> GeneratePseudoRandomBytes(_In_ std::size_t dataLength);

/// @brief 将非密码学伪随机字节写入调用方缓冲区。
/// @param[out] buffer 接收随机字节的缓冲区。
/// @param[in] dataLength 缓冲区大小和生成字节数。
/// @warning 不得用于密钥、令牌、Nonce或其他密码学场景。
/// @param dataLength 对应的数量或限制值。
void GeneratePseudoRandomBytes(_Out_writes_bytes_(dataLength) void* buffer, _In_ std::size_t dataLength);

} // namespace ytpp::sys_core::hash
