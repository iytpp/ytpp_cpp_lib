add_executable(launch_for_test launch.cpp launch.h)
target_link_libraries(launch_for_test PRIVATE
    ytpp::client_server
    ytpp::curl_ex
    ytpp::jsoncpp_ex
    ytpp::sys_core
)

add_test(NAME ytpp_core_tests COMMAND launch_for_test)
set_tests_properties(ytpp_core_tests PROPERTIES TIMEOUT 15)
