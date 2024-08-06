#ifndef _SYS_CORE_SYS_PROCESSING_H_
#define _SYS_CORE_SYS_PROCESSING_H_

#include <windows.h>
#include <string>

#ifdef _UNICODE

#define write_profile write_profileW
#define read_profile read_profileW
#define write_struct write_structW
#define read_struct read_structW

#else /* ANSI */

#define write_profile write_profileA
#define read_profile read_profileA
#define write_struct write_structA
#define read_struct read_structA

#endif /* _UNICODE */

namespace ytpp {
	namespace sys_core {
		using namespace std;



		/*
		* @brief 设置光标位置
		* @param [in] x: 水平位置
		* @param [in] y: 垂直位置
		* @return (bool) 是否设置成功
		*/
		BOOL set_cursorPosOrig(
			_In_ int x, 
			_In_ int y
		);

		/*
		* @brief 设置光标位置，两个参数可选择性填写，如果为NULL则不修改对应的测度
		* @brief 由于设置前会获取原鼠标的位置，所以性能方面会有损耗，如果不希望自动获取原位置，可使用set_cursorPosOrig或直接调用系统API：SetCursorPos
		* @param [in] x: 水平位置
		* @param [in] y: 垂直位置
		* @return (bool) 是否设置成功
		*/
		BOOL set_cursorPos(
			_In_ int x, 
			_In_ int y
		);

		/*
		* @brief 获取光标位置
		* @param [out] x: 水平位置
		* @param [out] y: 垂直位置
		* @return (bool) 是否获取成功
		*/
		BOOL get_cursorPos(
			_Out_ LONG* x, 
			_Out_ LONG* y
		);

		/*
		* @brief 获取光标的垂直位置
		* @return (int) 光标垂直位置，如果获取失败则返回 -1
		*/
		LONG get_cursorPosY();

		/*
		* @brief 获取光标的水平位置
		* @return (int) 光标水平位置，如果获取失败则返回 -1
		*/
		LONG get_cursorPosX();

		/*
		* @brief 获取屏幕宽度
		* @return (int) 屏幕宽度
		*/
		int get_screenWidth();

		/*
		* @brief 获取屏幕高度
		* @return (int) 屏幕高度
		*/
		int get_screenHeight();

		/* 2024年7月26日↑ */

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


	} /* namespace sys_core */
} /* namespace ytpp */





#endif /* _SYS_CORE_SYS_PROCESSING_H_ */