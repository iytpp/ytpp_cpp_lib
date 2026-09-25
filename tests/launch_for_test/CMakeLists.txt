add_executable(launch_for_test launch.cpp launch.h)
target_link_libraries(launch_for_test PRIVATE
    ytpp::curl_ex
    ytpp::jsoncpp_ex
    ytpp::sys_core
)
