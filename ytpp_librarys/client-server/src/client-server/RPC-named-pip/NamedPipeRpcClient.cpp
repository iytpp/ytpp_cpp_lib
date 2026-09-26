#include "client-server/RPC-named-pip/NamedPipeRpcClient.h"
#include "client-server/RPC-named-pip/RpcSecurity.h"
#include <chrono>
#include <thread>
#include <windows.h>

namespace ytpp::client_server {
NamedPipeRpcClient::NamedPipeRpcClient(_In_ const std::wstring& pipeName, _In_ const std::string& sharedSecret,
                                       _In_ DWORD connectTimeoutMs, _In_ std::size_t minPoolSize,
                                       _In_ std::size_t maxPoolSize)
    : pipeName_(pipeName), sharedSecret_(sharedSecret), connectTimeoutMs_(connectTimeoutMs),
      minimumPoolSize_(minPoolSize), maximumPoolSize_(maxPoolSize) {
    if (minimumPoolSize_ > maximumPoolSize_)
        minimumPoolSize_ = maximumPoolSize_;

    for (std::size_t i = 0; i < minimumPoolSize_; ++i)
        pool_.push_back(std::make_shared<Connection>());
}

NamedPipeRpcClient::~NamedPipeRpcClient() {
    {
        std::unique_lock<std::mutex> lifetimeLock(lifetimeMutex_);
        shuttingDown_ = true;
        lifetimeCondition_.wait(lifetimeLock, [this] { return activeOperations_ == 0; });
    }
    std::lock_guard<std::mutex> lock(poolMutex_);
    for (auto& c : pool_)
        Close(*c);
    pool_.clear();
}

void NamedPipeRpcClient::SetTimeout(_In_ DWORD timeoutMs) {
    connectTimeoutMs_.store(timeoutMs);
}

bool NamedPipeRpcClient::Connect(_Inout_ Connection& conn, _Out_ std::string& errorMessage) {
    if (conn.pipe != INVALID_HANDLE_VALUE)
        return true;

    const DWORD timeoutMs = connectTimeoutMs_.load();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    for (;;) {
        const auto now = std::chrono::steady_clock::now();
        const auto remaining = now < deadline
                                   ? static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count())
                                   : 0;
        if (::WaitNamedPipeW(pipeName_.c_str(), remaining))
            break;

        const DWORD error = ::GetLastError();
        if (error != ERROR_FILE_NOT_FOUND && error != ERROR_SEM_TIMEOUT) {
            errorMessage = "WaitNamedPipe failed, error=" + std::to_string(error);
            return false;
        }
        if (remaining == 0) {
            errorMessage = error == ERROR_FILE_NOT_FOUND ? "Server not running." : "Connect timeout.";
            return false;
        }
        if (error == ERROR_FILE_NOT_FOUND)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    conn.pipe = ::CreateFileW(pipeName_.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);

    if (conn.pipe == INVALID_HANDLE_VALUE) {
        errorMessage = "::CreateFileW failed, error=" + std::to_string(::GetLastError());
        return false;
    }

    DWORD mode = PIPE_READMODE_BYTE;
    if (!::SetNamedPipeHandleState(conn.pipe, &mode, nullptr, nullptr)) {
        errorMessage = "::SetNamedPipeHandleState failed, error=" + std::to_string(::GetLastError());
        Close(conn);
        return false;
    }

    return true;
}

void NamedPipeRpcClient::Close(_Inout_ Connection& conn) {
    if (conn.pipe != INVALID_HANDLE_VALUE) {
        ::CloseHandle(conn.pipe);
        conn.pipe = INVALID_HANDLE_VALUE;
    }
}

std::shared_ptr<NamedPipeRpcClient::Connection> NamedPipeRpcClient::AcquireConnection(_Out_ std::string& errorMessage) {
    std::unique_lock<std::mutex> lock(poolMutex_);

    for (;;) {
        for (auto& conn : pool_) {
            if (!conn->busy) {
                conn->busy = true;
                return conn;
            }
        }

        if (pool_.size() < maximumPoolSize_) {
            auto conn = std::make_shared<Connection>();
            conn->busy = true;
            pool_.push_back(conn);
            return conn;
        }

        if (poolCondition_.wait_for(lock, std::chrono::milliseconds(connectTimeoutMs_.load())) == std::cv_status::timeout) {
            errorMessage = "Acquire connection from pool timeout.";
            return nullptr;
        }
    }
}

void NamedPipeRpcClient::ReleaseConnection(_In_ const std::shared_ptr<Connection>& conn) {
    {
        std::lock_guard<std::mutex> lock(poolMutex_);
        conn->busy = false;
    }
    poolCondition_.notify_one();
}

bool NamedPipeRpcClient::SendAndReceive(_Inout_ Connection& conn, _In_ const std::string& functionName,
                                        _In_ const RpcArray& args, _Out_ RpcResult& result,
                                        _Out_ std::string& errorMessage) {
    RpcRequest req;
    req.functionName = functionName;
    req.args = args;
    req.timestamp = GetCurrentUnixTimestamp();
    req.nonce = GenerateNonce();
    req.signature = RpcSecurity::MakeSignature(req, sharedSecret_);

    auto reqBytes = SerializeRequest(req);
    if (!WriteMessageToPipe(conn.pipe, reqBytes)) {
        errorMessage = "Write request failed.";
        return false;
    }

    std::vector<std::uint8_t> respBytes;
    if (!ReadMessageFromPipe(conn.pipe, respBytes)) {
        errorMessage = "Read response failed.";
        return false;
    }

    if (!DeserializeResult(respBytes, result)) {
        errorMessage = "Invalid response format.";
        return false;
    }

    return true;
}

bool NamedPipeRpcClient::Call(_In_ const std::string& functionName, _In_ const RpcArray& args, _Out_ RpcResult& result,
                              _Out_ std::string& errorMessage) {
    if (!BeginOperation()) {
        errorMessage = "RPC client is shutting down.";
        return false;
    }
    auto operationGuard = std::unique_ptr<void, std::function<void(void*)>>(reinterpret_cast<void*>(1),
                                                                            [this](void*) { EndOperation(); });
    return CallCore(functionName, args, result, errorMessage);
}

bool NamedPipeRpcClient::CallCore(_In_ const std::string& functionName, _In_ const RpcArray& args,
                                  _Out_ RpcResult& result, _Out_ std::string& errorMessage) {
    result = RpcResult{};
    errorMessage.clear();

    auto conn = AcquireConnection(errorMessage);
    if (!conn)
        return false;

    auto releaseGuard = std::unique_ptr<void, std::function<void(void*)>>(reinterpret_cast<void*>(1),
                                                                          [&](void*) { ReleaseConnection(conn); });

    if (!Connect(*conn, errorMessage))
        return false;

    const bool completed = SendAndReceive(*conn, functionName, args, result, errorMessage);
    if (!completed)
        Close(*conn);
    return completed;
}

std::future<RpcAsyncResult> NamedPipeRpcClient::CallAsync(_In_ const std::string& functionName,
                                                          _In_ const RpcArray& args) {
    if (!BeginOperation())
        throw std::runtime_error("RPC client is shutting down");
    try {
        return std::async(std::launch::async, [this, functionName, args]() -> RpcAsyncResult {
            auto operationGuard = std::unique_ptr<void, std::function<void(void*)>>(reinterpret_cast<void*>(1),
                                                                                    [this](void*) { EndOperation(); });
            RpcAsyncResult result;
            result.ok = CallCore(functionName, args, result.result, result.errorMessage);
            return result;
        });
    } catch (...) {
        EndOperation();
        throw;
    }
}

bool NamedPipeRpcClient::BeginOperation() {
    std::lock_guard<std::mutex> lock(lifetimeMutex_);
    if (shuttingDown_)
        return false;
    ++activeOperations_;
    return true;
}

void NamedPipeRpcClient::EndOperation() {
    std::lock_guard<std::mutex> lock(lifetimeMutex_);
    if (--activeOperations_ == 0)
        lifetimeCondition_.notify_all();
}
} // namespace ytpp::client_server
