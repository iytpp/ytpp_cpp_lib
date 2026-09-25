add_library(ytpp_all STATIC
    $<TARGET_OBJECTS:ytpp_client_server_objects>
    $<TARGET_OBJECTS:ytpp_curl_ex_objects>
    $<TARGET_OBJECTS:ytpp_jsoncpp_ex_objects>
    $<TARGET_OBJECTS:ytpp_sys_core_objects>
)
add_library(ytpp::all ALIAS ytpp_all)

set_target_properties(ytpp_all PROPERTIES
    EXPORT_NAME all
    OUTPUT_NAME ytpp_cpp_lib
)

target_compile_features(ytpp_all INTERFACE cxx_std_23)
target_compile_definitions(ytpp_all INTERFACE UNICODE _UNICODE)
if(MSVC)
    target_compile_options(ytpp_all INTERFACE /utf-8 /permissive- /Zc:__cplusplus)
endif()
target_include_directories(ytpp_all PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/ytpp_librarys/client-server/src>"
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/ytpp_librarys/curl_ex/src>"
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/ytpp_librarys/jsoncpp_ex/src>"
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/ytpp_librarys/sys_core/src>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_link_libraries(ytpp_all PUBLIC
    CURL::libcurl
    JsonCpp::JsonCpp
    OpenSSL::Crypto
    OpenSSL::SSL
    bcrypt
    advapi32
    wbemuuid
    crypt32
    ws2_32
    iphlpapi
    winhttp
)
