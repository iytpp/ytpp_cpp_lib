#include "client-server/RPC-named-pip/RpcLogger.h"
#include <windows.h>

namespace ytpp::client_server {
RpcLogger::RpcLogger() {}

RpcLogger::~RpcLogger() {
    Close();
}

bool RpcLogger::Open(_In_ const std::wstring& filePath, _In_ LogLevel minLevel) {
    std::lock_guard<std::mutex> lock(mutex_);
    minLevel_ = minLevel;
    file_.open(filePath, std::ios::app);
    return file_.is_open();
}

void RpcLogger::Close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open())
        file_.close();
}

void RpcLogger::Debug(_In_ const std::wstring& msg) {
    Write(LogLevel::Debug, msg);
}

void RpcLogger::Info(_In_ const std::wstring& msg) {
    Write(LogLevel::Info, msg);
}

void RpcLogger::Warn(_In_ const std::wstring& msg) {
    Write(LogLevel::Warn, msg);
}

void RpcLogger::Error(_In_ const std::wstring& msg) {
    Write(LogLevel::Error, msg);
}

void RpcLogger::Write(_In_ LogLevel level, _In_ const std::wstring& msg) {
    if (level < minLevel_)
        return;

    std::lock_guard<std::mutex> lock(mutex_);
    if (!file_.is_open())
        return;

    file_ << L"[" << GetCurrentTimeText() << L"]" << L"[" << FormatLogLevel(level) << L"] " << msg << std::endl;
}

std::wstring RpcLogger::FormatLogLevel(_In_ LogLevel level) const {
    switch (level) {
    case LogLevel::Debug:
        return L"DEBUG";
    case LogLevel::Info:
        return L"INFO";
    case LogLevel::Warn:
        return L"WARN";
    case LogLevel::Error:
        return L"ERROR";
    default:
        return L"UNKNOWN";
    }
}

std::wstring RpcLogger::GetCurrentTimeText() const {
    SYSTEMTIME st{};
    ::GetLocalTime(&st);

    wchar_t buf[64] = {};
    ::swprintf_s(buf, L"%04u-%02u-%02u %02u:%02u:%02u.%03u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute,
                 st.wSecond, st.wMilliseconds);
    return buf;
}
} // namespace ytpp::client_server
