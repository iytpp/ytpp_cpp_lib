#ifndef _SYS_CORE_DISK_MANIPULATION_H_
#define _SYS_CORE_DISK_MANIPULATION_H_

#include <fstream>
#include <string>
#include <vector>
#include <windows.h>
#include "compatibility.h"

namespace ytpp {
	using namespace std;
	
	//======================================================================
	/*
	* @brief 判断文件是否存在
	* @brief 类似于易语言中的 "文件是否存在" 函数
	* @param [in] fileName: 文件名
	* @return true 文件存在，false 文件不存在
	*/
	bool file_isExists(
		_In_ const _tstring& fileName );
	/*
	* @brief 将vector数据以二进制的形式写入文件，如果文件不存在则创建文件，如果文件存在则覆盖文件内容
	* @brief 类似于易语言中的 "写到文件" 函数
	* @param [in] fileName: 文件名
	* @param [in] data: 欲写到文件中的数据
	* @return true 写入成功，false 写入失败
	*/
	bool write_to_file(
		_In_ const _tstring& fileName,
		_In_ const vector<char>& data );
	/*
	* @brief 将char数据以二进制形式写入文件，如果文件不存在则创建文件，如果文件存在则覆盖文件内容
	* @brief 类似于易语言中的 "写到文件" 函数
	* @param [in] fileName: 文件名
	* @param [in] data: 欲写到文件中的数据，当为NULL时，终止写入并返回true
	* @param [in] size: 数据大小，当为NULL时，终止写入并返回true
	* @return true 写入成功，false 写入失败
	*/
	bool write_to_file(
		_In_ const _tstring& fileName,
		_In_opt_ const char* data,
		_In_opt_ size_t size );
	/*
	* @brief 将字符串以文本形式写入文件，如果文件不存在则创建文件，如果文件存在则覆盖文件内容
	* @brief data采用ANSI编码和UTF8编码均可，可调用 encoding_ 前缀的编码函数进行编码转换。
	* @brief 类似于易语言中的 "写到文件" 函数
	* @param [in] fileName: 文件名
	* @param [in] data: 欲写到文件中的数据
	* @return true 写入成功，false 写入失败
	*/
	bool write_to_file(
		_In_ const _tstring& fileName,
		_In_ const string& data );

}


#endif /* _SYS_CORE_DISK_MANIPULATION_H_ */