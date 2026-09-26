#pragma once
#include "RpcCommon.h"
#include <sal.h>
#include <stdexcept>

namespace ytpp::client_server {
/// @brief 演示自定义结构体与RpcValue之间的适配。
struct UserInfo {
    std::int32_t id = 0;
    std::wstring name;
    bool enabled = false;
    double score = 0.0;
};

/// @brief 将UserInfo转换为可传输的RPC对象。
/// @param[in] user 要转换的用户信息。
/// @return 包含全部UserInfo字段的RpcValue对象。
inline RpcValue ToRpcValue(_In_ const UserInfo& user) {
    RpcObject obj;
    /// @brief 调用 RpcValue 完成对应操作。
    obj["id"] = RpcValue(user.id);
    /// @brief 调用 RpcValue 完成对应操作。
    obj["name"] = RpcValue(user.name);
    /// @brief 调用 RpcValue 完成对应操作。
    obj["enabled"] = RpcValue(user.enabled);
    /// @brief 调用 RpcValue 完成对应操作。
    obj["score"] = RpcValue(user.score);
    return RpcValue(obj);
}

/// @brief 将RPC对象转换为UserInfo。
/// @param[in] value 包含UserInfo字段的RpcValue对象。
/// @return 解析后的UserInfo。
/// @throws std::runtime_error 输入不是RPC对象时抛出。
inline UserInfo ToUserInfo(_In_ const RpcValue& value) {
    if (!value.IsObject())
        throw std::runtime_error("UserInfo must be RpcObject.");

    /// @brief 调用 AsObject 完成对应操作。
    const RpcObject& obj = value.AsObject();

    UserInfo u;
    /// @brief 调用 at 完成对应操作。
    u.id = obj.at("id").AsInt32();
    /// @brief 调用 at 完成对应操作。
    u.name = obj.at("name").AsWString();
    /// @brief 调用 at 完成对应操作。
    u.enabled = obj.at("enabled").AsBool();
    /// @brief 调用 at 完成对应操作。
    u.score = obj.at("score").AsDouble();
    return u;
}
} // namespace ytpp::client_server
