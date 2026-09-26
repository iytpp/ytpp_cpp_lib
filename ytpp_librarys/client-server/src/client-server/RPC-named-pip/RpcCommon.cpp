#include "client-server/RPC-named-pip/RpcCommon.h"
#include <algorithm>
#include <array>
#include <bit>
#include <bcrypt.h>
#include <cstring>
#include <ctime>
#include <limits>
#include <stdexcept>

namespace ytpp::client_server {
RpcValue::RpcValue() : value_(std::monostate{}) {}

RpcValue::RpcValue(_In_ std::nullptr_t) : value_(std::monostate{}) {}

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
    WriteUInt64(std::bit_cast<std::uint64_t>(value));
}

void ByteBufferWriter::WriteString(_In_ const std::string& value) {
    if (value.size() > (std::numeric_limits<std::uint32_t>::max)())
        throw std::length_error("RPC string exceeds the protocol limit");
    WriteUInt32(static_cast<std::uint32_t>(value.size()));
    buffer_.insert(buffer_.end(), value.begin(), value.end());
}

void ByteBufferWriter::WriteWString(_In_ const std::wstring& value) {
    if (value.size() > (std::numeric_limits<std::uint32_t>::max)())
        throw std::length_error("RPC wide string exceeds the protocol limit");
    WriteUInt32(static_cast<std::uint32_t>(value.size()));
    const std::uint8_t* p = reinterpret_cast<const std::uint8_t*>(value.data());
    buffer_.insert(buffer_.end(), p, p + value.size() * sizeof(wchar_t));
}

void ByteBufferWriter::WriteBinary(_In_ const RpcBinary& value) {
    if (value.size() > (std::numeric_limits<std::uint32_t>::max)())
        throw std::length_error("RPC binary value exceeds the protocol limit");
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
        if (arr.size() > kRpcMaximumCollectionItems)
            throw std::length_error("RPC array exceeds the protocol item limit");
        WriteUInt32(static_cast<std::uint32_t>(arr.size()));
        for (const auto& item : arr)
            WriteRpcValue(item);
        break;
    }
    case RpcValueType::Object: {
        const auto& obj = value.AsObject();
        if (obj.size() > kRpcMaximumCollectionItems)
            throw std::length_error("RPC object exceeds the protocol item limit");
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
    if (v > 1)
        return false;
    value = (v != 0);
    return true;
}

bool ByteBufferReader::ReadDouble(_Out_ double& value) {
    std::uint64_t u = 0;
    if (!ReadUInt64(u))
        return false;

    value = std::bit_cast<double>(u);
    return true;
}

bool ByteBufferReader::ReadString(_Out_ std::string& value) {
    std::uint32_t len = 0;
    if (!ReadUInt32(len))
        return false;
    if (len > buffer_.size() - offset_)
        return false;
    value.assign(reinterpret_cast<const char*>(buffer_.data() + offset_), len);
    offset_ += len;
    return true;
}

bool ByteBufferReader::ReadWString(_Out_ std::wstring& value) {
    std::uint32_t charCount = 0;
    if (!ReadUInt32(charCount))
        return false;
    if (charCount > (std::numeric_limits<std::size_t>::max)() / sizeof(wchar_t))
        return false;
    std::size_t byteCount = static_cast<std::size_t>(charCount) * sizeof(wchar_t);
    if (byteCount > buffer_.size() - offset_)
        return false;
    value.resize(charCount);
    if (byteCount != 0)
        std::memcpy(value.data(), buffer_.data() + offset_, byteCount);
    offset_ += byteCount;
    return true;
}

bool ByteBufferReader::ReadBinary(_Out_ RpcBinary& value) {
    std::uint32_t len = 0;
    if (!ReadUInt32(len))
        return false;
    if (len > buffer_.size() - offset_)
        return false;
    value.assign(buffer_.begin() + offset_, buffer_.begin() + offset_ + len);
    offset_ += len;
    return true;
}

bool ByteBufferReader::ReadRpcValue(_Out_ RpcValue& value) {
    return ReadRpcValueImpl(value, 0);
}

bool ByteBufferReader::ReadRpcValueImpl(_Out_ RpcValue& value, _In_ std::uint32_t depth) {
    if (depth > kRpcMaximumNestingDepth)
        return false;
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
        if (count > kRpcMaximumCollectionItems)
            return false;
        RpcArray arr;
        arr.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            RpcValue item;
            if (!ReadRpcValueImpl(item, depth + 1))
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
        if (count > kRpcMaximumCollectionItems)
            return false;
        RpcObject obj;
        for (std::uint32_t i = 0; i < count; ++i) {
            std::string key;
            RpcValue item;
            if (!ReadString(key))
                return false;
            if (!ReadRpcValueImpl(item, depth + 1))
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

bool ByteBufferReader::IsAtEnd() const noexcept {
    return offset_ == buffer_.size();
}

std::vector<std::uint8_t> SerializeRequest(_In_ const RpcRequest& request) {
    ByteBufferWriter w;
    w.WriteString(request.functionName);

    if (request.args.size() > kRpcMaximumCollectionItems)
        throw std::length_error("RPC request exceeds the argument limit");
    w.WriteUInt32(static_cast<std::uint32_t>(request.args.size()));
    for (const auto& arg : request.args)
        w.WriteRpcValue(arg);

    w.WriteInt64(request.timestamp);
    w.WriteString(request.nonce);
    w.WriteString(request.signature);
    return w.GetBuffer();
}

bool DeserializeRequest(_In_ const std::vector<std::uint8_t>& data, _Out_ RpcRequest& request) {
    if (data.size() > kRpcMaximumMessageSize)
        return false;
    ByteBufferReader r(data);

    if (!r.ReadString(request.functionName))
        return false;

    std::uint32_t argCount = 0;
    if (!r.ReadUInt32(argCount))
        return false;
    if (argCount > kRpcMaximumCollectionItems)
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

    return r.IsAtEnd();
}

std::vector<std::uint8_t> SerializeResult(_In_ const RpcResult& result) {
    ByteBufferWriter w;
    w.WriteBool(result.success);
    if (result.returnValues.size() > kRpcMaximumCollectionItems)
        throw std::length_error("RPC result exceeds the value limit");
    w.WriteUInt32(static_cast<std::uint32_t>(result.returnValues.size()));
    for (const auto& v : result.returnValues)
        w.WriteRpcValue(v);
    w.WriteString(result.errorMessage);
    return w.GetBuffer();
}

bool DeserializeResult(_In_ const std::vector<std::uint8_t>& data, _Out_ RpcResult& result) {
    if (data.size() > kRpcMaximumMessageSize)
        return false;
    ByteBufferReader r(data);
    if (!r.ReadBool(result.success))
        return false;

    std::uint32_t count = 0;
    if (!r.ReadUInt32(count))
        return false;
    if (count > kRpcMaximumCollectionItems)
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
    return r.IsAtEnd();
}

std::string BuildCanonicalRequestText(_In_ const RpcRequest& request) {
    RpcRequest unsignedRequest = request;
    unsignedRequest.signature.clear();
    const auto bytes = SerializeRequest(unsignedRequest);
    return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

namespace {
bool WriteAll(_In_ HANDLE pipe, _In_reads_bytes_(size) const void* data, _In_ std::size_t size) {
    const auto* current = static_cast<const std::uint8_t*>(data);
    while (size > 0) {
        DWORD written = 0;
        const DWORD chunk = static_cast<DWORD>((std::min)(size, static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())));
        if (!::WriteFile(pipe, current, chunk, &written, nullptr) || written == 0)
            return false;
        current += written;
        size -= written;
    }
    return true;
}

bool ReadExact(_In_ HANDLE pipe, _Out_writes_bytes_(size) void* data, _In_ std::size_t size) {
    auto* current = static_cast<std::uint8_t*>(data);
    while (size > 0) {
        DWORD readBytes = 0;
        const DWORD chunk = static_cast<DWORD>((std::min)(size, static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())));
        if (!::ReadFile(pipe, current, chunk, &readBytes, nullptr) || readBytes == 0)
            return false;
        current += readBytes;
        size -= readBytes;
    }
    return true;
}
} // namespace

bool WriteMessageToPipe(_In_ HANDLE hPipe, _In_ const std::vector<std::uint8_t>& data) {
    if (data.size() > kRpcMaximumMessageSize) {
        ::SetLastError(ERROR_FILE_TOO_LARGE);
        return false;
    }
    const std::uint32_t len = static_cast<std::uint32_t>(data.size());
    if (!WriteAll(hPipe, &kRpcProtocolMagic, sizeof(kRpcProtocolMagic)) ||
        !WriteAll(hPipe, &kRpcProtocolVersion, sizeof(kRpcProtocolVersion)) || !WriteAll(hPipe, &len, sizeof(len)))
        return false;

    if (len == 0)
        return ::FlushFileBuffers(hPipe) != FALSE;

    if (!WriteAll(hPipe, data.data(), len))
        return false;

    return ::FlushFileBuffers(hPipe) != FALSE;
}

bool ReadMessageFromPipe(_In_ HANDLE hPipe, _Out_ std::vector<std::uint8_t>& data) {
    std::uint32_t magic = 0;
    std::uint32_t version = 0;
    std::uint32_t len = 0;
    if (!ReadExact(hPipe, &magic, sizeof(magic)) || !ReadExact(hPipe, &version, sizeof(version)) ||
        !ReadExact(hPipe, &len, sizeof(len)))
        return false;
    if (magic != kRpcProtocolMagic || version != kRpcProtocolVersion) {
        ::SetLastError(ERROR_INVALID_DATA);
        return false;
    }
    if (len > kRpcMaximumMessageSize) {
        ::SetLastError(ERROR_FILE_TOO_LARGE);
        return false;
    }

    data.clear();
    data.resize(len);

    if (len == 0)
        return true;

    return ReadExact(hPipe, data.data(), len);
}

std::wstring Utf8ToWide(_In_ const std::string& str) {
    if (str.empty())
        return L"";
    if (str.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
        return L"";
    const int sourceLength = static_cast<int>(str.size());
    int len = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.data(), sourceLength, nullptr, 0);
    if (len <= 0)
        return L"";
    std::wstring out(static_cast<std::size_t>(len), L'\0');
    if (::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.data(), sourceLength, out.data(), len) != len)
        return L"";
    return out;
}

std::string WideToUtf8(_In_ const std::wstring& str) {
    if (str.empty())
        return "";
    if (str.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
        return "";
    const int sourceLength = static_cast<int>(str.size());
    int len = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, str.data(), sourceLength, nullptr, 0, nullptr, nullptr);
    if (len <= 0)
        return "";
    std::string out(static_cast<std::size_t>(len), '\0');
    if (::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, str.data(), sourceLength, out.data(), len, nullptr,
                              nullptr) != len)
        return "";
    return out;
}

std::int64_t GetCurrentUnixTimestamp() {
    return static_cast<std::int64_t>(std::time(nullptr));
}

std::string GenerateNonce() {
    std::array<std::uint8_t, 16> bytes{};
    if (!BCRYPT_SUCCESS(::BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()),
                                          BCRYPT_USE_SYSTEM_PREFERRED_RNG)))
        throw std::runtime_error("BCryptGenRandom failed while generating RPC nonce");
    constexpr char kHex[] = "0123456789abcdef";
    std::string nonce(bytes.size() * 2, '\0');
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        nonce[i * 2] = kHex[bytes[i] >> 4];
        nonce[i * 2 + 1] = kHex[bytes[i] & 0x0F];
    }
    return nonce;
}

std::wstring ToLower(_In_ const std::wstring& s) {
    std::wstring r = s;
    std::transform(r.begin(), r.end(), r.begin(), towlower);
    return r;
}
} // namespace ytpp::client_server
