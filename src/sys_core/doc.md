# sys_core - 核心库


## 简介
本库为系统核心库，包含系统操作、磁盘操作、编码转换、窗口操作、字符串操作等常用模块。


## 头文件
```c
//该头文件中包含了以下所有模块的头文件
#include "sys_core.h"
```

## 系统处理
```C++
//所属头文件
#include "sys_processing.h"
```
### write_profile 和 read_profile
```C++
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
```

### write_struct 和 read_struct
```C++
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
```

## 磁盘操作
```C++
//所属头文件
#include "disk_manipulation.h"
```
```C++
	/*
	* @brief 判断文件是否存在
	* @brief 类似于易语言中的 "文件是否存在" 函数
	* @param [in] fileName: 文件名
	* @return true 文件存在，false 文件不存在
	*/
	bool file_isExistsA(
		_In_ const string& fileName 
	);

	/*
	* @brief 判断文件是否存在
	* @brief 类似于易语言中的 "文件是否存在" 函数
	* @param [in] fileName: 文件名
	* @return true 文件存在，false 文件不存在
	*/
	bool file_isExistsW(
		_In_ const wstring& fileName 
	);

	/*
	* @brief 将vector数据以二进制的形式写入文件，如果文件不存在则创建文件，如果文件存在则覆盖文件内容
	* @brief 类似于易语言中的 "写到文件" 函数
	* @param [in] fileName: 文件名
	* @param [in] data: 欲写到文件中的数据
	* @return true 写入成功，false 写入失败
	*/
	bool write_to_fileA(
		_In_ const string& fileName,
		_In_ const vector<char>& data 
	);
	/*
	* @brief 将vector数据以二进制的形式写入文件，如果文件不存在则创建文件，如果文件存在则覆盖文件内容
	* @brief 类似于易语言中的 "写到文件" 函数
	* @param [in] fileName: 文件名
	* @param [in] data: 欲写到文件中的数据
	* @return true 写入成功，false 写入失败
	*/
	bool write_to_fileW(
		_In_ const wstring& fileName,
		_In_ const vector<char>& data 
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
		_In_ const string& fileName,
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
		_In_ const wstring& fileName,
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
		_In_ const string& fileName,
		_In_ const string& data 
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
		_In_ const wstring& fileName,
		_In_ const string& data 
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
		_In_ const string& fileName,
		_In_ const wstring& data 
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
		_In_ const wstring& fileName,
		_In_ const wstring& data 
	);
```


## 编码转换
```C++
//所属头文件
#include "encoding.h"
```

### ANSI 与 UTF-8 互转
```C++
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
```
### ANSI 与 UTF-16 互转
```C++
	/*
	* @brief 将ANSI编码的string转换为UTF-16编码的wstring宽字符串
	* @param [in] str: ANSI编码的string字符串
	* @return (wstring) UTF-16编码的wstring宽字符串
	*/
	wstring encoding_ANSI_to_wstring( _In_ const string& str );
	/*
	* @brief 将UTF-16编码的wstring宽字符串转换为ANSI编码的string字符串
	* @param [in] str: UTF-16编码的wstring宽字符串
	* @return (string) ANSI编码的string字符串
	*/
	string encoding_wstring_to_ANSI( _In_ const wstring& str );
```
### UTF-8 与 UTF-16 互转
```C++
	/*
	* @brief 将UTF-8编码的string转换为 UTF-16编码的wstring宽字符串
	* @param [in] str: UTF-8编码的string字符串
	* @return (wstring) UTF-16编码的wstring宽字符串
	*/
	wstring encoding_UTF8_to_wstring( _In_ const string& str );
	/*
	* @brief 将UTF-16编码的wstring宽字符串转换为UTF-8编码的string字符串
	* @param [in] str: UTF-16编码的wstring宽字符串
	* @return (string) UTF-8编码的string字符串
	*/
	string encoding_wstring_to_UTF8( _In_ const wstring& str );

```


## 窗口操作


## 字符串操作