#include <iostream>
#include "client-server/RPC-named-pip/SampleStructs.h" // 结构体适配例子
#include "client-server/RPC-named-pip/NamedPipeRpcServer.h" // 服务端头文件
#include "client-server/RPC-named-pip/NamedPipeRpcClient.h" // 客户端头文件
using namespace ytpp::client_server;


namespace ytpp::client_server
{
	// 服务端的使用例子
	int example_main_server()
	{
		const std::wstring pipeName = L"\\\\.\\pipe\\Company.ProductName.Rpc.v2"; // 命名管道的名称
		const std::string sharedSecret = "Replace_With_Your_Strong_Secret"; // 共享密钥

		RpcLogger logger;
		logger.Open(L"server_rpc.log", LogLevel::Debug);

		NamedPipeRpcServer server(pipeName, sharedSecret, 8);
		server.SetLogger(&logger);

		// ACL：只允许 SYSTEM、管理员、Authenticated Users 访问管道
		// 更严可以继续收紧
		server.SetPipeSecuritySddl(
			L"D:P"
			L"(A;;GA;;;SY)"
			L"(A;;GA;;;BA)"
			L"(A;;GRGW;;;AU)"
		);

		// 白名单进程（不添加白名单就无法连接和传输数据）
		server.AddWhitelistProcess(L"C:\\Demo\\RpcClient.exe", PermissionLevel::User);
		server.AddWhitelistProcess(L"C:\\Demo\\RpcAdminClient.exe", PermissionLevel::Admin);

		// 注册函数例子
		// Add(int32, int32) -> int32
		server.RegisterFunction("Add", PermissionLevel::User,
			[](const RpcCallContext&, const RpcArray& args) -> RpcResult
			{
				if (args.size() != 2) throw std::runtime_error("Add requires 2 arguments.");
				int32_t a = args[0].AsInt32();
				int32_t b = args[1].AsInt32();

				RpcResult r;
				r.success = true;
				r.returnValues.push_back(RpcValue(a + b));
				return r;
			});

		// Concat(wstring, wstring) -> wstring
		server.RegisterFunction("Concat", PermissionLevel::User,
			[](const RpcCallContext&, const RpcArray& args) -> RpcResult
			{
				if (args.size() != 2) throw std::runtime_error("Concat requires 2 arguments.");
				std::wstring a = args[0].AsWString();
				std::wstring b = args[1].AsWString();

				RpcResult r;
				r.success = true;
				r.returnValues.push_back(RpcValue(a + b));
				return r;
			});

		// GetPair() -> int32, wstring
		server.RegisterFunction("GetPair", PermissionLevel::User,
			[](const RpcCallContext&, const RpcArray&) -> RpcResult
			{
				RpcResult r;
				r.success = true;
				r.returnValues.push_back(RpcValue(int32_t(123)));
				r.returnValues.push_back(RpcValue(std::wstring(L"第二个返回值")));
				return r;
			});

		// EchoUser(UserInfo) -> UserInfo, bool
		server.RegisterFunction("EchoUser", PermissionLevel::User,
			[](const RpcCallContext&, const RpcArray& args) -> RpcResult
			{
				if (args.size() != 1) throw std::runtime_error("EchoUser requires 1 argument.");

				UserInfo u = FromRpcValueToUserInfo(args[0]);
				u.name += L"_server";
				u.score += 10.5;

				RpcResult r;
				r.success = true;
				r.returnValues.push_back(ToRpcValue(u));
				r.returnValues.push_back(RpcValue(true));
				return r;
			});

		// AdminOnly() -> wstring
		server.RegisterFunction("AdminOnly", PermissionLevel::Admin,
			[](const RpcCallContext& ctx, const RpcArray&) -> RpcResult
			{
				RpcResult r;
				r.success = true;
				r.returnValues.push_back(
					RpcValue(std::wstring(L"管理员调用成功，PID=") + std::to_wstring(ctx.clientProcessId))
				);
				return r;
			});

		if (!server.Start())
		{
			std::cout << "Server start failed.\n";
			return 1;
		}

		std::cout << "Server started.\n";
		std::cout << "Press Enter to exit...\n";
		std::cin.get();

		server.Stop();
		return 0;
	}

	// 客户端的使用例子
	int example_main_client()
	{
		const std::wstring pipeName = L"\\\\.\\pipe\\Company.ProductName.Rpc.v2"; // 命名管道的名称
		const std::string sharedSecret = "Replace_With_Your_Strong_Secret"; // 共享密钥

		// 连接超时默认 2000ms，连接池最小 2，最大 16
		NamedPipeRpcClient client(pipeName, sharedSecret, 2000, 2, 16);

		{
			RpcResult r;
			std::string err;
			bool ok = client.Call("Add",
				{
					RpcValue(int32_t(100)),
					RpcValue(int32_t(200))
				}, r, err);
			if (ok)
				std::wcout << L"Add result = " << r.returnValues[0].AsInt32() << std::endl;
			else
				std::cout << "Add failed: " << err << std::endl;
		}

		{
			RpcResult r;
			std::string err;
			bool ok = client.Call("Concat",
				{
					RpcValue(std::wstring(L"你好，")),
					RpcValue(std::wstring(L"世界"))
				}, r, err);

			if (ok)
				std::wcout << L"Concat result = " << r.returnValues[0].AsWString() << std::endl;
			else
				std::cout << "Concat failed: " << err << std::endl;
		}

		{
			RpcResult r;
			std::string err;
			bool ok = client.Call("GetPair", {}, r, err);

			if (ok)
			{
				int32_t a = r.returnValues[0].AsInt32();
				std::wstring b = r.returnValues[1].AsWString();
				std::wcout << L"GetPair result: " << a << L", " << b << std::endl;
			}
			else
			{
				std::cout << "GetPair failed: " << err << std::endl;
			}
		}

		{
			UserInfo u;
			u.id = 7;
			u.name = L"张三";
			u.enabled = true;
			u.score = 88.8;

			RpcResult r;
			std::string err;
			bool ok = client.Call("EchoUser", { ToRpcValue(u) }, r, err);
			if (ok)
			{
				UserInfo out = FromRpcValueToUserInfo(r.returnValues[0]);
				bool flag = r.returnValues[1].AsBool();

				std::wcout << L"EchoUser result: id=" << out.id
					<< L", name=" << out.name
					<< L", enabled=" << (out.enabled ? L"true" : L"false")
					<< L", score=" << out.score
					<< L", flag=" << (flag ? L"true" : L"false")
					<< std::endl;
			}
			else
			{
				std::cout << "EchoUser failed: " << err << std::endl;
			}
		}

		{
			auto fut = client.CallAsync("Concat",
				{
					RpcValue(std::wstring(L"异步")),
					RpcValue(std::wstring(L"调用"))
				});

			RpcAsyncResult ret = fut.get();
			if (ret.ok)
			{
				std::wcout << L"Async result = " << ret.result.returnValues[0].AsWString() << std::endl;
			}
			else
			{
				std::cout << "Async failed: " << ret.errorMessage << std::endl;
			}
		}

		{
			RpcResult r;
			std::string err;
			bool ok = client.Call("AdminOnly", {}, r, err);
			if (ok)
				std::wcout << L"AdminOnly result = " << r.returnValues[0].AsWString() << std::endl;
			else
				std::cout << "AdminOnly failed: " << err << std::endl;
		}

		return 0;
	}
}

