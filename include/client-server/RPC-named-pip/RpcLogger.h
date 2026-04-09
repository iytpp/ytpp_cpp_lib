#pragma once
#include <string>
#include <mutex>
#include <fstream>




namespace ytpp {
	namespace client_server
	{
		enum class LogLevel
		{
			Debug = 0,
			Info,
			Warn,
			Error
		};

		class RpcLogger
		{
		public:
			RpcLogger();
			~RpcLogger();

			bool Open(const std::wstring& filePath, LogLevel minLevel = LogLevel::Debug);
			void Close();

			void Debug(const std::wstring& msg);
			void Info(const std::wstring& msg);
			void Warn(const std::wstring& msg);
			void Error(const std::wstring& msg);

			void Write(LogLevel level, const std::wstring& msg);

		private:
			std::wstring LevelToString(LogLevel level) const;
			std::wstring NowText() const;

		private:
			std::wofstream m_file;
			std::mutex m_mutex;
			LogLevel m_minLevel = LogLevel::Debug;
		};
	}
}


