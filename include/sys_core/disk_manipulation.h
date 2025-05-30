﻿#ifndef _SYS_CORE_DISK_MANIPULATION_H_
#define _SYS_CORE_DISK_MANIPULATION_H_

#include <fstream>
#include <string>
#include <vector>
#include <windows.h>

#ifdef _UNICODE

#define file_isExists file_isExistsW
#define write_to_file write_to_fileW
#define write_resource_file write_resource_fileW

#else /* ANSI */

#define file_isExists file_isExistsA
#define write_to_file write_to_fileA
#define write_resource_file write_resource_fileA

#endif /* _UNICODE */

namespace ytpp {
    namespace sys_core 
	{

		using namespace std;

		//======================================================================
		/*
		* @brief 判断文件是否存在
		* @brief 类似于易语言中的 "文件是否存在" 函数
		* @param [in] fileName: 文件名
		* @return true 文件存在，false 文件不存在
		*/
		bool file_isExistsA(
			_In_ const string & fileName
		);

		/*
		* @brief 判断文件是否存在
		* @brief 类似于易语言中的 "文件是否存在" 函数
		* @param [in] fileName: 文件名
		* @return true 文件存在，false 文件不存在
		*/
		bool file_isExistsW(
			_In_ const wstring & fileName
		);

		/*
		* @brief 将vector数据以二进制的形式写入文件，如果文件不存在则创建文件，如果文件存在则覆盖文件内容
		* @brief 类似于易语言中的 "写到文件" 函数
		* @param [in] fileName: 文件名
		* @param [in] data: 欲写到文件中的数据
		* @return true 写入成功，false 写入失败
		*/
		bool write_to_fileA(
			_In_ const string & fileName,
			_In_ const vector<char>&data
		);
		/*
		* @brief 将vector数据以二进制的形式写入文件，如果文件不存在则创建文件，如果文件存在则覆盖文件内容
		* @brief 类似于易语言中的 "写到文件" 函数
		* @param [in] fileName: 文件名
		* @param [in] data: 欲写到文件中的数据
		* @return true 写入成功，false 写入失败
		*/
		bool write_to_fileW(
			_In_ const wstring & fileName,
			_In_ const vector<char>&data
		);

		/*
		* @brief 将char数据以二进制形式写入文件，如果文件不存在则创建文件，如果文件存在则覆盖文件内容
		* @brief 类似于易语言中的 "写到文件" 函数
		* @param [in] fileName: 文件名
		* @param [in] data: 欲写到文件中的数据，当为NULL时，终止写入并返回true
		* @param [in] size: 数据大小，当为NULL时，终止写入并返回true
		* @return true 写入成功，false 写入失败
		*/
		bool write_to_fileA(
			_In_ const string & fileName,
			_In_opt_ const char* data,
			_In_opt_ size_t size
		);

		/*
		* @brief 将char数据以二进制形式写入文件，如果文件不存在则创建文件，如果文件存在则覆盖文件内容
		* @brief 类似于易语言中的 "写到文件" 函数
		* @param [in] fileName: 文件名
		* @param [in] data: 欲写到文件中的数据，当为NULL时，终止写入并返回true
		* @param [in] size: 数据大小，当为NULL时，终止写入并返回true
		* @return true 写入成功，false 写入失败
		*/
		bool write_to_fileW(
			_In_ const wstring & fileName,
			_In_opt_ const char* data,
			_In_opt_ size_t size
		);

		/*
		* @brief 将字符串以文本形式写入文件，如果文件不存在则创建文件，如果文件存在则覆盖文件内容
		* @brief data采用ANSI编码和UTF8编码均可，可调用 encoding_ 前缀的编码函数进行编码转换。
		* @brief 类似于易语言中的 "写到文件" 函数
		* @param [in] fileName: 文件名
		* @param [in] data: 欲写到文件中的数据
		* @return true 写入成功，false 写入失败
		*/
		bool write_to_fileA(
			_In_ const string & fileName,
			_In_ const string & data
		);
		/*
		* @brief 将字符串以文本形式写入文件，如果文件不存在则创建文件，如果文件存在则覆盖文件内容
		* @brief data采用ANSI编码和UTF8编码均可，可调用 encoding_ 前缀的编码函数进行编码转换。
		* @brief 类似于易语言中的 "写到文件" 函数
		* @param [in] fileName: 文件名
		* @param [in] data: 欲写到文件中的数据
		* @return true 写入成功，false 写入失败
		*/
		bool write_to_fileW(
			_In_ const wstring & fileName,
			_In_ const string & data
		);

		/*
		* @brief 将字符串以文本形式写入文件，如果文件不存在则创建文件，如果文件存在则覆盖文件内容
		* @brief data采用wstring，可调用 encoding_ 前缀的编码函数进行编码转换。
		* @brief 类似于易语言中的 "写到文件" 函数
		* @param [in] fileName: 文件名
		* @param [in] data: 欲写到文件中的数据
		* @return true 写入成功，false 写入失败
		*/
		bool write_to_fileA(
			_In_ const string & fileName,
			_In_ const wstring & data
		);

		/*
		* @brief 将字符串以文本形式写入文件，如果文件不存在则创建文件，如果文件存在则覆盖文件内容
		* @brief data采用wstring，可调用 encoding_ 前缀的编码函数进行编码转换。
		* @brief 类似于易语言中的 "写到文件" 函数
		* @param [in] fileName: 文件名
		* @param [in] data: 欲写到文件中的数据
		* @return true 写入成功，false 写入失败
		*/
		bool write_to_fileW(
			_In_ const wstring & fileName,
			_In_ const wstring & data
		);

		/*
		* @brief 将自定义资源写出到指定路径
		* @param [in] hModule: 模块句柄，从哪个模块中提取资源
		* @param [in] resID: 资源ID，通常在resource.h中定义
		* @param [in] resType: 资源类型名
		* @param [in] outPath: 输出路径
		* @return true 写入成功，false 写入失败
		*/
		bool write_resource_fileA(
			_In_ HMODULE hModule,
			_In_ int resID,
			_In_ string resType,
			_In_ string outPath
		);

		/*
		* @brief 将自定义资源写出到指定路径，宽字符版本
		* @param [in] hModule: 模块句柄，从哪个模块中提取资源
		* @param [in] resID: 资源ID，通常在resource.h中定义
		* @param [in] resType: 资源类型名
		* @param [in] outPath: 输出路径
		* @return true 写入成功，false 写入失败
		*/
		bool write_resource_fileW(
			_In_ HMODULE hModule,
			_In_ int resID,
			_In_ wstring resType,
			_In_ wstring outPath
		);
	}
}


#endif /* _SYS_CORE_DISK_MANIPULATION_H_ */