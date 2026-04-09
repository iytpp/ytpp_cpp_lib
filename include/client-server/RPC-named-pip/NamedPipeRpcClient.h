#pragma once
#include "RpcCommon.h"
#include <string>
#include <vector>
#include <future>
#include <mutex>
#include <condition_variable>
#include <memory>



namespace ytpp {
	namespace client_server
	{
		struct RpcAsyncResult
		{
			bool ok = false;
			RpcResult result;
			std::string errorMessage;
		};

		class NamedPipeRpcClient
		{
		public:
			NamedPipeRpcClient(const std::wstring& pipeName,
				const std::string& sharedSecret,
				DWORD connectTimeoutMs = 2000,
				size_t minPoolSize = 2,
				size_t maxPoolSize = 16);
			~NamedPipeRpcClient();

			void SetTimeout(DWORD timeoutMs);

			bool Call(const std::string& functionName,
				const RpcArray& args,
				RpcResult& result,
				std::string& errorMessage);

			std::future<RpcAsyncResult> CallAsync(const std::string& functionName,
				const RpcArray& args);

		private:
			struct Connection
			{
				HANDLE hPipe = INVALID_HANDLE_VALUE;
				bool busy = false;
			};

			bool Connect(Connection& conn, std::string& errorMessage);
			void Close(Connection& conn);
			std::shared_ptr<Connection> AcquireConnection(std::string& errorMessage);
			void ReleaseConnection(const std::shared_ptr<Connection>& conn);
			bool SendAndReceive(Connection& conn,
				const std::string& functionName,
				const RpcArray& args,
				RpcResult& result,
				std::string& errorMessage);

		private:
			std::wstring m_pipeName;
			std::string m_sharedSecret;
			DWORD m_connectTimeoutMs = 2000;

			size_t m_minPoolSize = 2;
			size_t m_maxPoolSize = 16;

			std::vector<std::shared_ptr<Connection>> m_pool;
			std::mutex m_poolMutex;
			std::condition_variable m_poolCv;
		};
	}
}

