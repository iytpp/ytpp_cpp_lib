#include "client-server/RPC-named-pip/RpcSecurity.h"
#include <bcrypt.h>
#include <iomanip>
#include <limits>
#include <sstream>
#include <vector>
#include <windows.h>

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

namespace ytpp::client_server {
namespace {
std::string BytesToHex(_In_reads_bytes_(size) const BYTE* data, _In_ DWORD size) {
    std::ostringstream stream;
    for (DWORD i = 0; i < size; ++i)
        stream << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(data[i]);
    return stream.str();
}

bool ConstantTimeEqual(_In_ const std::string& left, _In_ const std::string& right) noexcept {
    if (left.size() != right.size())
        return false;
    unsigned char difference = 0;
    for (std::size_t i = 0; i < left.size(); ++i)
        difference |= static_cast<unsigned char>(left[i]) ^ static_cast<unsigned char>(right[i]);
    return difference == 0;
}
} // namespace

std::string RpcSecurity::Sha256Hex(_In_ const std::string& text) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    DWORD cbData = 0;
    DWORD hashObjectSize = 0;
    DWORD hashSize = 0;

    NTSTATUS status = ::BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    if (!NT_SUCCESS(status))
        return "";

    status = ::BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&hashObjectSize),
                                 sizeof(hashObjectSize), &cbData, 0);
    if (!NT_SUCCESS(status)) {
        ::BCryptCloseAlgorithmProvider(hAlg, 0);
        return "";
    }

    status = ::BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashSize), sizeof(hashSize),
                                 &cbData, 0);
    if (!NT_SUCCESS(status)) {
        ::BCryptCloseAlgorithmProvider(hAlg, 0);
        return "";
    }

    std::vector<BYTE> hashObject(hashObjectSize);
    std::vector<BYTE> hash(hashSize);

    status = ::BCryptCreateHash(hAlg, &hHash, hashObject.data(), hashObjectSize, nullptr, 0, 0);
    if (!NT_SUCCESS(status)) {
        ::BCryptCloseAlgorithmProvider(hAlg, 0);
        return "";
    }

    status = ::BCryptHashData(hHash, reinterpret_cast<PUCHAR>(const_cast<char*>(text.data())),
                              static_cast<ULONG>(text.size()), 0);
    if (!NT_SUCCESS(status)) {
        ::BCryptDestroyHash(hHash);
        ::BCryptCloseAlgorithmProvider(hAlg, 0);
        return "";
    }

    status = ::BCryptFinishHash(hHash, hash.data(), hashSize, 0);
    ::BCryptDestroyHash(hHash);
    ::BCryptCloseAlgorithmProvider(hAlg, 0);
    if (!NT_SUCCESS(status))
        return "";

    return BytesToHex(hash.data(), hashSize);
}

std::string RpcSecurity::MakeSignature(_In_ const RpcRequest& request, _In_ const std::string& secret) {
    if (secret.empty() || secret.size() > (std::numeric_limits<ULONG>::max)())
        return {};

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD bytesWritten = 0;
    DWORD objectSize = 0;
    DWORD hashSize = 0;
    NTSTATUS status = ::BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr,
                                                    BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (!NT_SUCCESS(status))
        return {};
    status = ::BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectSize),
                                 sizeof(objectSize), &bytesWritten, 0);
    if (NT_SUCCESS(status))
        status = ::BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashSize),
                                     sizeof(hashSize), &bytesWritten, 0);

    std::vector<BYTE> object(objectSize);
    std::vector<BYTE> digest(hashSize);
    if (NT_SUCCESS(status)) {
        status = ::BCryptCreateHash(algorithm, &hash, object.data(), objectSize,
                                    reinterpret_cast<PUCHAR>(const_cast<char*>(secret.data())),
                                    static_cast<ULONG>(secret.size()), 0);
    }
    const std::string payload = BuildCanonicalRequestText(request);
    if (payload.size() > (std::numeric_limits<ULONG>::max)()) {
        ::BCryptDestroyHash(hash);
        ::BCryptCloseAlgorithmProvider(algorithm, 0);
        return {};
    }
    if (NT_SUCCESS(status)) {
        status = ::BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(payload.data())),
                                  static_cast<ULONG>(payload.size()), 0);
    }
    if (NT_SUCCESS(status))
        status = ::BCryptFinishHash(hash, digest.data(), hashSize, 0);

    if (hash)
        ::BCryptDestroyHash(hash);
    ::BCryptCloseAlgorithmProvider(algorithm, 0);
    return NT_SUCCESS(status) ? BytesToHex(digest.data(), hashSize) : std::string{};
}

bool RpcSecurity::VerifySignature(_In_ const RpcRequest& request, _In_ const std::string& secret) {
    if (secret.empty() || request.signature.empty())
        return false;
    const std::string expected = MakeSignature(request, secret);
    return !expected.empty() && ConstantTimeEqual(expected, request.signature);
}
} // namespace ytpp::client_server
