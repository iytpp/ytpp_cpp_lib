#pragma once
#include "RpcCommon.h"
#include <string>

#pragma comment(lib, "bcrypt.lib")

namespace ytpp {
	namespace client_server
	{
		class RpcSecurity
		{
		public:
			static std::string Sha256Hex(const std::string& text);
			static std::string MakeSignature(const RpcRequest& request, const std::string& secret);
			static bool VerifySignature(const RpcRequest& request, const std::string& secret);
		};
	}
}


