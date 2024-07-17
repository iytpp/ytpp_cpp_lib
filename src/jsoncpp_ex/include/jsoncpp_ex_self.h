【！！！该文件已弃用！！！】
/*
* jsoncpp_ex_self.h
* author: 樱桃屁屁
* CreateDate: 2024-7-17
* Description: 用于兼容不同版本的 jsoncpp 静态库，但是由于操作性不太好，直接弃用
*/


#ifndef JSONCPP_EX_SELF_H
#define JSONCPP_EX_SELF_H



#ifdef _WIN64

#ifdef _DEBUG
#pragma comment(lib, "./x64/debug/jsoncpp_static.lib")
#else
#pragma comment(lib, "./x64/release/jsoncpp_static.lib")
#endif // _DEBUG


#else

#ifdef _DEBUG
#pragma comment(lib, "./x86/debug/jsoncpp_static.lib")
#else
#pragma comment(lib, "./x86/release/jsoncpp_static.lib")
#endif // _DEBUG

#endif // _WIN64




#endif // JSONCPP_EX_SELF_H