#ifndef _SYS_CORE_ENCODING_H_
#define _SYS_CORE_ENCODING_H_

#include <string>
#include <windows.h>


namespace ytpp {
	using namespace std;

	//=================== ANSI & UTF-8 ===================
	/*
	* @brief 将ANSI编码的string字符串转换为UTF-8编码的string字符串
	* @param [in] str: ANSI编码的string字符串
	* @return (string) UTF-8编码的string字符串
	*/
	string encoding_ANSI_to_UTF8( _In_ const string& str );
	/*
	* @brief 将UTF-8编码的string字符串转换为ANSI编码的string字符串
	* @param [in] str: UTF-8编码的string字符串
	* @return (string) ANSI编码的string字符串
	*/
	string encoding_UTF8_to_ANSI( _In_ const string& str );



	//=================== ANSI & UTF-16 ===================
	/*
	* @brief 将ANSI编码的string转换为UTF-16编码的wstring宽字符串
	* @param [in] str: ANSI编码的string字符串
	* @return (wstring) UTF-16编码的wstring宽字符串
	*/
	wstring encoding_ANSIstring_to_Widestring( _In_ const string& str );
	/*
	* @brief 将UTF-16编码的wstring宽字符串转换为ANSI编码的string字符串
	* @param [in] str: UTF-16编码的wstring宽字符串
	* @return (string) ANSI编码的string字符串
	*/
	string encoding_Widestring_to_ANSIstring( _In_ const wstring& str );

	//=================== UTF-8 & UTF-16 ===================
	/*
	* @brief 将UTF-8编码的string转换为 UTF-16编码的wstring宽字符串
	* @param [in] str: UTF-8编码的string字符串
	* @return (wstring) UTF-16编码的wstring宽字符串
	*/
	wstring encoding_UTF8string_to_Widestring( _In_ const string& str );
	/*
	* @brief 将UTF-16编码的wstring宽字符串转换为UTF-8编码的string字符串
	* @param [in] str: UTF-16编码的wstring宽字符串
	* @return (string) UTF-8编码的string字符串
	*/
	string encoding_Widestring_to_UTF8string( _In_ const wstring& str );


	
}



#endif /* _SYS_CORE_ENCODING_H_ */