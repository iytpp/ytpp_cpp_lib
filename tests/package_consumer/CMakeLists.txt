cmake_minimum_required(VERSION 3.25)
project(ytpp_package_consumer LANGUAGES CXX)

find_package(ytpp_cpp_lib CONFIG REQUIRED COMPONENTS client_server)

add_executable(ytpp_package_consumer main.cpp)
target_link_libraries(ytpp_package_consumer PRIVATE ytpp::client_server)
