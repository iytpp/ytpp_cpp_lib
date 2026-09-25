#include "client-server/RPC-named-pip/NamedPipeRpcClient.h"
#include "client-server/RPC-named-pip/RpcSecurity.h"
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
    std::lock_guard<std::mutex> lock(poolMutex_);
    for (auto& c : pool_)
        Close(*c);
    pool_.clear();
}

void NamedPipeRpcClient::SetTimeout(_In_ DWORD timeoutMs) {
    connectTimeoutMs_ = timeoutMs;
}

bool NamedPipeRpcClient::Connect(_Inout_ Connection& conn, _Out_ std::string& errorMessage) {
    if (conn.pipe != INVALID_HANDLE_VALUE)
        return true;

    if (!::WaitNamedPipeW(pipeName_.c_str(), connectTimeoutMs_)) {
        DWORD err = ::GetLastError();
        if (err == ERROR_FILE_NOT_FOUND)
            errorMessage = "Server not running.";
        else if (err == ERROR_SEM_TIMEOUT)
            errorMessage = "Connect timeout.";
        else
            errorMessage = "WaitNamedPipe failed, error=" + std::to_string(err);
        return false;
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

        if (poolCondition_.wait_for(lock, std::chrono::milliseconds(connectTimeoutMs_)) == std::cv_status::timeout) {
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

    if (!result.success) {
        errorMessage = result.errorMessage;
        return false;
    }

    return true;
}

bool NamedPipeRpcClient::Call(_In_ const std::string& functionName, _In_ const RpcArray& args, _Out_ RpcResult& result,
                              _Out_ std::string& errorMessage) {
    result = RpcResult{};
    errorMessage.clear();

    auto conn = AcquireConnection(errorMessage);
    if (!conn)
        return false;

    auto releaseGuard = std::unique_ptr<void, std::function<void(void*)>>(reinterpret_cast<void*>(1),
                                                                          [&](void*) { ReleaseConnection(conn); });

    if (!Connect(*conn, errorMessage))
        return false;

    if (SendAndReceive(*conn, functionName, args, result, errorMessage))
        return true;

    DWORD lastErr = ::GetLastError();
    if (lastErr == ERROR_BROKEN_PIPE || lastErr == ERROR_NO_DATA) {
        Close(*conn);
        if (!Connect(*conn, errorMessage))
            return false;

        result = RpcResult{};
        errorMessage.clear();
        return SendAndReceive(*conn, functionName, args, result, errorMessage);
    }

    return false;
}

std::future<RpcAsyncResult> NamedPipeRpcClient::CallAsync(_In_ const std::string& functionName,
                                                          _In_ const RpcArray& args) {
    return std::async(std::launch::async, [this, functionName, args]() -> RpcAsyncResult {
        RpcAsyncResult ret;
        ret.ok = Call(functionName, args, ret.result, ret.errorMessage);
        return ret;
    });
}
} // namespace ytpp::client_server
