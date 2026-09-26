#pragma once
#include "RpcCommon.h"
#include <sal.h>
#include <string>

#pragma comment(lib, "bcrypt.lib")

namespace ytpp::client_server {

class RpcSecurity {
  public:
    /// @brief 计算文本的SHA-256十六进制摘要。
    /// @param[in] text 输入字节字符串。
    /// @return 小写十六进制摘要。
    static std::string Sha256Hex(_In_ const std::string& text);

    /// @brief 使用HMAC-SHA256和共享密钥为RPC请求的确定性二进制表示生成签名。
    /// @param[in] request 要签名的RPC请求。
    /// @param[in] secret 调用双方共享的密钥。
    /// @return 小写十六进制签名；密钥为空或系统密码学操作失败时返回空字符串。
    static std::string MakeSignature(_In_ const RpcRequest& request, _In_ const std::string& secret);

    /// @brief 验证RPC请求中携带的签名。
    /// @param[in] request 要验证的RPC请求。
    /// @param[in] secret 调用双方共享的密钥。
    /// @return 签名匹配时返回true。
    static bool VerifySignature(_In_ const RpcRequest& request, _In_ const std::string& secret);
};
} // namespace ytpp::client_server
