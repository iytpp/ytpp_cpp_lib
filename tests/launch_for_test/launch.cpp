#include "client-server/RPC-named-pip/RpcCommon.h"
#include "client-server/RPC-named-pip/NamedPipeRpcClient.h"
#include "client-server/RPC-named-pip/NamedPipeRpcServer.h"
#include "client-server/RPC-named-pip/RpcSecurity.h"
#include "sys_core/disk_manipulation.h"
#include "sys_core/encryption.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include <windows.h>

namespace {

void Require(_In_ bool condition, _In_ const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

void TestRpcSignatureCoversBinaryContent() {
    ytpp::client_server::RpcRequest first;
    first.functionName = "Upload";
    first.args.emplace_back(ytpp::client_server::RpcBinary{1, 2, 3});
    first.timestamp = 123;
    first.nonce = "nonce";

    ytpp::client_server::RpcRequest second = first;
    second.args[0] = ytpp::client_server::RpcValue(ytpp::client_server::RpcBinary{3, 2, 1});
    const std::string firstSignature = ytpp::client_server::RpcSecurity::MakeSignature(first, "test-secret");
    const std::string secondSignature = ytpp::client_server::RpcSecurity::MakeSignature(second, "test-secret");
    Require(!firstSignature.empty(), "RPC HMAC must not be empty");
    Require(firstSignature != secondSignature, "RPC HMAC must cover binary content");
    first.signature = firstSignature;
    Require(ytpp::client_server::RpcSecurity::VerifySignature(first, "test-secret"), "valid RPC HMAC was rejected");
    Require(!ytpp::client_server::RpcSecurity::VerifySignature(first, "wrong-secret"), "invalid RPC HMAC was accepted");
}

void TestRpcDeserializerRejectsInvalidFrames() {
    ytpp::client_server::RpcRequest request;
    request.functionName = "Ping";
    request.timestamp = 123;
    request.nonce = "nonce";
    request.signature = "signature";
    auto bytes = ytpp::client_server::SerializeRequest(request);
    bytes.push_back(0xFF);
    ytpp::client_server::RpcRequest decoded;
    Require(!ytpp::client_server::DeserializeRequest(bytes, decoded), "RPC request with trailing data was accepted");

    ytpp::client_server::ByteBufferWriter writer;
    writer.WriteString("Ping");
    writer.WriteUInt32(ytpp::client_server::kRpcMaximumCollectionItems + 1);
    Require(!ytpp::client_server::DeserializeRequest(writer.GetBuffer(), decoded),
            "RPC request with excessive argument count was accepted");
}

void TestEmptyWriteTruncatesFile(_In_ const std::filesystem::path& directory) {
    const auto path = directory / "empty-write.bin";
    {
        std::ofstream stream(path, std::ios::binary);
        stream << "old-data";
    }
    Require(ytpp::sys_core::disk_manipulation::WriteDataToFile(path.string(), std::vector<char>{}),
            "empty file write failed");
    Require(std::filesystem::file_size(path) == 0, "empty file write did not truncate the destination");
}

void TestAuthenticatedDecryptDoesNotPublishInvalidPlaintext(_In_ const std::filesystem::path& directory) {
    const auto input = directory / "plain.txt";
    const auto container = directory / "encrypted.ytpp";
    const auto output = directory / "output.txt";
    {
        std::ofstream stream(input, std::ios::binary);
        stream << "authenticated plaintext";
    }
    {
        std::ofstream stream(output, std::ios::binary);
        stream << "preserve-me";
    }
    ytpp::sys_core::encryption::FileCrypto::EncryptFileToContainerWithPasswordAes256GcmPbkdf2(
        input.string(), container.string(), "correct-password", {}, 1000, 4096);

    bool rejected = false;
    try {
        ytpp::sys_core::encryption::FileCrypto::DecryptFileFromContainerWithPasswordAes256GcmPbkdf2(
            container.string(), output.string(), "wrong-password", 4096);
    } catch (const ytpp::sys_core::encryption::OpenSslException&) {
        rejected = true;
    }
    Require(rejected, "container with wrong password was accepted");
    std::ifstream stream(output, std::ios::binary);
    const std::string content((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    Require(content == "preserve-me", "failed authentication modified the destination file");
}

std::wstring GetCurrentExecutablePath() {
    std::wstring path(32768, L'\0');
    const DWORD length = ::GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    Require(length > 0 && length < path.size(), "failed to resolve test executable path");
    path.resize(length);
    return path;
}

void TestNamedPipeBusinessFailureAndShutdown() {
    const std::wstring pipeName = L"\\\\.\\pipe\\ytpp-test-" + std::to_wstring(::GetCurrentProcessId());
    ytpp::client_server::NamedPipeRpcServer invalidServer(pipeName + L"-invalid", "secret", 1);
    invalidServer.SetPipeSecuritySddl(L"this is not valid SDDL");
    Require(!invalidServer.Start(), "server accepted invalid SDDL");

    ytpp::client_server::NamedPipeRpcServer server(pipeName, "secret", 1);
    server.AddWhitelistProcess(GetCurrentExecutablePath(), ytpp::client_server::PermissionLevel::User);
    server.RegisterFunction("BusinessFailure", ytpp::client_server::PermissionLevel::User,
                            [](const ytpp::client_server::RpcCallContext&, const ytpp::client_server::RpcArray&) {
                                ytpp::client_server::RpcResult result;
                                result.success = false;
                                result.errorMessage = "expected business failure";
                                return result;
                            });
    Require(server.Start(), "failed to start named-pipe test server");

    ytpp::client_server::NamedPipeRpcClient client(pipeName, "secret", 2000, 1, 1);
    ytpp::client_server::RpcResult result;
    std::string transportError;
    if (!client.Call("BusinessFailure", {}, result, transportError))
        throw std::runtime_error("business failure was incorrectly reported as a transport failure: " +
                                 transportError);
    Require(!result.success && result.errorMessage == "expected business failure",
            "business failure payload was not preserved");
    Require(transportError.empty(), "business failure populated the transport error");
    server.Stop();
}

} // namespace

int main() {
    try {
        const auto testDirectory = std::filesystem::temp_directory_path() / "ytpp-cpp-lib-tests";
        std::filesystem::create_directories(testDirectory);
        TestRpcSignatureCoversBinaryContent();
        TestRpcDeserializerRejectsInvalidFrames();
        TestEmptyWriteTruncatesFile(testDirectory);
        TestAuthenticatedDecryptDoesNotPublishInvalidPlaintext(testDirectory);
        TestNamedPipeBusinessFailureAndShutdown();
        std::filesystem::remove_all(testDirectory);
        std::cout << "All ytpp_cpp_lib tests passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Test failure: " << exception.what() << '\n';
        return 1;
    }
}
