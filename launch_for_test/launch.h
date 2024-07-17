#ifndef LAUNCH_H
#define LAUNCH_H





#ifdef _WIN64

#ifdef _DEBUG
#pragma comment(lib, "./jsoncpp_ex/static_lib/x64/Debug/jsoncpp_ex.lib")
#pragma comment(lib, "./sys_core/static_lib/x64/Debug/sys_core.lib")
#else
#pragma comment(lib, "./jsoncpp_ex/static_lib/x64/Release/jsoncpp_ex.lib")
#pragma comment(lib, "./sys_core/static_lib/x64/Release/sys_core.lib")
#endif // _DEBUG


#else

#ifdef _DEBUG
#pragma comment(lib, "./jsoncpp_ex/static_lib/Win32/Debug/jsoncpp_ex.lib")
#pragma comment(lib, "./sys_core/static_lib/Win32/Debug/sys_core.lib")
#else
#pragma comment(lib, "./jsoncpp_ex/static_lib/Win32/Release/jsoncpp_ex.lib")
#pragma comment(lib, "./sys_core/static_lib/Win32/Release/sys_core.lib")
#endif // _DEBUG

#endif // _WIN64







#endif // LAUNCH_H