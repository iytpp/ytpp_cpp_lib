cmake_minimum_required(VERSION 3.25)

if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
        "$<IF:$<CONFIG:Debug,RelWithDebInfo>,EditAndContinue,ProgramDatabase>")
endif()

project(ytpp_cpp_lib VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "ytpp_cpp_lib currently supports Windows only.")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(YTPP_BUILD_EXAMPLES "Build ytpp_cpp_lib examples" OFF)
option(YTPP_BUILD_TEST_APP "Build the launch_for_test application" ON)

find_package(CURL REQUIRED)
find_package(jsoncpp CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

include(cmake/ytpp_compile_options.cmake)

add_subdirectory(ytpp_librarys)

if(YTPP_BUILD_EXAMPLES)
    add_subdirectory(examples/client-server)
endif()

if(YTPP_BUILD_TEST_APP)
    add_subdirectory(tests/launch_for_test)
endif()

install(
    TARGETS
        ytpp_client_server
        ytpp_curl_ex
        ytpp_jsoncpp_ex
        ytpp_sys_core
        ytpp_all
    EXPORT ytpp_cpp_lib-targets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

set(YTPP_CMAKE_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/ytpp_cpp_lib")

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ytpp_cpp_lib-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ytpp_cpp_lib-config.cmake"
    INSTALL_DESTINATION "${YTPP_CMAKE_INSTALL_DIR}"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ytpp_cpp_lib-config-version.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
)

install(
    EXPORT ytpp_cpp_lib-targets
    FILE ytpp_cpp_lib-targets.cmake
    NAMESPACE ytpp::
    DESTINATION "${YTPP_CMAKE_INSTALL_DIR}"
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/ytpp_cpp_lib-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/ytpp_cpp_lib-config-version.cmake"
    DESTINATION "${YTPP_CMAKE_INSTALL_DIR}"
)
