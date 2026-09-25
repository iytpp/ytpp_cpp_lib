set(curl_ex_source_root "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_library(ytpp_curl_ex_objects OBJECT src/curl_ex/curl_ex.cpp)
ytpp_configure_object_target(ytpp_curl_ex_objects)
target_include_directories(ytpp_curl_ex_objects PRIVATE "${curl_ex_source_root}")
target_link_libraries(ytpp_curl_ex_objects PRIVATE CURL::libcurl)

add_library(ytpp_curl_ex STATIC $<TARGET_OBJECTS:ytpp_curl_ex_objects>)
add_library(ytpp::curl_ex ALIAS ytpp_curl_ex)
ytpp_configure_library_target(
    ytpp_curl_ex
    curl_ex
    curl_ex
    "${curl_ex_source_root}"
)
target_link_libraries(ytpp_curl_ex PUBLIC CURL::libcurl ws2_32 crypt32)

install(
    DIRECTORY "${curl_ex_source_root}/curl_ex"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)
