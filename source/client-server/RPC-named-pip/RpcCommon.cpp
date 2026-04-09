#include "client-server/RPC-named-pip/RpcCommon.h"
#include <stdexcept>
#include <sstream>
#include <random>
#include <ctime>
#include <functional>
#include <algorithm>

namespace ytpp {
	namespace client_server
	{
		RpcValue::RpcValue() : m_value(std::monostate{}) {}
		RpcValue::RpcValue(std::nullptr_t) : m_value(std::monostate{}) {}
		RpcValue::RpcValue(bool v) : m_value(v) {}
		RpcValue::RpcValue(int32_t v) : m_value(v) {}
		RpcValue::RpcValue(int64_t v) : m_value(v) {}
		RpcValue::RpcValue(uint32_t v) : m_value(v) {}
		RpcValue::RpcValue(uint64_t v) : m_value(v) {}
		RpcValue::RpcValue(double v) : m_value(v) {}
		RpcValue::RpcValue(const std::string& v) : m_value(v) {}
		RpcValue::RpcValue(const char* v) : m_value(std::string(v ? v : "")) {}
		RpcValue::RpcValue(const std::wstring& v) : m_value(v) {}
		RpcValue::RpcValue(const wchar_t* v) : m_value(std::wstring(v ? v : L"")) {}
		RpcValue::RpcValue(const RpcBinary& v) : m_value(v) {}
		RpcValue::RpcValue(const RpcArray& v) : m_value(v) {}
		RpcValue::RpcValue(const RpcObject& v) : m_value(v) {}

		RpcValueType RpcValue::GetType() const
		{
			if (std::holds_alternative<std::monostate>(m_value)) return RpcValueType::Null;
			if (std::holds_alternative<bool>(m_value)) return RpcValueType::Bool;
			if (std::holds_alternative<int32_t>(m_value)) return RpcValueType::Int32;
			if (std::holds_alternative<int64_t>(m_value)) return RpcValueType::Int64;
			if (std::holds_alternative<uint32_t>(m_value)) return RpcValueType::UInt32;
			if (std::holds_alternative<uint64_t>(m_value)) return RpcValueType::UInt64;
			if (std::holds_alternative<double>(m_value)) return RpcValueType::Double;
			if (std::holds_alternative<std::string>(m_value)) return RpcValueType::String;
			if (std::holds_alternative<std::wstring>(m_value)) return RpcValueType::WString;
			if (std::holds_alternative<RpcBinary>(m_value)) return RpcValueType::Binary;
			if (std::holds_alternative<RpcArray>(m_value)) return RpcValueType::Array;
			return RpcValueType::Object;
		}

		bool RpcValue::IsNull() const { return std::holds_alternative<std::monostate>(m_value); }
		bool RpcValue::IsBool() const { return std::holds_alternative<bool>(m_value); }
		bool RpcValue::IsInt32() const { return std::holds_alternative<int32_t>(m_value); }
		bool RpcValue::IsInt64() const { return std::holds_alternative<int64_t>(m_value); }
		bool RpcValue::IsUInt32() const { return std::holds_alternative<uint32_t>(m_value); }
		bool RpcValue::IsUInt64() const { return std::holds_alternative<uint64_t>(m_value); }
		bool RpcValue::IsDouble() const { return std::holds_alternative<double>(m_value); }
		bool RpcValue::IsString() const { return std::holds_alternative<std::string>(m_value); }
		bool RpcValue::IsWString() const { return std::holds_alternative<std::wstring>(m_value); }
		bool RpcValue::IsBinary() const { return std::holds_alternative<RpcBinary>(m_value); }
		bool RpcValue::IsArray() const { return std::holds_alternative<RpcArray>(m_value); }
		bool RpcValue::IsObject() const { return std::holds_alternative<RpcObject>(m_value); }

		bool RpcValue::AsBool() const { return std::get<bool>(m_value); }
		int32_t RpcValue::AsInt32() const { return std::get<int32_t>(m_value); }
		int64_t RpcValue::AsInt64() const { return std::get<int64_t>(m_value); }
		uint32_t RpcValue::AsUInt32() const { return std::get<uint32_t>(m_value); }
		uint64_t RpcValue::AsUInt64() const { return std::get<uint64_t>(m_value); }
		double RpcValue::AsDouble() const { return std::get<double>(m_value); }
		const std::string& RpcValue::AsString() const { return std::get<std::string>(m_value); }
		const std::wstring& RpcValue::AsWString() const { return std::get<std::wstring>(m_value); }
		const RpcBinary& RpcValue::AsBinary() const { return std::get<RpcBinary>(m_value); }
		const RpcArray& RpcValue::AsArray() const { return std::get<RpcArray>(m_value); }
		const RpcObject& RpcValue::AsObject() const { return std::get<RpcObject>(m_value); }
		RpcArray& RpcValue::AsArray() { return std::get<RpcArray>(m_value); }
		RpcObject& RpcValue::AsObject() { return std::get<RpcObject>(m_value); }
		const RpcValue::VariantType& RpcValue::GetRaw() const { return m_value; }

		void ByteBufferWriter::WriteUInt8(uint8_t value) { m_buffer.push_back(value); }

		void ByteBufferWriter::WriteUInt32(uint32_t value)
		{
			for (int i = 0; i < 4; ++i)
				m_buffer.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
		}

		void ByteBufferWriter::WriteUInt64(uint64_t value)
		{
			for (int i = 0; i < 8; ++i)
				m_buffer.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
		}

		void ByteBufferWriter::WriteInt32(int32_t value) { WriteUInt32(static_cast<uint32_t>(value)); }
		void ByteBufferWriter::WriteInt64(int64_t value) { WriteUInt64(static_cast<uint64_t>(value)); }
		void ByteBufferWriter::WriteBool(bool value) { WriteUInt8(value ? 1 : 0); }

		void ByteBufferWriter::WriteDouble(double value)
		{
			static_assert(sizeof(double) == 8, "double must be 8 bytes");
			union { double d; uint64_t u; } u{};
			u.d = value;
			WriteUInt64(u.u);
		}

		void ByteBufferWriter::WriteString(const std::string& value)
		{
			WriteUInt32(static_cast<uint32_t>(value.size()));
			m_buffer.insert(m_buffer.end(), value.begin(), value.end());
		}

		void ByteBufferWriter::WriteWString(const std::wstring& value)
		{
			WriteUInt32(static_cast<uint32_t>(value.size()));
			const uint8_t* p = reinterpret_cast<const uint8_t*>(value.data());
			m_buffer.insert(m_buffer.end(), p, p + value.size() * sizeof(wchar_t));
		}

		void ByteBufferWriter::WriteBinary(const RpcBinary& value)
		{
			WriteUInt32(static_cast<uint32_t>(value.size()));
			m_buffer.insert(m_buffer.end(), value.begin(), value.end());
		}

		void ByteBufferWriter::WriteRpcValue(const RpcValue& value)
		{
			WriteUInt8(static_cast<uint8_t>(value.GetType()));

			switch (value.GetType())
			{
			case RpcValueType::Null:
				break;
			case RpcValueType::Bool:
				WriteBool(value.AsBool());
				break;
			case RpcValueType::Int32:
				WriteInt32(value.AsInt32());
				break;
			case RpcValueType::Int64:
				WriteInt64(value.AsInt64());
				break;
			case RpcValueType::UInt32:
				WriteUInt32(value.AsUInt32());
				break;
			case RpcValueType::UInt64:
				WriteUInt64(value.AsUInt64());
				break;
			case RpcValueType::Double:
				WriteDouble(value.AsDouble());
				break;
			case RpcValueType::String:
				WriteString(value.AsString());
				break;
			case RpcValueType::WString:
				WriteWString(value.AsWString());
				break;
			case RpcValueType::Binary:
				WriteBinary(value.AsBinary());
				break;
			case RpcValueType::Array:
			{
				const auto& arr = value.AsArray();
				WriteUInt32(static_cast<uint32_t>(arr.size()));
				for (const auto& item : arr)
					WriteRpcValue(item);
				break;
			}
			case RpcValueType::Object:
			{
				const auto& obj = value.AsObject();
				WriteUInt32(static_cast<uint32_t>(obj.size()));
				for (const auto& kv : obj)
				{
					WriteString(kv.first);
					WriteRpcValue(kv.second);
				}
				break;
			}
			default:
				throw std::runtime_error("Unknown RpcValueType");
			}
		}

		const std::vector<uint8_t>& ByteBufferWriter::GetBuffer() const { return m_buffer; }

		ByteBufferReader::ByteBufferReader(const std::vector<uint8_t>& buffer)
			: m_buffer(buffer), m_offset(0)
		{
		}

		bool ByteBufferReader::ReadUInt8(uint8_t& value)
		{
			if (m_offset + 1 > m_buffer.size()) return false;
			value = m_buffer[m_offset++];
			return true;
		}

		bool ByteBufferReader::ReadUInt32(uint32_t& value)
		{
			if (m_offset + 4 > m_buffer.size()) return false;
			value = 0;
			for (int i = 0; i < 4; ++i)
				value |= static_cast<uint32_t>(m_buffer[m_offset + i]) << (i * 8);
			m_offset += 4;
			return true;
		}

		bool ByteBufferReader::ReadUInt64(uint64_t& value)
		{
			if (m_offset + 8 > m_buffer.size()) return false;
			value = 0;
			for (int i = 0; i < 8; ++i)
				value |= static_cast<uint64_t>(m_buffer[m_offset + i]) << (i * 8);
			m_offset += 8;
			return true;
		}

		bool ByteBufferReader::ReadInt32(int32_t& value)
		{
			uint32_t v = 0;
			if (!ReadUInt32(v)) return false;
			value = static_cast<int32_t>(v);
			return true;
		}

		bool ByteBufferReader::ReadInt64(int64_t& value)
		{
			uint64_t v = 0;
			if (!ReadUInt64(v)) return false;
			value = static_cast<int64_t>(v);
			return true;
		}

		bool ByteBufferReader::ReadBool(bool& value)
		{
			uint8_t v = 0;
			if (!ReadUInt8(v)) return false;
			value = (v != 0);
			return true;
		}

		bool ByteBufferReader::ReadDouble(double& value)
		{
			uint64_t u = 0;
			if (!ReadUInt64(u)) return false;
			union { double d; uint64_t u; } tmp{};
			tmp.u = u;
			value = tmp.d;
			return true;
		}

		bool ByteBufferReader::ReadString(std::string& value)
		{
			uint32_t len = 0;
			if (!ReadUInt32(len)) return false;
			if (m_offset + len > m_buffer.size()) return false;
			value.assign(reinterpret_cast<const char*>(m_buffer.data() + m_offset), len);
			m_offset += len;
			return true;
		}

		bool ByteBufferReader::ReadWString(std::wstring& value)
		{
			uint32_t charCount = 0;
			if (!ReadUInt32(charCount)) return false;
			size_t byteCount = static_cast<size_t>(charCount) * sizeof(wchar_t);
			if (m_offset + byteCount > m_buffer.size()) return false;
			value.assign(reinterpret_cast<const wchar_t*>(m_buffer.data() + m_offset), charCount);
			m_offset += byteCount;
			return true;
		}

		bool ByteBufferReader::ReadBinary(RpcBinary& value)
		{
			uint32_t len = 0;
			if (!ReadUInt32(len)) return false;
			if (m_offset + len > m_buffer.size()) return false;
			value.assign(m_buffer.begin() + m_offset, m_buffer.begin() + m_offset + len);
			m_offset += len;
			return true;
		}

		bool ByteBufferReader::ReadRpcValue(RpcValue& value)
		{
			uint8_t typeRaw = 0;
			if (!ReadUInt8(typeRaw)) return false;

			RpcValueType type = static_cast<RpcValueType>(typeRaw);
			switch (type)
			{
			case RpcValueType::Null:
				value = RpcValue(nullptr);
				return true;
			case RpcValueType::Bool:
			{
				bool v = false;
				if (!ReadBool(v)) return false;
				value = RpcValue(v);
				return true;
			}
			case RpcValueType::Int32:
			{
				int32_t v = 0;
				if (!ReadInt32(v)) return false;
				value = RpcValue(v);
				return true;
			}
			case RpcValueType::Int64:
			{
				int64_t v = 0;
				if (!ReadInt64(v)) return false;
				value = RpcValue(v);
				return true;
			}
			case RpcValueType::UInt32:
			{
				uint32_t v = 0;
				if (!ReadUInt32(v)) return false;
				value = RpcValue(v);
				return true;
			}
			case RpcValueType::UInt64:
			{
				uint64_t v = 0;
				if (!ReadUInt64(v)) return false;
				value = RpcValue(v);
				return true;
			}
			case RpcValueType::Double:
			{
				double v = 0.0;
				if (!ReadDouble(v)) return false;
				value = RpcValue(v);
				return true;
			}
			case RpcValueType::String:
			{
				std::string v;
				if (!ReadString(v)) return false;
				value = RpcValue(v);
				return true;
			}
			case RpcValueType::WString:
			{
				std::wstring v;
				if (!ReadWString(v)) return false;
				value = RpcValue(v);
				return true;
			}
			case RpcValueType::Binary:
			{
				RpcBinary v;
				if (!ReadBinary(v)) return false;
				value = RpcValue(v);
				return true;
			}
			case RpcValueType::Array:
			{
				uint32_t count = 0;
				if (!ReadUInt32(count)) return false;
				RpcArray arr;
				arr.reserve(count);
				for (uint32_t i = 0; i < count; ++i)
				{
					RpcValue item;
					if (!ReadRpcValue(item)) return false;
					arr.push_back(item);
				}
				value = RpcValue(arr);
				return true;
			}
			case RpcValueType::Object:
			{
				uint32_t count = 0;
				if (!ReadUInt32(count)) return false;
				RpcObject obj;
				for (uint32_t i = 0; i < count; ++i)
				{
					std::string key;
					RpcValue item;
					if (!ReadString(key)) return false;
					if (!ReadRpcValue(item)) return false;
					obj.emplace(key, item);
				}
				value = RpcValue(obj);
				return true;
			}
			default:
				return false;
			}
		}

		std::vector<uint8_t> SerializeRequest(const RpcRequest& request)
		{
			ByteBufferWriter w;
			w.WriteString(request.functionName);

			w.WriteUInt32(static_cast<uint32_t>(request.args.size()));
			for (const auto& arg : request.args)
				w.WriteRpcValue(arg);

			w.WriteInt64(request.timestamp);
			w.WriteString(request.nonce);
			w.WriteString(request.signature);
			return w.GetBuffer();
		}

		bool DeserializeRequest(const std::vector<uint8_t>& data, RpcRequest& request)
		{
			ByteBufferReader r(data);

			if (!r.ReadString(request.functionName)) return false;

			uint32_t argCount = 0;
			if (!r.ReadUInt32(argCount)) return false;

			request.args.clear();
			request.args.reserve(argCount);
			for (uint32_t i = 0; i < argCount; ++i)
			{
				RpcValue v;
				if (!r.ReadRpcValue(v)) return false;
				request.args.push_back(v);
			}

			if (!r.ReadInt64(request.timestamp)) return false;
			if (!r.ReadString(request.nonce)) return false;
			if (!r.ReadString(request.signature)) return false;

			return true;
		}

		std::vector<uint8_t> SerializeResult(const RpcResult& result)
		{
			ByteBufferWriter w;
			w.WriteBool(result.success);
			w.WriteUInt32(static_cast<uint32_t>(result.returnValues.size()));
			for (const auto& v : result.returnValues)
				w.WriteRpcValue(v);
			w.WriteString(result.errorMessage);
			return w.GetBuffer();
		}

		bool DeserializeResult(const std::vector<uint8_t>& data, RpcResult& result)
		{
			ByteBufferReader r(data);
			if (!r.ReadBool(result.success)) return false;

			uint32_t count = 0;
			if (!r.ReadUInt32(count)) return false;

			result.returnValues.clear();
			result.returnValues.reserve(count);
			for (uint32_t i = 0; i < count; ++i)
			{
				RpcValue v;
				if (!r.ReadRpcValue(v)) return false;
				result.returnValues.push_back(v);
			}

			if (!r.ReadString(result.errorMessage)) return false;
			return true;
		}

		std::string BuildCanonicalRequestText(const RpcRequest& request)
		{
			std::ostringstream oss;
			oss << "func=" << request.functionName << ";";
			oss << "ts=" << request.timestamp << ";";
			oss << "nonce=" << request.nonce << ";";
			oss << "argc=" << request.args.size() << ";";

			std::function<void(const RpcValue&)> dumpValue = [&](const RpcValue& v)
				{
					oss << "type=" << static_cast<int>(v.GetType()) << ";";
					switch (v.GetType())
					{
					case RpcValueType::Null:
						oss << "null;";
						break;
					case RpcValueType::Bool:
						oss << (v.AsBool() ? "true;" : "false;");
						break;
					case RpcValueType::Int32:
						oss << v.AsInt32() << ";";
						break;
					case RpcValueType::Int64:
						oss << v.AsInt64() << ";";
						break;
					case RpcValueType::UInt32:
						oss << v.AsUInt32() << ";";
						break;
					case RpcValueType::UInt64:
						oss << v.AsUInt64() << ";";
						break;
					case RpcValueType::Double:
						oss << v.AsDouble() << ";";
						break;
					case RpcValueType::String:
						oss << v.AsString().size() << ":" << v.AsString() << ";";
						break;
					case RpcValueType::WString:
					{
						auto s = WideToUtf8(v.AsWString());
						oss << s.size() << ":" << s << ";";
						break;
					}
					case RpcValueType::Binary:
						oss << "binlen=" << v.AsBinary().size() << ";";
						break;
					case RpcValueType::Array:
						oss << "arrcount=" << v.AsArray().size() << ";";
						for (const auto& x : v.AsArray()) dumpValue(x);
						break;
					case RpcValueType::Object:
						oss << "objcount=" << v.AsObject().size() << ";";
						for (const auto& kv : v.AsObject())
						{
							oss << kv.first << "=";
							dumpValue(kv.second);
						}
						break;
					}
				};

			for (const auto& arg : request.args)
				dumpValue(arg);

			return oss.str();
		}

		bool WriteMessageToPipe(HANDLE hPipe, const std::vector<uint8_t>& data)
		{
			DWORD written = 0;
			uint32_t len = static_cast<uint32_t>(data.size());

			if (!WriteFile(hPipe, &len, sizeof(len), &written, nullptr) || written != sizeof(len))
				return false;

			if (len == 0)
				return FlushFileBuffers(hPipe) != FALSE;

			if (!WriteFile(hPipe, data.data(), len, &written, nullptr) || written != len)
				return false;

			return FlushFileBuffers(hPipe) != FALSE;
		}

		bool ReadMessageFromPipe(HANDLE hPipe, std::vector<uint8_t>& data)
		{
			DWORD readBytes = 0;
			uint32_t len = 0;

			if (!ReadFile(hPipe, &len, sizeof(len), &readBytes, nullptr) || readBytes != sizeof(len))
				return false;

			data.clear();
			data.resize(len);

			if (len == 0)
				return true;

			if (!ReadFile(hPipe, data.data(), len, &readBytes, nullptr) || readBytes != len)
				return false;

			return true;
		}

		std::wstring Utf8ToWide(const std::string& str)
		{
			if (str.empty()) return L"";
			int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
			if (len <= 0) return L"";
			std::wstring out(len - 1, L'\0');
			MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &out[0], len);
			return out;
		}

		std::string WideToUtf8(const std::wstring& str)
		{
			if (str.empty()) return "";
			int len = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, nullptr, 0, nullptr, nullptr);
			if (len <= 0) return "";
			std::string out(len - 1, '\0');
			WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, &out[0], len, nullptr, nullptr);
			return out;
		}

		int64_t GetCurrentUnixTimestamp()
		{
			return static_cast<int64_t>(std::time(nullptr));
		}

		std::string GenerateNonce()
		{
			static thread_local std::mt19937_64 rng(std::random_device{}());
			std::uniform_int_distribution<uint64_t> dist;

			uint64_t a = dist(rng);
			uint64_t b = dist(rng);
			char buf[64] = {};
			sprintf_s(buf, "%016llx%016llx",
				static_cast<unsigned long long>(a),
				static_cast<unsigned long long>(b));
			return std::string(buf);
		}

		std::wstring ToLowerString(const std::wstring& s)
		{
			std::wstring r = s;
			std::transform(r.begin(), r.end(), r.begin(), towlower);
			return r;
		}
	}
}

