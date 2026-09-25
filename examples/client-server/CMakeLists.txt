add_library(ytpp_client_server_examples OBJECT EXCLUDE_FROM_ALL
    Examples_RPC-named-pip.cpp
)
target_link_libraries(ytpp_client_server_examples PRIVATE ytpp::client_server)
