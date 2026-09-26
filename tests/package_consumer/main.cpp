#include <client-server/RPC-named-pip/RpcCommon.h>
#include <client-server/RPC-named-pip/RpcSecurity.h>

int main() {
    ytpp::client_server::RpcRequest request;
    request.functionName = "PackageConsumer";
    request.timestamp = 1;
    request.nonce = "nonce";
    return ytpp::client_server::RpcSecurity::MakeSignature(request, "secret").empty() ? 1 : 0;
}
