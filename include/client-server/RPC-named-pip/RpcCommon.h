#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <cstdint>
#include <optional>

namespace ytpp {
	namespace client_server
	{
		enum class RpcValueType : uint8_t
		{
			Null = 0,
			Bool = 1,
			Int32 = 2,
			Int64 = 3,
			UInt32 = 4,
			UInt64 = 5,
			Double = 6,
			String = 7,
			WString = 8,
			Binary = 9,
			Array = 10,
			Object = 11
		};

		struct RpcValue;
		using RpcArray = std::vector<RpcValue>;
		using RpcObject = std::map<std::string, RpcValue>;
		using RpcBinary = std::vector<uint8_t>;

		struct RpcValue
		{
		public:
			using VariantType = std::variant<
				std::monostate,
				bool,
				int32_t,
				int64_t,
				uint32_t,
				uint64_t,
				double,
				std::string,
				std::wstring,
				RpcBinary,
				RpcArray,
				RpcObject
			>;

			RpcValue();
			RpcValue(std::nullptr_t);
			RpcValue(bool v);
			RpcValue(int32_t v);
			RpcValue(int64_t v);
			RpcValue(uint32_t v);
			RpcValue(uint64_t v);
			RpcValue(double v);
			RpcValue(const std::string& v);
			RpcValue(const char* v);
			RpcValue(const std::wstring& v);
			RpcValue(const wchar_t* v);
			RpcValue(const RpcBinary& v);
			RpcValue(const RpcArray& v);
			RpcValue(const RpcObject& v);

			RpcValueType GetType() const;

			bool IsNull() const;
			bool IsBool() const;
			bool IsInt32() const;
			bool IsInt64() const;
			bool IsUInt32() const;
			bool IsUInt64() const;
			bool IsDouble() const;
			bool IsString() const;
			bool IsWString() const;
			bool IsBinary() const;
			bool IsArray() const;
			bool IsObject() const;

			bool AsBool() const;
			int32_t AsInt32() const;
			int64_t AsInt64() const;
			uint32_t AsUInt32() const;
			uint64_t AsUInt64() const;
			double AsDouble() const;
			const std::string& AsString() const;
			const std::wstring& AsWString() const;
			const RpcBinary& AsBinary() const;
			const RpcArray& AsArray() const;
			const RpcObject& AsObject() const;

			RpcArray& AsArray();
			RpcObject& AsObject();

			const VariantType& GetRaw() const;

		private:
			VariantType m_value;
		};

		enum class PermissionLevel : uint32_t
		{
			Guest = 0,
			User = 1,
			Admin = 2,
			Super = 3
		};

		struct RpcRequest
		{
			std::string functionName;
			RpcArray args;
			int64_t timestamp = 0;
			std::string nonce;
			std::string signature;
		};

		struct RpcResult
		{
			bool success = false;
			RpcArray returnValues;
			std::string errorMessage;
		};

		struct RpcCallContext
		{
			uint32_t clientProcessId = 0;
			std::wstring clientProcessPath;
			PermissionLevel clientPermission = PermissionLevel::Guest;
		};

		class ByteBufferWriter
		{
		public:
			void WriteUInt8(uint8_t value);
			void WriteUInt32(uint32_t value);
			void WriteUInt64(uint64_t value);
			void WriteInt32(int32_t value);
			void WriteInt64(int64_t value);
			void WriteBool(bool value);
			void WriteDouble(double value);
			void WriteString(const std::string& value);
			void WriteWString(const std::wstring& value);
			void WriteBinary(const RpcBinary& value);
			void WriteRpcValue(const RpcValue& value);
			const std::vector<uint8_t>& GetBuffer() const;

		private:
			std::vector<uint8_t> m_buffer;
		};

		class ByteBufferReader
		{
		public:
			ByteBufferReader(const std::vector<uint8_t>& buffer);

			bool ReadUInt8(uint8_t& value);
			bool ReadUInt32(uint32_t& value);
			bool ReadUInt64(uint64_t& value);
			bool ReadInt32(int32_t& value);
			bool ReadInt64(int64_t& value);
			bool ReadBool(bool& value);
			bool ReadDouble(double& value);
			bool ReadString(std::string& value);
			bool ReadWString(std::wstring& value);
			bool ReadBinary(RpcBinary& value);
			bool ReadRpcValue(RpcValue& value);

		private:
			const std::vector<uint8_t>& m_buffer;
			size_t m_offset = 0;
		};

		std::vector<uint8_t> SerializeRequest(const RpcRequest& request);
		bool DeserializeRequest(const std::vector<uint8_t>& data, RpcRequest& request);

		std::vector<uint8_t> SerializeResult(const RpcResult& result);
		bool DeserializeResult(const std::vector<uint8_t>& data, RpcResult& result);

		std::string BuildCanonicalRequestText(const RpcRequest& request);

		bool WriteMessageToPipe(HANDLE hPipe, const std::vector<uint8_t>& data);
		bool ReadMessageFromPipe(HANDLE hPipe, std::vector<uint8_t>& data);

		std::wstring Utf8ToWide(const std::string& str);
		std::string WideToUtf8(const std::wstring& str);

		int64_t GetCurrentUnixTimestamp();
		std::string GenerateNonce();

		std::wstring ToLowerString(const std::wstring& s);
	}
}

