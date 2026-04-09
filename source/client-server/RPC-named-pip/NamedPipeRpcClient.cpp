#include "client-server/RPC-named-pip/NamedPipeRpcClient.h"
#include "client-server/RPC-named-pip/RpcSecurity.h"
#include <windows.h>



namespace ytpp {
	namespace client_server
	{
		NamedPipeRpcClient::NamedPipeRpcClient(const std::wstring& pipeName,
			const std::string& sharedSecret,
			DWORD connectTimeoutMs,
			size_t minPoolSize,
			size_t maxPoolSize)
			: m_pipeName(pipeName),
			m_sharedSecret(sharedSecret),
			m_connectTimeoutMs(connectTimeoutMs),
			m_minPoolSize(minPoolSize),
			m_maxPoolSize(maxPoolSize)
		{
			if (m_minPoolSize > m_maxPoolSize)
				m_minPoolSize = m_maxPoolSize;

			for (size_t i = 0; i < m_minPoolSize; ++i)
				m_pool.push_back(std::make_shared<Connection>());
		}

		NamedPipeRpcClient::~NamedPipeRpcClient()
		{
			std::lock_guard<std::mutex> lock(m_poolMutex);
			for (auto& c : m_pool)
				Close(*c);
			m_pool.clear();
		}

		void NamedPipeRpcClient::SetTimeout(DWORD timeoutMs)
		{
			m_connectTimeoutMs = timeoutMs;
		}

		bool NamedPipeRpcClient::Connect(Connection& conn, std::string& errorMessage)
		{
			if (conn.hPipe != INVALID_HANDLE_VALUE)
				return true;

			if (!WaitNamedPipeW(m_pipeName.c_str(), m_connectTimeoutMs))
			{
				DWORD err = GetLastError();
				if (err == ERROR_FILE_NOT_FOUND)
					errorMessage = "Server not running.";
				else if (err == ERROR_SEM_TIMEOUT)
					errorMessage = "Connect timeout.";
				else
					errorMessage = "WaitNamedPipe failed, error=" + std::to_string(err);
				return false;
			}

			conn.hPipe = CreateFileW(
				m_pipeName.c_str(),
				GENERIC_READ | GENERIC_WRITE,
				0, nullptr, OPEN_EXISTING, 0, nullptr);

			if (conn.hPipe == INVALID_HANDLE_VALUE)
			{
				errorMessage = "CreateFileW failed, error=" + std::to_string(GetLastError());
				return false;
			}

			DWORD mode = PIPE_READMODE_BYTE;
			if (!SetNamedPipeHandleState(conn.hPipe, &mode, nullptr, nullptr))
			{
				errorMessage = "SetNamedPipeHandleState failed, error=" + std::to_string(GetLastError());
				Close(conn);
				return false;
			}

			return true;
		}

		void NamedPipeRpcClient::Close(Connection& conn)
		{
			if (conn.hPipe != INVALID_HANDLE_VALUE)
			{
				CloseHandle(conn.hPipe);
				conn.hPipe = INVALID_HANDLE_VALUE;
			}
		}

		std::shared_ptr<NamedPipeRpcClient::Connection> NamedPipeRpcClient::AcquireConnection(std::string& errorMessage)
		{
			std::unique_lock<std::mutex> lock(m_poolMutex);

			for (;;)
			{
				for (auto& conn : m_pool)
				{
					if (!conn->busy)
					{
						conn->busy = true;
						return conn;
					}
				}

				if (m_pool.size() < m_maxPoolSize)
				{
					auto conn = std::make_shared<Connection>();
					conn->busy = true;
					m_pool.push_back(conn);
					return conn;
				}

				if (m_poolCv.wait_for(lock, std::chrono::milliseconds(m_connectTimeoutMs)) == std::cv_status::timeout)
				{
					errorMessage = "Acquire connection from pool timeout.";
					return nullptr;
				}
			}
		}

		void NamedPipeRpcClient::ReleaseConnection(const std::shared_ptr<Connection>& conn)
		{
			{
				std::lock_guard<std::mutex> lock(m_poolMutex);
				conn->busy = false;
			}
			m_poolCv.notify_one();
		}

		bool NamedPipeRpcClient::SendAndReceive(Connection& conn,
			const std::string& functionName,
			const RpcArray& args,
			RpcResult& result,
			std::string& errorMessage)
		{
			RpcRequest req;
			req.functionName = functionName;
			req.args = args;
			req.timestamp = GetCurrentUnixTimestamp();
			req.nonce = GenerateNonce();
			req.signature = RpcSecurity::MakeSignature(req, m_sharedSecret);

			auto reqBytes = SerializeRequest(req);
			if (!WriteMessageToPipe(conn.hPipe, reqBytes))
			{
				errorMessage = "Write request failed.";
				return false;
			}

			std::vector<uint8_t> respBytes;
			if (!ReadMessageFromPipe(conn.hPipe, respBytes))
			{
				errorMessage = "Read response failed.";
				return false;
			}

			if (!DeserializeResult(respBytes, result))
			{
				errorMessage = "Invalid response format.";
				return false;
			}

			if (!result.success)
			{
				errorMessage = result.errorMessage;
				return false;
			}

			return true;
		}

		bool NamedPipeRpcClient::Call(const std::string& functionName,
			const RpcArray& args,
			RpcResult& result,
			std::string& errorMessage)
		{
			result = RpcResult{};
			errorMessage.clear();

			auto conn = AcquireConnection(errorMessage);
			if (!conn)
				return false;

			auto releaseGuard = std::unique_ptr<void, std::function<void(void*)>>(
				reinterpret_cast<void*>(1),
				[&](void*) { ReleaseConnection(conn); });

			if (!Connect(*conn, errorMessage))
				return false;

			if (SendAndReceive(*conn, functionName, args, result, errorMessage))
				return true;

			DWORD lastErr = GetLastError();
			if (lastErr == ERROR_BROKEN_PIPE || lastErr == ERROR_NO_DATA)
			{
				Close(*conn);
				if (!Connect(*conn, errorMessage))
					return false;

				result = RpcResult{};
				errorMessage.clear();
				return SendAndReceive(*conn, functionName, args, result, errorMessage);
			}

			return false;
		}

		std::future<RpcAsyncResult> NamedPipeRpcClient::CallAsync(const std::string& functionName,
			const RpcArray& args)
		{
			return std::async(std::launch::async, [this, functionName, args]() -> RpcAsyncResult
				{
					RpcAsyncResult ret;
					ret.ok = Call(functionName, args, ret.result, ret.errorMessage);
					return ret;
				});
		}
	}
}

