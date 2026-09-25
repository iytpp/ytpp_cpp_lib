set(jsoncpp_ex_source_root "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_library(ytpp_jsoncpp_ex_objects OBJECT src/jsoncpp_ex/jsoncpp_ex.cpp)
ytpp_configure_object_target(ytpp_jsoncpp_ex_objects)
target_include_directories(ytpp_jsoncpp_ex_objects PRIVATE "${jsoncpp_ex_source_root}")
target_link_libraries(ytpp_jsoncpp_ex_objects PRIVATE JsonCpp::JsonCpp)

add_library(ytpp_jsoncpp_ex STATIC $<TARGET_OBJECTS:ytpp_jsoncpp_ex_objects>)
add_library(ytpp::jsoncpp_ex ALIAS ytpp_jsoncpp_ex)
ytpp_configure_library_target(
    ytpp_jsoncpp_ex
    jsoncpp_ex
    jsoncpp_ex
    "${jsoncpp_ex_source_root}"
)
target_link_libraries(ytpp_jsoncpp_ex PUBLIC JsonCpp::JsonCpp)

install(
    DIRECTORY "${jsoncpp_ex_source_root}/jsoncpp_ex"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)
