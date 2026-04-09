#pragma once
#include "RpcCommon.h"
#include <stdexcept>


namespace ytpp {
	namespace client_server
	{
		struct UserInfo
		{
			int32_t id = 0;
			std::wstring name;
			bool enabled = false;
			double score = 0.0;
		};

		inline RpcValue ToRpcValue(const UserInfo& u)
		{
			RpcObject obj;
			obj["id"] = RpcValue(u.id);
			obj["name"] = RpcValue(u.name);
			obj["enabled"] = RpcValue(u.enabled);
			obj["score"] = RpcValue(u.score);
			return RpcValue(obj);
		}

		inline UserInfo FromRpcValueToUserInfo(const RpcValue& v)
		{
			if (!v.IsObject())
				throw std::runtime_error("UserInfo must be RpcObject.");

			const RpcObject& obj = v.AsObject();

			UserInfo u;
			u.id = obj.at("id").AsInt32();
			u.name = obj.at("name").AsWString();
			u.enabled = obj.at("enabled").AsBool();
			u.score = obj.at("score").AsDouble();
			return u;
		}
	}
}

