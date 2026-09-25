#pragma once

#include <fstream>
#include <mutex>
#include <sal.h>
#include <string>

namespace ytpp::client_server {

/// @brief RPC日志消息级别。
enum class LogLevel {
    Debug,
    Info,
    Warn,
    Error
};

/// @brief 提供线程安全的UTF-16文件日志输出。
class RpcLogger {
  public:
    /// @brief 创建尚未打开日志文件的记录器。
    RpcLogger();

    /// @brief 关闭日志文件并释放资源。
    ~RpcLogger();

    /// @brief 打开或覆盖日志文件。
    /// @param[in] filePath 日志文件路径。
    /// @param[in] minimumLevel 最低记录级别。
    /// @return 文件成功打开时返回true。
    /// @param filePath 文件路径。
    /// @param minimumLevel 传递给 Open 的 minimumLevel 参数。
    bool Open(_In_ const std::wstring& filePath, _In_ LogLevel minimumLevel = LogLevel::Debug);

    /// @brief 关闭当前日志文件。
    void Close();

    /// @brief 写入Debug级别消息。
    /// @param[in] message 日志正文。
    /// @param message 传递给 Debug 的 message 参数。
    void Debug(_In_ const std::wstring& message);

    /// @brief 写入Info级别消息。
    /// @param[in] message 日志正文。
    /// @param message 传递给 Info 的 message 参数。
    void Info(_In_ const std::wstring& message);

    /// @brief 写入Warn级别消息。
    /// @param[in] message 日志正文。
    /// @param message 传递给 Warn 的 message 参数。
    void Warn(_In_ const std::wstring& message);

    /// @brief 写入Error级别消息。
    /// @param[in] message 日志正文。
    /// @param message 传递给 Error 的 message 参数。
    void Error(_In_ const std::wstring& message);

    /// @brief 按指定级别写入一条日志。
    /// @param[in] level 消息级别。
    /// @param[in] message 日志正文。
    /// @param level 传递给 Write 的 level 参数。
    /// @param message 传递给 Write 的 message 参数。
    void Write(_In_ LogLevel level, _In_ const std::wstring& message);

  private:
    /// @brief 将日志级别格式化为文本。
    /// @param[in] level 日志级别。
    /// @return 级别名称。
    /// @param level 传递给 FormatLogLevel 的 level 参数。
    std::wstring FormatLogLevel(_In_ LogLevel level) const;

    /// @brief 获取用于日志前缀的当前本地时间文本。
    /// @return 当前时间文本。
    std::wstring GetCurrentTimeText() const;

    std::wofstream file_;
    std::mutex mutex_;
    LogLevel minLevel_ = LogLevel::Debug;
};

} // namespace ytpp::client_server
