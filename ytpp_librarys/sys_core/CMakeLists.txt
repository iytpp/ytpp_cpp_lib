set(sys_core_source_root "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(sys_core_sources
    src/sys_core/date_time.cpp
    src/sys_core/disk_manipulation.cpp
    src/sys_core/encoding.cpp
    src/sys_core/encryption.cpp
    src/sys_core/environment.cpp
    src/sys_core/hash.cpp
    src/sys_core/machine_feature.cpp
    src/sys_core/string_ex.cpp
    src/sys_core/sys_processing.cpp
)

add_library(ytpp_sys_core_objects OBJECT ${sys_core_sources})
ytpp_configure_object_target(ytpp_sys_core_objects)
target_include_directories(ytpp_sys_core_objects PRIVATE "${sys_core_source_root}")
target_link_libraries(ytpp_sys_core_objects PRIVATE OpenSSL::Crypto OpenSSL::SSL)

add_library(ytpp_sys_core STATIC $<TARGET_OBJECTS:ytpp_sys_core_objects>)
add_library(ytpp::sys_core ALIAS ytpp_sys_core)
ytpp_configure_library_target(
    ytpp_sys_core
    sys_core
    sys_core
    "${sys_core_source_root}"
)
target_link_libraries(ytpp_sys_core PUBLIC
    OpenSSL::Crypto
    OpenSSL::SSL
    wbemuuid
    crypt32
    ws2_32
    iphlpapi
    winhttp
)

install(
    DIRECTORY "${sys_core_source_root}/sys_core"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)
