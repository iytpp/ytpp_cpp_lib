#include "client-server/RPC-named-pip/NamedPipeRpcServer.h"
#include "client-server/RPC-named-pip/RpcSecurity.h"
#include <sddl.h>
#include <vector>
#include <windows.h>

namespace ytpp::client_server {
NamedPipeRpcServer::NamedPipeRpcServer(_In_ const std::wstring& pipeName, _In_ const std::string& sharedSecret,
                                       _In_ std::size_t workerCount)
    : pipeName_(pipeName), sharedSecret_(sharedSecret) {
    if (workerCount == 0) {
        std::size_t hc = std::thread::hardware_concurrency();
        workerCount_ = (hc == 0 ? 8 : hc * 2);
    } else {
        workerCount_ = workerCount;
    }
}

NamedPipeRpcServer::~NamedPipeRpcServer() {
    Stop();
}

void NamedPipeRpcServer::SetLogger(_In_opt_ RpcLogger* logger) {
    logger_.store(logger);
}

void NamedPipeRpcServer::RegisterFunction(_In_ const std::string& functionName, _In_ PermissionLevel permission,
                                          _In_ RpcHandler handler) {
    std::unique_lock lock(functionMutex_);
    functions_[functionName] = RegisteredFunction{permission, std::move(handler)};
}

void NamedPipeRpcServer::AddWhitelistProcess(_In_ const std::wstring& processPath, _In_ PermissionLevel permission) {
    std::unique_lock lock(whitelistMutex_);
    whitelist_.push_back({ToLower(processPath), permission});
}

void NamedPipeRpcServer::SetPipeSecuritySddl(_In_ const std::wstring& sddl) {
    std::lock_guard<std::mutex> lock(configurationMutex_);
    pipeSecuritySddl_ = sddl;
}

bool NamedPipeRpcServer::Start() {
    if (running_.exchange(true))
        return false;

    {
        std::lock_guard<std::mutex> lock(configurationMutex_);
        if (!pipeSecuritySddl_.empty()) {
            PSECURITY_DESCRIPTOR descriptor = nullptr;
            if (!::ConvertStringSecurityDescriptorToSecurityDescriptorW(pipeSecuritySddl_.c_str(), SDDL_REVISION_1,
                                                                         &descriptor, nullptr)) {
                running_ = false;
                LogError(L"Invalid pipe security SDDL.");
                return false;
            }
            ::LocalFree(descriptor);
        }
    }

    try {
        for (std::size_t i = 0; i < workerCount_; ++i)
            workers_.emplace_back(&NamedPipeRpcServer::WorkerLoop, this);
    } catch (...) {
        running_ = false;
        for (auto& worker : workers_)
            ::CancelSynchronousIo(worker.native_handle());
        for (auto& worker : workers_) {
            if (worker.joinable())
                worker.join();
        }
        workers_.clear();
        return false;
    }

    LogInfo(L"Server started.");
    return true;
}

void NamedPipeRpcServer::Stop() {
    if (!running_.exchange(false))
        return;

    for (auto& worker : workers_)
        ::CancelSynchronousIo(worker.native_handle());

    for (std::size_t i = 0; i < workerCount_; ++i) {
        HANDLE hPipe =
            ::CreateFileW(pipeName_.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hPipe != INVALID_HANDLE_VALUE)
            ::CloseHandle(hPipe);
    }

    for (auto& t : workers_) {
        if (t.joinable())
            t.join();
    }
    workers_.clear();

    LogInfo(L"Server stopped.");
}

HANDLE NamedPipeRpcServer::CreatePipeInstance() const {
    SECURITY_ATTRIBUTES sa{};
    SECURITY_DESCRIPTOR* pSd = nullptr;
    SECURITY_ATTRIBUTES* pSa = nullptr;

    std::wstring sddl;
    {
        std::lock_guard<std::mutex> lock(configurationMutex_);
        sddl = pipeSecuritySddl_;
    }
    if (!sddl.empty()) {
        if (::ConvertStringSecurityDescriptorToSecurityDescriptorW(
                sddl.c_str(), SDDL_REVISION_1, reinterpret_cast<PSECURITY_DESCRIPTOR*>(&pSd), nullptr)) {
            sa.nLength = sizeof(sa);
            sa.lpSecurityDescriptor = pSd;
            sa.bInheritHandle = FALSE;
            pSa = &sa;
        } else
            return INVALID_HANDLE_VALUE;
    }

    DWORD openMode = PIPE_ACCESS_DUPLEX;
#ifdef PIPE_REJECT_REMOTE_CLIENTS
    DWORD pipeMode = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS;
#else
    DWORD pipeMode = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT;
#endif

    HANDLE hPipe = ::CreateNamedPipeW(pipeName_.c_str(), openMode, pipeMode, PIPE_UNLIMITED_INSTANCES, 64 * 1024,
                                      64 * 1024, 0, pSa);

    if (pSd)
        ::LocalFree(pSd);

    return hPipe;
}

bool NamedPipeRpcServer::CheckWhitelist(_In_ const std::wstring& processPath,
                                        _Inout_ PermissionLevel& permission) const {
    std::shared_lock lock(whitelistMutex_);
    std::wstring norm = ToLower(processPath);

    for (const auto& rule : whitelist_) {
        if (norm == rule.processPath) {
            permission = rule.permission;
            return true;
        }
    }
    return false;
}

bool NamedPipeRpcServer::TryBuildClientContext(_In_ HANDLE hPipe, _Inout_ RpcCallContext& ctx) {
    ULONG pid = 0;
    if (!::GetNamedPipeClientProcessId(hPipe, &pid))
        return false;

    HANDLE hProc = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc)
        return false;

    wchar_t pathBuf[1024] = {};
    DWORD size = static_cast<DWORD>(std::size(pathBuf));
    bool ok = false;
    if (::QueryFullProcessImageNameW(hProc, 0, pathBuf, &size)) {
        ctx.clientProcessId = static_cast<std::uint32_t>(pid);
        ctx.clientProcessPath = pathBuf;
        PermissionLevel perm = PermissionLevel::Guest;
        ok = CheckWhitelist(ctx.clientProcessPath, perm);
        if (ok)
            ctx.clientPermission = perm;
    }

    ::CloseHandle(hProc);
    return ok;
}

bool NamedPipeRpcServer::ValidateRequest(_In_ const RpcRequest& request, _Out_ std::string& errorMessage) {
    if (!RpcSecurity::VerifySignature(request, sharedSecret_)) {
        errorMessage = "Signature verification failed.";
        return false;
    }

    std::int64_t now = GetCurrentUnixTimestamp();
    if (request.timestamp < now - allowedTimeSkewSeconds_ || request.timestamp > now + allowedTimeSkewSeconds_) {
        errorMessage = "Request timestamp invalid.";
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(nonceMutex_);
        CleanupExpiredNonces();

        auto it = usedNonces_.find(request.nonce);
        if (it != usedNonces_.end()) {
            errorMessage = "Replay detected: duplicated nonce.";
            return false;
        }

        usedNonces_[request.nonce] = request.timestamp;
    }

    return true;
}

RpcResult NamedPipeRpcServer::ProcessRequest(_In_ const RpcCallContext& ctx, _In_ const RpcRequest& request) {
    RpcResult result;
    result.success = false;

    std::string err;
    if (!ValidateRequest(request, err)) {
        result.errorMessage = err;
        return result;
    }

    RegisteredFunction fn;
    {
        std::shared_lock lock(functionMutex_);
        auto it = functions_.find(request.functionName);
        if (it == functions_.end()) {
            result.errorMessage = "Function not found: " + request.functionName;
            return result;
        }
        fn = it->second;
    }

    if (static_cast<std::uint32_t>(ctx.clientPermission) < static_cast<std::uint32_t>(fn.requiredPermission)) {
        result.errorMessage = "Permission denied.";
        return result;
    }

    try {
        result = fn.handler(ctx, request.args);
        if (!result.success && result.errorMessage.empty())
            result.errorMessage = "Handler returned failure.";
    } catch (const std::exception& ex) {
        result.success = false;
        result.errorMessage = ex.what();
    } catch (...) {
        result.success = false;
        result.errorMessage = "Unknown server exception.";
    }

    return result;
}

void NamedPipeRpcServer::CleanupExpiredNonces() {
    std::int64_t now = GetCurrentUnixTimestamp();
    for (auto it = usedNonces_.begin(); it != usedNonces_.end();) {
        if (it->second < now - nonceExpireSeconds_)
            it = usedNonces_.erase(it);
        else
            ++it;
    }
}

void NamedPipeRpcServer::WorkerLoop() {
    while (running_) {
        HANDLE hPipe = CreatePipeInstance();
        if (hPipe == INVALID_HANDLE_VALUE) {
            ::Sleep(50);
            continue;
        }

        BOOL connected = ::ConnectNamedPipe(hPipe, nullptr) ? TRUE : (::GetLastError() == ERROR_PIPE_CONNECTED);

        if (!connected) {
            ::CloseHandle(hPipe);
            continue;
        }

        if (!running_) {
            ::DisconnectNamedPipe(hPipe);
            ::CloseHandle(hPipe);
            break;
        }

        RpcCallContext clientCtx;
        if (!TryBuildClientContext(hPipe, clientCtx)) {
            LogWarn(L"Rejected client: not in whitelist or failed to identify.");
            ::DisconnectNamedPipe(hPipe);
            ::CloseHandle(hPipe);
            continue;
        }

        LogInfo(L"Accepted client PID=" + std::to_wstring(clientCtx.clientProcessId) + L", Path=" +
                clientCtx.clientProcessPath);

        while (running_) {
            std::vector<std::uint8_t> reqData;
            if (!ReadMessageFromPipe(hPipe, reqData))
                break;

            RpcResult result;
            RpcRequest request;

            if (!DeserializeRequest(reqData, request)) {
                result.success = false;
                result.errorMessage = "Invalid request format.";
            } else {
                result = ProcessRequest(clientCtx, request);
            }

            auto resp = SerializeResult(result);
            if (!WriteMessageToPipe(hPipe, resp))
                break;
        }

        if (running_)
            ::FlushFileBuffers(hPipe);
        ::DisconnectNamedPipe(hPipe);
        ::CloseHandle(hPipe);
    }
}

void NamedPipeRpcServer::LogInfo(_In_ const std::wstring& msg) {
    if (RpcLogger* logger = logger_.load())
        logger->Info(msg);
}

void NamedPipeRpcServer::LogWarn(_In_ const std::wstring& msg) {
    if (RpcLogger* logger = logger_.load())
        logger->Warn(msg);
}

void NamedPipeRpcServer::LogError(_In_ const std::wstring& msg) {
    if (RpcLogger* logger = logger_.load())
        logger->Error(msg);
}
} // namespace ytpp::client_server
