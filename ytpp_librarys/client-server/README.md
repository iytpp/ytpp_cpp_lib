# client-server 使用手册

`client-server` 提供 Windows 命名管道 RPC：客户端连接池、服务端工作线程、动态参数模型、二进制序列化、请求签名、时间戳与 Nonce 防重放、进程白名单、权限控制和文件日志。

## 引入方式

```cmake
find_package(ytpp_cpp_lib CONFIG REQUIRED COMPONENTS client_server)
target_link_libraries(my_app PRIVATE ytpp::client_server)
```

```cpp
#include <client-server/RPC-named-pip/NamedPipeRpcClient.h>
#include <client-server/RPC-named-pip/NamedPipeRpcServer.h>
#include <client-server/RPC-named-pip/RpcLogger.h>
```

公共接口位于 `ytpp::client_server`，当前实现仅支持 Windows。

## 快速开始

服务端：

```cpp
using namespace ytpp::client_server;

NamedPipeRpcServer server(LR"(\\.\pipe\ytpp-demo)", "replace-with-a-long-random-secret");
server.AddWhitelistProcess(LR"(C:\Program Files\Demo\demo-client.exe)", PermissionLevel::User);
server.RegisterFunction("Add", PermissionLevel::User,
    [](const RpcCallContext&, const RpcArray& args) {
        RpcResult result;
        if (args.size() != 2 || !args[0].IsInt32() || !args[1].IsInt32()) {
            result.errorMessage = "Add requires two Int32 arguments.";
            return result;
        }
        result.success = true;
        result.returnValues.emplace_back(args[0].AsInt32() + args[1].AsInt32());
        return result;
    });

if (!server.Start()) {
    // 处理启动失败。
}
```

客户端：

```cpp
using namespace ytpp::client_server;

NamedPipeRpcClient client(LR"(\\.\pipe\ytpp-demo)", "replace-with-a-long-random-secret");
RpcResult result;
std::string error;
if (client.Call("Add", {RpcValue(std::int32_t{20}), RpcValue(std::int32_t{22})}, result, error) && result.success) {
    const std::int32_t sum = result.returnValues.at(0).AsInt32();
}
```

两端必须使用相同的管道名与共享密钥。

## RPC 数据模型

### `RpcValue`

支持 Null、`bool`、有符号/无符号 32/64 位整数、`double`、`std::string`、`std::wstring`、二进制、数组和对象。

```cpp
RpcValue nullValue;
RpcValue text("hello");
RpcArray array{RpcValue(true), RpcValue(std::int32_t{7})};
RpcObject object{{"name", RpcValue("demo")}, {"items", RpcValue(array)}};
```

先使用 `GetType()` 或 `IsBool()`、`IsInt32()`、`IsArray()` 等检查类型，再调用 `As...()`；类型不匹配时会抛出 `std::bad_variant_access`。

| 类型 | 定义 |
|---|---|
| `RpcArray` | `std::vector<RpcValue>` |
| `RpcObject` | `std::map<std::string, RpcValue>` |
| `RpcBinary` | `std::vector<std::uint8_t>` |

### `RpcRequest`、`RpcResult`、`RpcCallContext`

`RpcRequest` 包含函数名、参数、Unix 时间戳、Nonce 和签名，通常由客户端自动构造。

`RpcResult::success` 表示远端业务结果，`returnValues` 保存返回值，`errorMessage` 保存业务错误。`RpcCallContext` 提供客户端进程 ID、完整路径和权限，可用于审计与二次授权。

## 服务端 `NamedPipeRpcServer`

```cpp
NamedPipeRpcServer(
    const std::wstring& pipeName,
    const std::string& sharedSecret,
    std::size_t workerCount = 0);
```

- `pipeName`：建议使用完整形式 `\\.\pipe\name`。
- `sharedSecret`：请求签名密钥，应使用长随机值并安全配置。
- `workerCount`：工作线程数；`0` 表示自动选择。

`Start()` 启动服务；`Stop()` 停止服务并等待工作线程退出；析构函数负责最终回收。

### 注册函数

```cpp
server.RegisterFunction("GetProfile", PermissionLevel::User,
    [](const RpcCallContext& context, const RpcArray& args) -> RpcResult {
        RpcResult result;
        // 校验 args 数量和每个 RpcValue 的类型。
        result.success = true;
        return result;
    });
```

处理器可能在不同工作线程并发执行，捕获或访问的业务状态必须自行同步。业务代码应保证函数名唯一。

### 白名单和权限

权限从低到高为 `Guest`、`User`、`Admin`、`Super`。注册函数时指定最低权限；`AddWhitelistProcess()` 根据客户端可执行文件完整路径授予权限。

```cpp
server.AddWhitelistProcess(
    LR"(C:\Program Files\Demo\demo-client.exe)",
    PermissionLevel::Admin);
```

路径白名单不等于代码签名验证；强安全场景还应配置严格管道 ACL，并验证客户端程序签名或部署完整性。

### 管道 ACL

`SetPipeSecuritySddl()` 设置创建管道实例时使用的 SDDL。SDDL 配置错误可能导致无法启动或授权过宽，应根据实际服务账户设计，不能盲目复制示例。

### 日志

```cpp
RpcLogger logger;
if (logger.Open(L"rpc.log", LogLevel::Info)) {
    server.SetLogger(&logger);
}
```

`SetLogger()` 接收非拥有指针；调用方必须保证日志器的生命周期覆盖服务端运行期。`RpcLogger` 支持 `Debug`、`Info`、`Warn`、`Error` 和通用 `Write()`，内部使用互斥锁保护文件写入。

## 客户端 `NamedPipeRpcClient`

```cpp
NamedPipeRpcClient(
    const std::wstring& pipeName,
    const std::string& sharedSecret,
    DWORD connectTimeoutMs = 2000,
    std::size_t minimumPoolSize = 2,
    std::size_t maximumPoolSize = 16);
```

连接池供并发调用复用。`minimumPoolSize` 不应超过 `maximumPoolSize`，池上限应结合服务端工作线程数设置。`SetTimeout()` 更新后续连接和管道操作的超时值。

### 同步调用

```cpp
RpcResult result;
std::string transportError;
if (!client.Call("Ping", {}, result, transportError)) {
    // 本地连接、超时、读写或协议错误。
} else if (!result.success) {
    // 服务端业务错误：result.errorMessage。
}
```

`Call()` 的布尔返回值与 `RpcResult::success` 含义不同，必须分别判断。

### 异步调用

```cpp
std::future<RpcAsyncResult> future = client.CallAsync("Ping", {});
RpcAsyncResult asyncResult = future.get();
```

`RpcAsyncResult::ok` 表示调用流程是否完成，`result.success` 表示远端业务是否成功。异步任务仍占用池连接；不要在异步任务完成前销毁客户端。

## 签名与防重放

`RpcSecurity::MakeSignature()` 和 `VerifySignature()` 使用 HMAC-SHA256 对请求的确定性二进制表示进行认证，函数名、全部参数内容、时间戳和 Nonce 都在认证范围内。服务端同时检查时间戳偏差和 Nonce 重复。

- 不要把共享密钥写入日志或提交到源码。
- 两端系统时间需要保持同步。
- 签名提供完整性和共享密钥身份校验，但不加密消息内容。
- 访问控制仍依赖 SDDL、Windows 身份和进程白名单。
- 共享密钥不能为空；密码学操作失败或签名为空时验证失败，不会降级放行。
- 签名比较采用常量时间比较，避免普通字符串比较泄漏匹配前缀。

## 底层序列化接口

`ByteBufferWriter`、`ByteBufferReader`、`SerializeRequest()`、`DeserializeRequest()`、`SerializeResult()`、`DeserializeResult()` 主要用于协议扩展和测试。常规业务应使用客户端与服务端类。

`ByteBufferReader` 保存输入缓冲区引用，输入容器必须比读取器活得更久。所有 `Read...()` 返回成功状态，失败后不应继续使用剩余内容。

线协议版本为 2，帧包含 magic、协议版本和长度。单帧最大 16 MiB，单个集合最多 65536 项，动态值最大嵌套深度为 64；超出限制或存在尾随数据时会拒绝消息。版本 2 与旧版无 magic 帧不兼容，客户端和服务端必须同时升级。

客户端不会自动重放已经开始发送的请求。传输失败只返回本地错误，避免非幂等处理器被执行两次；需要业务重试时，应由上层结合稳定业务请求 ID 和幂等策略决定。

`WriteMessageToPipe()` / `ReadMessageFromPipe()` 处理包含协议魔数、协议版本和负载长度的消息帧，传入的 `HANDLE` 必须是有效且已连接的命名管道。

## 自定义结构体

`SampleStructs.h` 演示 `UserInfo` 与 `RpcValue` 的转换。业务结构体建议定义成对转换函数，并验证字段存在性和类型。示例使用的 `map::at()` 在字段缺失时会抛出 `std::out_of_range`，处理不受信输入时应捕获或提前检查。

## 线程安全与所有权

- 服务端注册表和白名单有内部同步；处理器自己的共享状态由业务代码保护。
- 客户端连接池支持并发调用。
- `RpcLogger` 支持多线程写入。
- `SetLogger()` 不转移所有权。
- 不要在处理器中销毁服务端，也不要在未完成异步任务时销毁客户端。

## 常见故障

| 现象 | 检查项 |
|---|---|
| 连接失败 | 管道名、服务端是否启动、ACL、连接超时 |
| 请求被拒绝 | 客户端完整路径、白名单权限、函数最低权限 |
| 签名失败 | 两端密钥、系统时间、协议版本 |
| 偶发超时 | 工作线程数、连接池上限、处理器耗时 |
| `As...()` 抛异常 | 是否先检查 `RpcValue` 类型 |
| 日志无输出 | `Open()` 结果、最低级别、日志器生命周期 |
