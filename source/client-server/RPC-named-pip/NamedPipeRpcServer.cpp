#include "client-server/RPC-named-pip/NamedPipeRpcServer.h"
#include "client-server/RPC-named-pip/RpcSecurity.h"
#include <windows.h>
#include <sddl.h>
#include <vector>


namespace ytpp {
	namespace client_server
	{
		NamedPipeRpcServer::NamedPipeRpcServer(const std::wstring& pipeName,
			const std::string& sharedSecret,
			size_t workerCount)
			: m_pipeName(pipeName),
			m_sharedSecret(sharedSecret)
		{
			if (workerCount == 0)
			{
				size_t hc = std::thread::hardware_concurrency();
				m_workerCount = (hc == 0 ? 8 : hc * 2);
			}
			else
			{
				m_workerCount = workerCount;
			}
		}

		NamedPipeRpcServer::~NamedPipeRpcServer()
		{
			Stop();
		}

		void NamedPipeRpcServer::SetLogger(RpcLogger* logger)
		{
			m_logger = logger;
		}

		void NamedPipeRpcServer::RegisterFunction(const std::string& functionName,
			PermissionLevel permission,
			RpcHandler handler)
		{
			std::unique_lock lock(m_functionMutex);
			m_functions[functionName] = RegisteredFunction{ permission, std::move(handler) };
		}

		void NamedPipeRpcServer::AddWhitelistProcess(const std::wstring& processPath, PermissionLevel permission)
		{
			std::unique_lock lock(m_whitelistMutex);
			m_whitelist.push_back({ ToLowerString(processPath), permission });
		}

		void NamedPipeRpcServer::SetPipeSecuritySddl(const std::wstring& sddl)
		{
			m_pipeSecuritySddl = sddl;
		}

		bool NamedPipeRpcServer::Start()
		{
			if (m_running.exchange(true))
				return false;

			for (size_t i = 0; i < m_workerCount; ++i)
				m_workers.emplace_back(&NamedPipeRpcServer::WorkerLoop, this);

			LogInfo(L"Server started.");
			return true;
		}

		void NamedPipeRpcServer::Stop()
		{
			if (!m_running.exchange(false))
				return;

			for (size_t i = 0; i < m_workerCount; ++i)
			{
				HANDLE hPipe = CreateFileW(
					m_pipeName.c_str(),
					GENERIC_READ | GENERIC_WRITE,
					0, nullptr, OPEN_EXISTING, 0, nullptr);
				if (hPipe != INVALID_HANDLE_VALUE)
					CloseHandle(hPipe);
			}

			for (auto& t : m_workers)
			{
				if (t.joinable())
					t.join();
			}
			m_workers.clear();

			LogInfo(L"Server stopped.");
		}

		HANDLE NamedPipeRpcServer::CreatePipeInstance() const
		{
			SECURITY_ATTRIBUTES sa{};
			SECURITY_DESCRIPTOR* pSd = nullptr;
			SECURITY_ATTRIBUTES* pSa = nullptr;

			if (!m_pipeSecuritySddl.empty())
			{
				if (ConvertStringSecurityDescriptorToSecurityDescriptorW(
					m_pipeSecuritySddl.c_str(),
					SDDL_REVISION_1,
					reinterpret_cast<PSECURITY_DESCRIPTOR*>(&pSd),
					nullptr))
				{
					sa.nLength = sizeof(sa);
					sa.lpSecurityDescriptor = pSd;
					sa.bInheritHandle = FALSE;
					pSa = &sa;
				}
			}

			DWORD openMode = PIPE_ACCESS_DUPLEX;
#ifdef PIPE_REJECT_REMOTE_CLIENTS
			DWORD pipeMode = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS;
#else
			DWORD pipeMode = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT;
#endif

			HANDLE hPipe = CreateNamedPipeW(
				m_pipeName.c_str(),
				openMode,
				pipeMode,
				PIPE_UNLIMITED_INSTANCES,
				64 * 1024,
				64 * 1024,
				0,
				pSa
			);

			if (pSd)
				LocalFree(pSd);

			return hPipe;
		}

		bool NamedPipeRpcServer::CheckWhitelist(const std::wstring& processPath, PermissionLevel& permission) const
		{
			std::shared_lock lock(m_whitelistMutex);
			std::wstring norm = ToLowerString(processPath);

			for (const auto& rule : m_whitelist)
			{
				if (norm == rule.processPath)
				{
					permission = rule.permission;
					return true;
				}
			}
			return false;
		}

		bool NamedPipeRpcServer::TryBuildClientContext(HANDLE hPipe, RpcCallContext& ctx)
		{
			ULONG pid = 0;
			if (!GetNamedPipeClientProcessId(hPipe, &pid))
				return false;

			HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
			if (!hProc)
				return false;

			wchar_t pathBuf[1024] = {};
			DWORD size = static_cast<DWORD>(std::size(pathBuf));
			bool ok = false;
			if (QueryFullProcessImageNameW(hProc, 0, pathBuf, &size))
			{
				ctx.clientProcessId = static_cast<uint32_t>(pid);
				ctx.clientProcessPath = pathBuf;
				PermissionLevel perm = PermissionLevel::Guest;
				ok = CheckWhitelist(ctx.clientProcessPath, perm);
				if (ok)
					ctx.clientPermission = perm;
			}

			CloseHandle(hProc);
			return ok;
		}

		bool NamedPipeRpcServer::ValidateRequest(const RpcRequest& request, std::string& errorMessage)
		{
			if (!RpcSecurity::VerifySignature(request, m_sharedSecret))
			{
				errorMessage = "Signature verification failed.";
				return false;
			}

			int64_t now = GetCurrentUnixTimestamp();
			if (request.timestamp < now - m_allowedTimeSkewSeconds ||
				request.timestamp > now + m_allowedTimeSkewSeconds)
			{
				errorMessage = "Request timestamp invalid.";
				return false;
			}

			{
				std::lock_guard<std::mutex> lock(m_nonceMutex);
				CleanupExpiredNonces();

				auto it = m_usedNonces.find(request.nonce);
				if (it != m_usedNonces.end())
				{
					errorMessage = "Replay detected: duplicated nonce.";
					return false;
				}

				m_usedNonces[request.nonce] = request.timestamp;
			}

			return true;
		}

		RpcResult NamedPipeRpcServer::ProcessRequest(const RpcCallContext& ctx, const RpcRequest& request)
		{
			RpcResult result;
			result.success = false;

			std::string err;
			if (!ValidateRequest(request, err))
			{
				result.errorMessage = err;
				return result;
			}

			RegisteredFunction fn;
			{
				std::shared_lock lock(m_functionMutex);
				auto it = m_functions.find(request.functionName);
				if (it == m_functions.end())
				{
					result.errorMessage = "Function not found: " + request.functionName;
					return result;
				}
				fn = it->second;
			}

			if (static_cast<uint32_t>(ctx.clientPermission) < static_cast<uint32_t>(fn.requiredPermission))
			{
				result.errorMessage = "Permission denied.";
				return result;
			}

			try
			{
				result = fn.handler(ctx, request.args);
				if (!result.success && result.errorMessage.empty())
					result.errorMessage = "Handler returned failure.";
			}
			catch (const std::exception& ex)
			{
				result.success = false;
				result.errorMessage = ex.what();
			}
			catch (...)
			{
				result.success = false;
				result.errorMessage = "Unknown server exception.";
			}

			return result;
		}

		void NamedPipeRpcServer::CleanupExpiredNonces()
		{
			int64_t now = GetCurrentUnixTimestamp();
			for (auto it = m_usedNonces.begin(); it != m_usedNonces.end(); )
			{
				if (it->second < now - m_nonceExpireSeconds)
					it = m_usedNonces.erase(it);
				else
					++it;
			}
		}

		void NamedPipeRpcServer::WorkerLoop()
		{
			while (m_running)
			{
				HANDLE hPipe = CreatePipeInstance();
				if (hPipe == INVALID_HANDLE_VALUE)
				{
					Sleep(50);
					continue;
				}

				BOOL connected = ConnectNamedPipe(hPipe, nullptr) ?
					TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

				if (!connected)
				{
					CloseHandle(hPipe);
					continue;
				}

				if (!m_running)
				{
					DisconnectNamedPipe(hPipe);
					CloseHandle(hPipe);
					break;
				}

				RpcCallContext clientCtx;
				if (!TryBuildClientContext(hPipe, clientCtx))
				{
					LogWarn(L"Rejected client: not in whitelist or failed to identify.");
					DisconnectNamedPipe(hPipe);
					CloseHandle(hPipe);
					continue;
				}

				LogInfo(L"Accepted client PID=" + std::to_wstring(clientCtx.clientProcessId) +
					L", Path=" + clientCtx.clientProcessPath);

				while (m_running)
				{
					std::vector<uint8_t> reqData;
					if (!ReadMessageFromPipe(hPipe, reqData))
						break;

					RpcResult result;
					RpcRequest request;

					if (!DeserializeRequest(reqData, request))
					{
						result.success = false;
						result.errorMessage = "Invalid request format.";
					}
					else
					{
						result = ProcessRequest(clientCtx, request);
					}

					auto resp = SerializeResult(result);
					if (!WriteMessageToPipe(hPipe, resp))
						break;
				}

				FlushFileBuffers(hPipe);
				DisconnectNamedPipe(hPipe);
				CloseHandle(hPipe);
			}
		}

		void NamedPipeRpcServer::LogInfo(const std::wstring& msg)
		{
			if (m_logger) m_logger->Info(msg);
		}

		void NamedPipeRpcServer::LogWarn(const std::wstring& msg)
		{
			if (m_logger) m_logger->Warn(msg);
		}

		void NamedPipeRpcServer::LogError(const std::wstring& msg)
		{
			if (m_logger) m_logger->Error(msg);
		}
	}
}

