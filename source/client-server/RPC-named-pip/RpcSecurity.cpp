#include "client-server/RPC-named-pip/RpcSecurity.h"
#include <windows.h>
#include <bcrypt.h>
#include <vector>
#include <sstream>
#include <iomanip>

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

namespace ytpp {
	namespace client_server
	{
		std::string RpcSecurity::Sha256Hex(const std::string& text)
		{
			BCRYPT_ALG_HANDLE hAlg = nullptr;
			BCRYPT_HASH_HANDLE hHash = nullptr;
			DWORD cbData = 0;
			DWORD hashObjectSize = 0;
			DWORD hashSize = 0;

			NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
			if (!NT_SUCCESS(status)) return "";

			status = BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH,
				reinterpret_cast<PUCHAR>(&hashObjectSize), sizeof(hashObjectSize), &cbData, 0);
			if (!NT_SUCCESS(status))
			{
				BCryptCloseAlgorithmProvider(hAlg, 0);
				return "";
			}

			status = BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH,
				reinterpret_cast<PUCHAR>(&hashSize), sizeof(hashSize), &cbData, 0);
			if (!NT_SUCCESS(status))
			{
				BCryptCloseAlgorithmProvider(hAlg, 0);
				return "";
			}

			std::vector<BYTE> hashObject(hashObjectSize);
			std::vector<BYTE> hash(hashSize);

			status = BCryptCreateHash(hAlg, &hHash, hashObject.data(), hashObjectSize, nullptr, 0, 0);
			if (!NT_SUCCESS(status))
			{
				BCryptCloseAlgorithmProvider(hAlg, 0);
				return "";
			}

			status = BCryptHashData(hHash,
				reinterpret_cast<PUCHAR>(const_cast<char*>(text.data())),
				static_cast<ULONG>(text.size()), 0);
			if (!NT_SUCCESS(status))
			{
				BCryptDestroyHash(hHash);
				BCryptCloseAlgorithmProvider(hAlg, 0);
				return "";
			}

			status = BCryptFinishHash(hHash, hash.data(), hashSize, 0);
			BCryptDestroyHash(hHash);
			BCryptCloseAlgorithmProvider(hAlg, 0);
			if (!NT_SUCCESS(status)) return "";

			std::ostringstream oss;
			for (BYTE b : hash)
				oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
			return oss.str();
		}

		std::string RpcSecurity::MakeSignature(const RpcRequest& request, const std::string& secret)
		{
			return Sha256Hex(secret + "|" + BuildCanonicalRequestText(request));
		}

		bool RpcSecurity::VerifySignature(const RpcRequest& request, const std::string& secret)
		{
			return MakeSignature(request, secret) == request.signature;
		}
	}
}

