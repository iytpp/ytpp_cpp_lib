#pragma once

#include "RpcCommon.h"

#include <condition_variable>
#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <sal.h>
#include <string>
#include <vector>

namespace ytpp::client_server {

/// @brief 异步RPC调用的结果包装。
struct RpcAsyncResult {
    bool ok = false;
    RpcResult result;
    std::string errorMessage;
};

/// @brief 使用可复用命名管道连接池调用RPC服务。
class NamedPipeRpcClient {
  public:
    /// @brief 创建RPC客户端并初始化连接池配置。
    /// @param[in] pipeName 完整命名管道名称。
    /// @param[in] sharedSecret 请求签名共享密钥。
    /// @param[in] connectTimeoutMs 连接超时毫秒数。
    /// @param[in] minimumPoolSize 初始连接数量。
    /// @param[in] maximumPoolSize 最大连接数量。
    NamedPipeRpcClient(_In_ const std::wstring& pipeName, _In_ const std::string& sharedSecret,
                       _In_ DWORD connectTimeoutMs = 2000, _In_ std::size_t minimumPoolSize = 2,
                       _In_ std::size_t maximumPoolSize = 16);

    /// @brief 关闭连接池中的全部命名管道句柄。
    ~NamedPipeRpcClient();

    /// @brief 设置连接和管道操作超时。
    /// @param[in] timeoutMs 超时毫秒数。
    void SetTimeout(_In_ DWORD timeoutMs);

    /// @brief 同步调用远端RPC函数。
    /// @param[in] functionName 已注册的远端函数名。
    /// @param[in] args RPC参数数组。
    /// @param[out] result 接收RPC结果。
    /// @param[out] errorMessage 接收本地通信错误说明。
    /// @return 请求和响应传输及解析成功时返回true；远端业务结果由result.success表示。
    bool Call(_In_ const std::string& functionName, _In_ const RpcArray& args, _Out_ RpcResult& result,
              _Out_ std::string& errorMessage);

    /// @brief 在线程池任务中异步调用远端RPC函数。
    /// @param[in] functionName 已注册的远端函数名。
    /// @param[in] args RPC参数数组。
    /// @return 可用于等待异步结果的future。
    std::future<RpcAsyncResult> CallAsync(_In_ const std::string& functionName, _In_ const RpcArray& args);

  private:
    struct Connection {
        HANDLE pipe = INVALID_HANDLE_VALUE;
        bool busy = false;
    };

    /// @brief 连接指定连接对象。@param[in,out] connection 连接对象。@param[out] errorMessage 错误说明。@return
    /// 成功返回true。
    bool Connect(_Inout_ Connection& connection, _Out_ std::string& errorMessage);
    /// @brief 关闭指定连接。@param[in,out] connection 连接对象。
    void Close(_Inout_ Connection& connection);
    /// @brief 从连接池取得可用连接。@param[out] errorMessage 错误说明。@return 可用连接，失败时为空。
    std::shared_ptr<Connection> AcquireConnection(_Out_ std::string& errorMessage);
    /// @brief 将连接归还连接池。@param[in] connection 要归还的连接。
    void ReleaseConnection(_In_ const std::shared_ptr<Connection>& connection);
    /// @brief 在已连接管道上完成一次请求和响应。@param[in,out] connection 连接。@param[in] functionName
    /// 函数名。@param[in] args 参数。@param[out] result RPC结果。@param[out] errorMessage 错误说明。@return
    /// 成功返回true。
    /// @param result 接收操作结果。
    bool SendAndReceive(_Inout_ Connection& connection, _In_ const std::string& functionName, _In_ const RpcArray& args,
                        _Out_ RpcResult& result, _Out_ std::string& errorMessage);
    /// @brief 在调用计数已经登记后执行一次同步RPC调用。
    /// @param[in] functionName 已注册的远端函数名。
    /// @param[in] args RPC参数数组。
    /// @param[out] result 接收远端业务结果。
    /// @param[out] errorMessage 接收本地传输或协议错误。
    /// @return 请求及响应传输和解析成功时返回true。
    bool CallCore(_In_ const std::string& functionName, _In_ const RpcArray& args, _Out_ RpcResult& result,
                  _Out_ std::string& errorMessage);
    /// @brief 登记一个活动调用，阻止析构期间启动新操作。
    /// @return 客户端尚未关闭且登记成功时返回true。
    bool BeginOperation();
    /// @brief 注销一个活动调用，并在最后一个调用结束时唤醒析构线程。
    void EndOperation();

    std::wstring pipeName_;
    std::string sharedSecret_;
    std::atomic<DWORD> connectTimeoutMs_{2000};
    std::size_t minimumPoolSize_ = 2;
    std::size_t maximumPoolSize_ = 16;
    std::vector<std::shared_ptr<Connection>> pool_;
    std::mutex poolMutex_;
    std::condition_variable poolCondition_;
    std::mutex lifetimeMutex_;
    std::condition_variable lifetimeCondition_;
    std::size_t activeOperations_ = 0;
    bool shuttingDown_ = false;
};

} // namespace ytpp::client_server
