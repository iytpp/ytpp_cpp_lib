#include "client-server/RPC-named-pip/RpcLogger.h"
#include <windows.h>


namespace ytpp {
	namespace client_server
	{
		RpcLogger::RpcLogger() {}
		RpcLogger::~RpcLogger() { Close(); }

		bool RpcLogger::Open(const std::wstring& filePath, LogLevel minLevel)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_minLevel = minLevel;
			m_file.open(filePath, std::ios::app);
			return m_file.is_open();
		}

		void RpcLogger::Close()
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_file.is_open())
				m_file.close();
		}

		void RpcLogger::Debug(const std::wstring& msg) { Write(LogLevel::Debug, msg); }
		void RpcLogger::Info(const std::wstring& msg) { Write(LogLevel::Info, msg); }
		void RpcLogger::Warn(const std::wstring& msg) { Write(LogLevel::Warn, msg); }
		void RpcLogger::Error(const std::wstring& msg) { Write(LogLevel::Error, msg); }

		void RpcLogger::Write(LogLevel level, const std::wstring& msg)
		{
			if (level < m_minLevel) return;

			std::lock_guard<std::mutex> lock(m_mutex);
			if (!m_file.is_open()) return;

			m_file << L"[" << NowText() << L"]"
				<< L"[" << LevelToString(level) << L"] "
				<< msg << std::endl;
		}

		std::wstring RpcLogger::LevelToString(LogLevel level) const
		{
			switch (level)
			{
			case LogLevel::Debug: return L"DEBUG";
			case LogLevel::Info:  return L"INFO";
			case LogLevel::Warn:  return L"WARN";
			case LogLevel::Error: return L"ERROR";
			default: return L"UNKNOWN";
			}
		}

		std::wstring RpcLogger::NowText() const
		{
			SYSTEMTIME st{};
			GetLocalTime(&st);

			wchar_t buf[64] = {};
			swprintf_s(buf, L"%04u-%02u-%02u %02u:%02u:%02u.%03u",
				st.wYear, st.wMonth, st.wDay,
				st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
			return buf;
		}
	}
}

