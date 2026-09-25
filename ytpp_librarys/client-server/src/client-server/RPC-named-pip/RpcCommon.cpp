#include "client-server/RPC-named-pip/RpcCommon.h"
#include <algorithm>
#include <ctime>
#include <functional>
#include <random>
#include <sstream>
#include <stdexcept>

namespace ytpp::client_server {
RpcValue::RpcValue() : value_(_In_ std::monostate{}) {}

RpcValue::RpcValue(_In_ std::nullptr_t) : value_(_In_ std::monostate{}) {}

RpcValue::RpcValue(_In_ bool v) : value_(v) {}

RpcValue::RpcValue(_In_ std::int32_t v) : value_(v) {}

RpcValue::RpcValue(_In_ std::int64_t v) : value_(v) {}

RpcValue::RpcValue(_In_ std::uint32_t v) : value_(v) {}

RpcValue::RpcValue(_In_ std::uint64_t v) : value_(v) {}

RpcValue::RpcValue(_In_ double v) : value_(v) {}

RpcValue::RpcValue(_In_ const std::string& v) : value_(v) {}

RpcValue::RpcValue(_In_ const char* v) : value_(std::string(v ? v : "")) {}

RpcValue::RpcValue(_In_ const std::wstring& v) : value_(v) {}

RpcValue::RpcValue(_In_ const wchar_t* v) : value_(std::wstring(v ? v : L"")) {}

RpcValue::RpcValue(_In_ const RpcBinary& v) : value_(v) {}

RpcValue::RpcValue(_In_ const RpcArray& v) : value_(v) {}

RpcValue::RpcValue(_In_ const RpcObject& v) : value_(v) {}

RpcValueType RpcValue::GetType() const {
    if (std::holds_alternative<std::monostate>(value_))
        return RpcValueType::Null;
    if (std::holds_alternative<bool>(value_))
        return RpcValueType::Bool;
    if (std::holds_alternative<std::int32_t>(value_))
        return RpcValueType::Int32;
    if (std::holds_alternative<std::int64_t>(value_))
        return RpcValueType::Int64;
    if (std::holds_alternative<std::uint32_t>(value_))
        return RpcValueType::UInt32;
    if (std::holds_alternative<std::uint64_t>(value_))
        return RpcValueType::UInt64;
    if (std::holds_alternative<double>(value_))
        return RpcValueType::Double;
    if (std::holds_alternative<std::string>(value_))
        return RpcValueType::String;
    if (std::holds_alternative<std::wstring>(value_))
        return RpcValueType::WString;
    if (std::holds_alternative<RpcBinary>(value_))
        return RpcValueType::Binary;
    if (std::holds_alternative<RpcArray>(value_))
        return RpcValueType::Array;
    return RpcValueType::Object;
}

bool RpcValue::IsNull() const {
    return std::holds_alternative<std::monostate>(value_);
}

bool RpcValue::IsBool() const {
    return std::holds_alternative<bool>(value_);
}

bool RpcValue::IsInt32() const {
    return std::holds_alternative<std::int32_t>(value_);
}

bool RpcValue::IsInt64() const {
    return std::holds_alternative<std::int64_t>(value_);
}

bool RpcValue::IsUInt32() const {
    return std::holds_alternative<std::uint32_t>(value_);
}

bool RpcValue::IsUInt64() const {
    return std::holds_alternative<std::uint64_t>(value_);
}

bool RpcValue::IsDouble() const {
    return std::holds_alternative<double>(value_);
}

bool RpcValue::IsString() const {
    return std::holds_alternative<std::string>(value_);
}

bool RpcValue::IsWString() const {
    return std::holds_alternative<std::wstring>(value_);
}

bool RpcValue::IsBinary() const {
    return std::holds_alternative<RpcBinary>(value_);
}

bool RpcValue::IsArray() const {
    return std::holds_alternative<RpcArray>(value_);
}

bool RpcValue::IsObject() const {
    return std::holds_alternative<RpcObject>(value_);
}

bool RpcValue::AsBool() const {
    return std::get<bool>(value_);
}

std::int32_t RpcValue::AsInt32() const {
    return std::get<std::int32_t>(value_);
}

std::int64_t RpcValue::AsInt64() const {
    return std::get<std::int64_t>(value_);
}

std::uint32_t RpcValue::AsUInt32() const {
    return std::get<std::uint32_t>(value_);
}

std::uint64_t RpcValue::AsUInt64() const {
    return std::get<std::uint64_t>(value_);
}

double RpcValue::AsDouble() const {
    return std::get<double>(value_);
}

const std::string& RpcValue::AsString() const {
    return std::get<std::string>(value_);
}

const std::wstring& RpcValue::AsWString() const {
    return std::get<std::wstring>(value_);
}

const RpcBinary& RpcValue::AsBinary() const {
    return std::get<RpcBinary>(value_);
}

const RpcArray& RpcValue::AsArray() const {
    return std::get<RpcArray>(value_);
}

const RpcObject& RpcValue::AsObject() const {
    return std::get<RpcObject>(value_);
}

RpcArray& RpcValue::AsArray() {
    return std::get<RpcArray>(value_);
}

RpcObject& RpcValue::AsObject() {
    return std::get<RpcObject>(value_);
}

const RpcValue::VariantType& RpcValue::GetRaw() const {
    return value_;
}

void ByteBufferWriter::WriteUInt8(_In_ std::uint8_t value) {
    buffer_.push_back(value);
}

void ByteBufferWriter::WriteUInt32(_In_ std::uint32_t value) {
    for (int i = 0; i < 4; ++i)
        buffer_.push_back(static_cast<std::uint8_t>((value >> (i * 8)) & 0xFF));
}

void ByteBufferWriter::WriteUInt64(_In_ std::uint64_t value) {
    for (int i = 0; i < 8; ++i)
        buffer_.push_back(static_cast<std::uint8_t>((value >> (i * 8)) & 0xFF));
}

void ByteBufferWriter::WriteInt32(_In_ std::int32_t value) {
    WriteUInt32(static_cast<std::uint32_t>(value));
}

void ByteBufferWriter::WriteInt64(_In_ std::int64_t value) {
    WriteUInt64(static_cast<std::uint64_t>(value));
}

void ByteBufferWriter::WriteBool(_In_ bool value) {
    WriteUInt8(value ? 1 : 0);
}

void ByteBufferWriter::WriteDouble(_In_ double value) {
    static_assert(sizeof(double) == 8, "double must be 8 bytes");

    union {
        double d;
        std::uint64_t u;
    } u{};

    u.d = value;
    WriteUInt64(u.u);
}

void ByteBufferWriter::WriteString(_In_ const std::string& value) {
    WriteUInt32(static_cast<std::uint32_t>(value.size()));
    buffer_.insert(buffer_.end(), value.begin(), value.end());
}

void ByteBufferWriter::WriteWString(_In_ const std::wstring& value) {
    WriteUInt32(static_cast<std::uint32_t>(value.size()));
    const std::uint8_t* p = reinterpret_cast<const std::uint8_t*>(value.data());
    buffer_.insert(buffer_.end(), p, p + value.size() * sizeof(wchar_t));
}

void ByteBufferWriter::WriteBinary(_In_ const RpcBinary& value) {
    WriteUInt32(static_cast<std::uint32_t>(value.size()));
    buffer_.insert(buffer_.end(), value.begin(), value.end());
}

void ByteBufferWriter::WriteRpcValue(_In_ const RpcValue& value) {
    WriteUInt8(static_cast<std::uint8_t>(value.GetType()));

    switch (value.GetType()) {
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
    case RpcValueType::Array: {
        const auto& arr = value.AsArray();
        WriteUInt32(static_cast<std::uint32_t>(arr.size()));
        for (const auto& item : arr)
            WriteRpcValue(item);
        break;
    }
    case RpcValueType::Object: {
        const auto& obj = value.AsObject();
        WriteUInt32(static_cast<std::uint32_t>(obj.size()));
        for (const auto& kv : obj) {
            WriteString(kv.first);
            WriteRpcValue(kv.second);
        }
        break;
    }
    default:
        throw std::runtime_error("Unknown RpcValueType");
    }
}

const std::vector<std::uint8_t>& ByteBufferWriter::GetBuffer() const {
    return buffer_;
}

ByteBufferReader::ByteBufferReader(_In_ const std::vector<std::uint8_t>& buffer) : buffer_(buffer), offset_(0) {}

bool ByteBufferReader::ReadUInt8(_Out_ std::uint8_t& value) {
    if (offset_ + 1 > buffer_.size())
        return false;
    value = buffer_[offset_++];
    return true;
}

bool ByteBufferReader::ReadUInt32(_Out_ std::uint32_t& value) {
    if (offset_ + 4 > buffer_.size())
        return false;
    value = 0;
    for (int i = 0; i < 4; ++i)
        value |= static_cast<std::uint32_t>(buffer_[offset_ + i]) << (i * 8);
    offset_ += 4;
    return true;
}

bool ByteBufferReader::ReadUInt64(_Out_ std::uint64_t& value) {
    if (offset_ + 8 > buffer_.size())
        return false;
    value = 0;
    for (int i = 0; i < 8; ++i)
        value |= static_cast<std::uint64_t>(buffer_[offset_ + i]) << (i * 8);
    offset_ += 8;
    return true;
}

bool ByteBufferReader::ReadInt32(_Out_ std::int32_t& value) {
    std::uint32_t v = 0;
    if (!ReadUInt32(v))
        return false;
    value = static_cast<std::int32_t>(v);
    return true;
}

bool ByteBufferReader::ReadInt64(_Out_ std::int64_t& value) {
    std::uint64_t v = 0;
    if (!ReadUInt64(v))
        return false;
    value = static_cast<std::int64_t>(v);
    return true;
}

bool ByteBufferReader::ReadBool(_Out_ bool& value) {
    std::uint8_t v = 0;
    if (!ReadUInt8(v))
        return false;
    value = (v != 0);
    return true;
}

bool ByteBufferReader::ReadDouble(_Out_ double& value) {
    std::uint64_t u = 0;
    if (!ReadUInt64(u))
        return false;

    union {
        double d;
        std::uint64_t u;
    } tmp{};

    tmp.u = u;
    value = tmp.d;
    return true;
}

bool ByteBufferReader::ReadString(_Out_ std::string& value) {
    std::uint32_t len = 0;
    if (!ReadUInt32(len))
        return false;
    if (offset_ + len > buffer_.size())
        return false;
    value.assign(reinterpret_cast<const char*>(buffer_.data() + offset_), len);
    offset_ += len;
    return true;
}

bool ByteBufferReader::ReadWString(_Out_ std::wstring& value) {
    std::uint32_t charCount = 0;
    if (!ReadUInt32(charCount))
        return false;
    std::size_t byteCount = static_cast<std::size_t>(charCount) * sizeof(wchar_t);
    if (offset_ + byteCount > buffer_.size())
        return false;
    value.assign(reinterpret_cast<const wchar_t*>(buffer_.data() + offset_), charCount);
    offset_ += byteCount;
    return true;
}

bool ByteBufferReader::ReadBinary(_Out_ RpcBinary& value) {
    std::uint32_t len = 0;
    if (!ReadUInt32(len))
        return false;
    if (offset_ + len > buffer_.size())
        return false;
    value.assign(buffer_.begin() + offset_, buffer_.begin() + offset_ + len);
    offset_ += len;
    return true;
}

bool ByteBufferReader::ReadRpcValue(_Out_ RpcValue& value) {
    std::uint8_t typeRaw = 0;
    if (!ReadUInt8(typeRaw))
        return false;

    RpcValueType type = static_cast<RpcValueType>(typeRaw);
    switch (type) {
    case RpcValueType::Null:
        value = RpcValue(nullptr);
        return true;
    case RpcValueType::Bool: {
        bool v = false;
        if (!ReadBool(v))
            return false;
        value = RpcValue(v);
        return true;
    }
    case RpcValueType::Int32: {
        std::int32_t v = 0;
        if (!ReadInt32(v))
            return false;
        value = RpcValue(v);
        return true;
    }
    case RpcValueType::Int64: {
        std::int64_t v = 0;
        if (!ReadInt64(v))
            return false;
        value = RpcValue(v);
        return true;
    }
    case RpcValueType::UInt32: {
        std::uint32_t v = 0;
        if (!ReadUInt32(v))
            return false;
        value = RpcValue(v);
        return true;
    }
    case RpcValueType::UInt64: {
        std::uint64_t v = 0;
        if (!ReadUInt64(v))
            return false;
        value = RpcValue(v);
        return true;
    }
    case RpcValueType::Double: {
        double v = 0.0;
        if (!ReadDouble(v))
            return false;
        value = RpcValue(v);
        return true;
    }
    case RpcValueType::String: {
        std::string v;
        if (!ReadString(v))
            return false;
        value = RpcValue(v);
        return true;
    }
    case RpcValueType::WString: {
        std::wstring v;
        if (!ReadWString(v))
            return false;
        value = RpcValue(v);
        return true;
    }
    case RpcValueType::Binary: {
        RpcBinary v;
        if (!ReadBinary(v))
            return false;
        value = RpcValue(v);
        return true;
    }
    case RpcValueType::Array: {
        std::uint32_t count = 0;
        if (!ReadUInt32(count))
            return false;
        RpcArray arr;
        arr.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            RpcValue item;
            if (!ReadRpcValue(item))
                return false;
            arr.push_back(item);
        }
        value = RpcValue(arr);
        return true;
    }
    case RpcValueType::Object: {
        std::uint32_t count = 0;
        if (!ReadUInt32(count))
            return false;
        RpcObject obj;
        for (std::uint32_t i = 0; i < count; ++i) {
            std::string key;
            RpcValue item;
            if (!ReadString(key))
                return false;
            if (!ReadRpcValue(item))
                return false;
            obj.emplace(key, item);
        }
        value = RpcValue(obj);
        return true;
    }
    default:
        return false;
    }
}

std::vector<std::uint8_t> SerializeRequest(_In_ const RpcRequest& request) {
    ByteBufferWriter w;
    w.WriteString(request.functionName);

    w.WriteUInt32(static_cast<std::uint32_t>(request.args.size()));
    for (const auto& arg : request.args)
        w.WriteRpcValue(arg);

    w.WriteInt64(request.timestamp);
    w.WriteString(request.nonce);
    w.WriteString(request.signature);
    return w.GetBuffer();
}

bool DeserializeRequest(_In_ const std::vector<std::uint8_t>& data, _Out_ RpcRequest& request) {
    ByteBufferReader r(data);

    if (!r.ReadString(request.functionName))
        return false;

    std::uint32_t argCount = 0;
    if (!r.ReadUInt32(argCount))
        return false;

    request.args.clear();
    request.args.reserve(argCount);
    for (std::uint32_t i = 0; i < argCount; ++i) {
        RpcValue v;
        if (!r.ReadRpcValue(v))
            return false;
        request.args.push_back(v);
    }

    if (!r.ReadInt64(request.timestamp))
        return false;
    if (!r.ReadString(request.nonce))
        return false;
    if (!r.ReadString(request.signature))
        return false;

    return true;
}

std::vector<std::uint8_t> SerializeResult(_In_ const RpcResult& result) {
    ByteBufferWriter w;
    w.WriteBool(result.success);
    w.WriteUInt32(static_cast<std::uint32_t>(result.returnValues.size()));
    for (const auto& v : result.returnValues)
        w.WriteRpcValue(v);
    w.WriteString(result.errorMessage);
    return w.GetBuffer();
}

bool DeserializeResult(_In_ const std::vector<std::uint8_t>& data, _Out_ RpcResult& result) {
    ByteBufferReader r(data);
    if (!r.ReadBool(result.success))
        return false;

    std::uint32_t count = 0;
    if (!r.ReadUInt32(count))
        return false;

    result.returnValues.clear();
    result.returnValues.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        RpcValue v;
        if (!r.ReadRpcValue(v))
            return false;
        result.returnValues.push_back(v);
    }

    if (!r.ReadString(result.errorMessage))
        return false;
    return true;
}

std::string BuildCanonicalRequestText(_In_ const RpcRequest& request) {
    std::ostringstream oss;
    oss << "func=" << request.functionName << ";";
    oss << "ts=" << request.timestamp << ";";
    oss << "nonce=" << request.nonce << ";";
    oss << "argc=" << request.args.size() << ";";

    std::function<void(const RpcValue&)> dumpValue = [&](const RpcValue& v) {
        oss << "type=" << static_cast<int>(v.GetType()) << ";";
        switch (v.GetType()) {
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
        case RpcValueType::WString: {
            auto s = WideToUtf8(v.AsWString());
            oss << s.size() << ":" << s << ";";
            break;
        }
        case RpcValueType::Binary:
            oss << "binlen=" << v.AsBinary().size() << ";";
            break;
        case RpcValueType::Array:
            oss << "arrcount=" << v.AsArray().size() << ";";
            for (const auto& x : v.AsArray())
                dumpValue(x);
            break;
        case RpcValueType::Object:
            oss << "objcount=" << v.AsObject().size() << ";";
            for (const auto& kv : v.AsObject()) {
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

bool WriteMessageToPipe(_In_ HANDLE hPipe, _In_ const std::vector<std::uint8_t>& data) {
    DWORD written = 0;
    std::uint32_t len = static_cast<std::uint32_t>(data.size());

    if (!::WriteFile(hPipe, &len, sizeof(len), &written, nullptr) || written != sizeof(len))
        return false;

    if (len == 0)
        return ::FlushFileBuffers(hPipe) != FALSE;

    if (!::WriteFile(hPipe, data.data(), len, &written, nullptr) || written != len)
        return false;

    return ::FlushFileBuffers(hPipe) != FALSE;
}

bool ReadMessageFromPipe(_In_ HANDLE hPipe, _Out_ std::vector<std::uint8_t>& data) {
    DWORD readBytes = 0;
    std::uint32_t len = 0;

    if (!::ReadFile(hPipe, &len, sizeof(len), &readBytes, nullptr) || readBytes != sizeof(len))
        return false;

    data.clear();
    data.resize(len);

    if (len == 0)
        return true;

    if (!::ReadFile(hPipe, data.data(), len, &readBytes, nullptr) || readBytes != len)
        return false;

    return true;
}

std::wstring Utf8ToWide(_In_ const std::string& str) {
    if (str.empty())
        return L"";
    int len = ::MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (len <= 0)
        return L"";
    std::wstring out(len - 1, L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &out[0], len);
    return out;
}

std::string WideToUtf8(_In_ const std::wstring& str) {
    if (str.empty())
        return "";
    int len = ::WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0)
        return "";
    std::string out(len - 1, '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, &out[0], len, nullptr, nullptr);
    return out;
}

std::int64_t GetCurrentUnixTimestamp() {
    return static_cast<std::int64_t>(std::time(nullptr));
}

std::string GenerateNonce() {
    static thread_local std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<std::uint64_t> dist;

    std::uint64_t a = dist(rng);
    std::uint64_t b = dist(rng);
    char buf[64] = {};
    sprintf_s(buf, "%016llx%016llx", static_cast<unsigned long long>(a), static_cast<unsigned long long>(b));
    return std::string(buf);
}

std::wstring ToLower(_In_ const std::wstring& s) {
    std::wstring r = s;
    std::transform(r.begin(), r.end(), r.begin(), towlower);
    return r;
}
} // namespace ytpp::client_server
