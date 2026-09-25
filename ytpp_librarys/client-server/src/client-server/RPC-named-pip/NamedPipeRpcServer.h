#pragma once

#include "RpcCommon.h"
#include "RpcLogger.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <sal.h>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace ytpp::client_server {

/// @brief 定义允许连接命名管道的进程及其权限。
struct WhitelistRule {
    std::wstring processPath;
    PermissionLevel permission = PermissionLevel::Guest;
};

/// @brief 提供带签名、权限和进程白名单的命名管道RPC服务端。
class NamedPipeRpcServer {
  public:
    using RpcHandler = std::function<RpcResult(const RpcCallContext&, const RpcArray&)>;

    /// @brief 创建尚未启动的RPC服务端。
    /// @param[in] pipeName 完整命名管道名称。
    /// @param[in] sharedSecret 请求签名共享密钥。
    /// @param[in] workerCount 工作线程数量；0表示自动选择。
    /// @param pipeName 传递给 NamedPipeRpcServer 的 pipeName 参数。
    /// @param sharedSecret 传递给 NamedPipeRpcServer 的 sharedSecret 参数。
    /// @param workerCount 对应的数量或限制值。
    NamedPipeRpcServer(_In_ const std::wstring& pipeName, _In_ const std::string& sharedSecret,
                       _In_ std::size_t workerCount = 0);

    /// @brief 停止服务端并回收工作线程。
    ~NamedPipeRpcServer();

    /// @brief 启动命名管道工作线程。
    /// @return 启动成功时返回true。
    bool Start();

    /// @brief 停止服务端并等待全部工作线程退出。
    void Stop();

    /// @brief 注册可由客户端调用的RPC函数。
    /// @param[in] functionName 客户端请求使用的函数名称。
    /// @param[in] permission 调用该函数需要的最低权限。
    /// @param[in] handler 请求处理回调。
    /// @param functionName 传递给 RegisterFunction 的 functionName 参数。
    /// @param permission 传递给 RegisterFunction 的 permission 参数。
    /// @param handler 请求处理函数。
    void RegisterFunction(_In_ const std::string& functionName, _In_ PermissionLevel permission,
                          _In_ RpcHandler handler);

    /// @brief 添加允许访问管道的进程路径。
    /// @param[in] processPath 允许的可执行文件完整路径。
    /// @param[in] permission 为该进程授予的权限。
    /// @param processPath 文件或目录路径。
    /// @param permission 传递给 AddWhitelistProcess 的 permission 参数。
    void AddWhitelistProcess(_In_ const std::wstring& processPath, _In_ PermissionLevel permission);

    /// @brief 设置创建命名管道时使用的安全描述符。
    /// @param[in] sddl SDDL安全描述符字符串。
    /// @param sddl 传递给 SetPipeSecuritySddl 的 sddl 参数。
    void SetPipeSecuritySddl(_In_ const std::wstring& sddl);

    /// @brief 设置非拥有的日志记录器。
    /// @param[in] logger 日志记录器指针，可以为空；调用方必须保证其生命周期覆盖服务端运行期。
    /// @param logger 日志记录器；传空指针表示禁用。
    void SetLogger(_In_opt_ RpcLogger* logger);

  private:
    struct RegisteredFunction {
        PermissionLevel requiredPermission = PermissionLevel::Guest;
        RpcHandler handler;
    };

    /// @brief 创建一个等待客户端连接的命名管道实例。@return 成功时返回管道句柄。
    HANDLE CreatePipeInstance() const;
    /// @brief 工作线程循环，接受并处理客户端请求。
    void WorkerLoop();
    /// @brief 构造客户端调用上下文。@param[in] pipe 管道句柄。@param[out] context 接收上下文。@return 成功返回true。
    /// @param pipe 传递给 TryBuildClientContext 的 pipe 参数。
    /// @param context 传递给 TryBuildClientContext 的 context 参数。
    bool TryBuildClientContext(_In_ HANDLE pipe, _Out_ RpcCallContext& context);
    /// @brief 检查进程路径是否位于白名单。@param[in] processPath 进程路径。@param[out] permission 接收权限。@return
    /// 匹配时返回true。
    /// @param processPath 文件或目录路径。
    /// @param permission 传递给 CheckWhitelist 的 permission 参数。
    bool CheckWhitelist(_In_ const std::wstring& processPath, _Out_ PermissionLevel& permission) const;
    /// @brief 验证请求时间戳、Nonce和签名。@param[in] request 请求。@param[out] errorMessage 接收失败说明。@return
    /// 验证通过时返回true。
    /// @param request 请求对象。
    /// @param errorMessage 传递给 ValidateRequest 的 errorMessage 参数。
    bool ValidateRequest(_In_ const RpcRequest& request, _Out_ std::string& errorMessage);
    /// @brief 查找并调用RPC处理函数。@param[in] context 客户端上下文。@param[in] request 请求。@return RPC执行结果。
    /// @param context 传递给 ProcessRequest 的 context 参数。
    /// @param request 请求对象。
    RpcResult ProcessRequest(_In_ const RpcCallContext& context, _In_ const RpcRequest& request);
    /// @brief 删除已超过有效期的Nonce记录。
    void CleanupExpiredNonces();
    /// @brief 写入Info日志。@param[in] message 日志正文。
    /// @param message 传递给 LogInfo 的 message 参数。
    void LogInfo(_In_ const std::wstring& message);
    /// @brief 写入Warn日志。@param[in] message 日志正文。
    /// @param message 传递给 LogWarn 的 message 参数。
    void LogWarn(_In_ const std::wstring& message);
    /// @brief 写入Error日志。@param[in] message 日志正文。
    /// @param message 传递给 LogError 的 message 参数。
    void LogError(_In_ const std::wstring& message);

    std::wstring pipeName_;
    std::string sharedSecret_;
    std::size_t workerCount_ = 0;
    std::vector<std::thread> workers_;
    std::atomic<bool> running_{false};
    std::unordered_map<std::string, RegisteredFunction> functions_;
    mutable std::shared_mutex functionMutex_;
    std::vector<WhitelistRule> whitelist_;
    mutable std::shared_mutex whitelistMutex_;
    std::unordered_map<std::string, std::int64_t> usedNonces_;
    std::mutex nonceMutex_;
    std::wstring pipeSecuritySddl_;
    RpcLogger* logger_ = nullptr;
    std::int64_t allowedTimeSkewSeconds_ = 30;
    std::int64_t nonceExpireSeconds_ = 60;
};

} // namespace ytpp::client_server
