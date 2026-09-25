#ifndef _SYS_CORE_DISK_MANIPULATION_H_
#define _SYS_CORE_DISK_MANIPULATION_H_

#include <fstream>
#include <string>
#include <vector>

#include <ShlObj.h>
#include <windows.h>

#ifdef _UNICODE

#define file_isExists file_isExistsW
#define write_to_file write_to_fileW
#define write_resource_file write_resource_fileW
#define GetExeDir GetExeDirW
#define GetKnownFolderPath GetKnownFolderPathW

#else /* ANSI */

#define file_isExists file_isExistsA
#define write_to_file write_to_fileA
#define write_resource_file write_resource_fileA
#define GetExeDir GetExeDirA
#define GetKnownFolderPath GetKnownFolderPathU8

#endif /* _UNICODE */

namespace ytpp {
    namespace sys_core 
	{
		using namespace std;
		//======================================================================


		/*
		* @brief 取特定目录
		* @param [in] folderId : FOLDERID_ 开头的枚举值
		* @param [in] trailingSlash : 是否以 \ 反斜杠结尾
		* @return 返回特定目录路径
		*/
		std::wstring GetKnownFolderPathW(REFKNOWNFOLDERID folderId, bool trailingSlash = true);

		/*
		* @brief 取特定目录
		* @param [in] folderId : FOLDERID_ 开头的枚举值
		* @param [in] trailingSlash : 是否以 \ 反斜杠结尾
		* @return 返回特定目录路径
		*/
		std::string GetKnownFolderPathU8(REFKNOWNFOLDERID folderId, bool trailingSlash = true);

		/*
		* @brief 获取当前可执行文件路径全路径，可通过GetExePath().parent_path()获取exe所在目录，可通过GetExePath().filename()获取exe文件名。
		* @brief 如果要获得不加引号的路径，可访问string()函数，例如GetExePath().parent_path().string()
		* @return 返回当前可执行文件的全路径
		*/
		std::filesystem::path GetExePath();

		/*
		* @brief 获取当前可执行文件路径，UTF8版本支持长路径
		* @brief 类似于易语言中的 "取运行目录" 函数
		* @param [in] withSlash : 是否结尾有反斜杠
		* @return 返回当前可执行文件路径，默认结尾有反斜杠，可通过参数控制
		*/
		[[deprecated("Use GetExePath() instead")]]
		std::string GetExeDirA_UTF8(bool withSlash = true);

		/*
		* @brief 获取当前可执行文件路径，ANSI版本不支持长路径
		* @brief 类似于易语言中的 "取运行目录" 函数
		* @param [in] withSlash : 是否结尾有反斜杠
		* @return 返回当前可执行文件路径，默认结尾有反斜杠，可通过参数控制
		*/
		[[deprecated("Use GetExePath() instead")]]
		std::string GetExeDirA(bool withSlash = true);

		/*
		* @brief 获取当前可执行文件路径
		* @brief 类似于易语言中的 "取运行目录" 函数
		* @param [in] withSlash : 是否结尾有反斜杠
		* @return 返回当前可执行文件路径，默认结尾有反斜杠，可通过参数控制
		*/
		[[deprecated("Use GetExePath() instead")]]
		std::wstring GetExeDirW(bool withSlash = true);


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
			_In_ LPCSTR resName,
			_In_ LPCSTR resType,
			_In_ const std::string& outPath
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
			_In_ LPCWSTR resName,
			_In_ LPCWSTR resType,
			_In_ const std::wstring& outPath
		);

				/*
		* @brief 写配置项，支持ANSI和UTF8
		* @param [in] fileName: 文件名
		* @param [in] section: 配置节名称（区段名）
		* @param [in] key： 配置项名称（键名称）
		* @param [in] value： 欲写入的值
		* @return (bool) 是否写入成功
		*/
		bool write_profileA(
			_In_ const string& fileName,
			_In_ const string& section,
			_In_ const string& key,
			_In_ const string& value
		);

		/*
		* @brief 写配置项，写出的是ANSI编码，虽然是W版本，但是写出的文件却是ANSI编码（Windows系统的问题），如需UNICODE支持，改用A版本并使用UTF8编码
		* @param [in] fileName: 文件名
		* @param [in] section: 配置节名称（区段名）
		* @param [in] key： 配置项名称（键名称）
		* @param [in] value： 欲写入的值
		* @return (bool) 是否写入成功
		*/
		bool write_profileW(
			_In_ const wstring& fileName,
			_In_ const wstring& section,
			_In_ const wstring& key,
			_In_ const wstring& value
		);


		/*
		* @brief 读配置项，支持ANSI和UTF8
		* @param [in] fileName: 文件名
		* @param [in] section: 配置节名称（区段名）
		* @param [in] key： 配置项名称（键名称）
		* @param [in] defaultValue： 默认值，当配置项不存在时，返回此值
		* @param [in] defaultBufferSize: 默认缓冲区大小，默认为256，每次空间不够的时候会自动扩容256个字节。使用者需要预估项值的长度，防止扩容次数过多造成的时间开销。
		* @return: (string)[ANSI] 配置项的值，如果失败则返回空字符串
		*/
		string read_profileA(
			_In_ const string& fileName,
			_In_ const string& section,
			_In_ const string& key,
			_In_ const string& defaultValue,
			_In_ DWORD defaultBufferSize = 256
		);

		/*
		* @brief 读配置项，W版本读入的是 ANSI 或 UTF-16 编码的文件，结果为 UTF-16 编码，可使用 encoding_ 前缀的编码转换函数转换为 ANSI 或 UTF8 编码
		* @param [in] fileName: 文件名
		* @param [in] section: 配置节名称（区段名）
		* @param [in] key： 配置项名称（键名称）
		* @param [in] defaultValue： 默认值，当配置项不存在时，返回此值
		* @param [in] defaultBufferSize: 默认缓冲区大小，默认为256，每次空间不够的时候会自动扩容256个字节。使用者需要预估项值的长度，防止扩容次数过多造成的时间开销。
		* @return: (string) 配置项的值，如果失败则返回空字符串
		*/
		wstring read_profileW(
			_In_ const wstring& fileName,
			_In_ const wstring& section,
			_In_ const wstring& key,
			_In_ const wstring& defaultValue,
			_In_ DWORD defaultBufferSize = 256
		);


		/*
		* @brief 写出结构体数据
		* @param [in] fileName: 文件名
		* @param [in] section: 配置节名称（区段名）
		* @param [in] key： 配置项名称（键名称）
		* @param [in] lpStruct: 欲写入的结构体指针
		* @param [in] uSizeStruct: 结构体大小
		* @return: (bool) 是否写入成功
		*/
		bool write_structA(
			_In_ const string& fileName,
			_In_ const string& section,
			_In_ const string& key,
			_In_ void* lpStruct,
			_In_ UINT uSizeStruct
		);

		/*
		* @brief 写出结构体数据
		* @param [in] fileName: 文件名
		* @param [in] section: 配置节名称（区段名）
		* @param [in] key： 配置项名称（键名称）
		* @param [in] lpStruct: 欲写入的结构体指针
		* @param [in] uSizeStruct: 结构体大小
		* @return: (bool) 是否写入成功
		*/
		bool write_structW(
			_In_ const wstring& fileName,
			_In_ const wstring& section,
			_In_ const wstring& key,
			_In_ void* lpStruct,
			_In_ UINT uSizeStruct
		);

		/*
		* @brief 读入结构体数据
		* @param [in] fileName: 文件名
		* @param [in] section: 配置节名称（区段名）
		* @param [in] key： 配置项名称（键名称）
		* @param [out] lpStruct: 欲读入的结构体指针
		* @param [in] uSizeStruct: 结构体大小
		* @return: (bool) 是否读取成功
		*/
		bool read_structA(
			_In_ const string& fileName,
			_In_ const string& section,
			_In_ const string& key,
			_Out_ void* lpStruct,
			_In_ UINT uSizeStruct
		);

		/*
		* @brief 读入结构体数据
		* @param [in] fileName: 文件名
		* @param [in] section: 配置节名称（区段名）
		* @param [in] key： 配置项名称（键名称）
		* @param [out] lpStruct: 欲读入的结构体指针
		* @param [in] uSizeStruct: 结构体大小
		* @return: (bool) 是否读取成功
		*/
		bool read_structW(
			_In_ const wstring& fileName,
			_In_ const wstring& section,
			_In_ const wstring& key,
			_Out_ void* lpStruct,
			_In_ UINT uSizeStruct
		);
	}
}


#endif /* _SYS_CORE_DISK_MANIPULATION_H_ */