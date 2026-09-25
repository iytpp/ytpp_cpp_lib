set(client_server_source_root "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(client_server_sources
    src/client-server/RPC-named-pip/NamedPipeRpcClient.cpp
    src/client-server/RPC-named-pip/NamedPipeRpcServer.cpp
    src/client-server/RPC-named-pip/RpcCommon.cpp
    src/client-server/RPC-named-pip/RpcLogger.cpp
    src/client-server/RPC-named-pip/RpcSecurity.cpp
)

add_library(ytpp_client_server_objects OBJECT ${client_server_sources})
ytpp_configure_object_target(ytpp_client_server_objects)
target_include_directories(ytpp_client_server_objects PRIVATE "${client_server_source_root}")

add_library(ytpp_client_server STATIC $<TARGET_OBJECTS:ytpp_client_server_objects>)
add_library(ytpp::client_server ALIAS ytpp_client_server)
ytpp_configure_library_target(
    ytpp_client_server
    client_server
    client-server
    "${client_server_source_root}"
)
target_link_libraries(ytpp_client_server PUBLIC bcrypt advapi32)

install(
    DIRECTORY "${client_server_source_root}/client-server"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)
