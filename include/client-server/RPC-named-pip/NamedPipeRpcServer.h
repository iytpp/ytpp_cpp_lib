#pragma once
#include "RpcCommon.h"
#include "RpcLogger.h"
#include <functional>
#include <unordered_map>
#include <thread>
#include <shared_mutex>
#include <atomic>
#include <mutex>
#include <memory>

#pragma comment(lib, "advapi32.lib")



namespace ytpp {
	namespace client_server
	{
		struct WhitelistRule
		{
			std::wstring processPath;
			PermissionLevel permission = PermissionLevel::Guest;
		};

		class NamedPipeRpcServer
		{
		public:
			using RpcHandler = std::function<RpcResult(const RpcCallContext&, const RpcArray&)>;

			NamedPipeRpcServer(const std::wstring& pipeName,
				const std::string& sharedSecret,
				size_t workerCount = 0);
			~NamedPipeRpcServer();

			bool Start();
			void Stop();

			void RegisterFunction(const std::string& functionName,
				PermissionLevel permission,
				RpcHandler handler);

			void AddWhitelistProcess(const std::wstring& processPath, PermissionLevel permission);
			void SetPipeSecuritySddl(const std::wstring& sddl);
			void SetLogger(RpcLogger* logger);

		private:
			struct RegisteredFunction
			{
				PermissionLevel requiredPermission = PermissionLevel::Guest;
				RpcHandler handler;
			};

			HANDLE CreatePipeInstance() const;
			void WorkerLoop();

			bool TryBuildClientContext(HANDLE hPipe, RpcCallContext& ctx);
			bool CheckWhitelist(const std::wstring& processPath, PermissionLevel& permission) const;
			bool ValidateRequest(const RpcRequest& request, std::string& errorMessage);
			RpcResult ProcessRequest(const RpcCallContext& ctx, const RpcRequest& request);

			void CleanupExpiredNonces();
			void LogInfo(const std::wstring& msg);
			void LogWarn(const std::wstring& msg);
			void LogError(const std::wstring& msg);

		private:
			std::wstring m_pipeName;
			std::string m_sharedSecret;
			size_t m_workerCount = 0;
			std::vector<std::thread> m_workers;
			std::atomic<bool> m_running{ false };

			std::unordered_map<std::string, RegisteredFunction> m_functions;
			mutable std::shared_mutex m_functionMutex;

			std::vector<WhitelistRule> m_whitelist;
			mutable std::shared_mutex m_whitelistMutex;

			std::unordered_map<std::string, int64_t> m_usedNonces;
			std::mutex m_nonceMutex;

			std::wstring m_pipeSecuritySddl;
			RpcLogger* m_logger = nullptr;

			int64_t m_allowedTimeSkewSeconds = 30;
			int64_t m_nonceExpireSeconds = 60;
		};
	}
}


