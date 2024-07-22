#include "sys_processing.h"

#include "debug_tools.h" //用于调试的工具

namespace ytpp {

	bool write_profileA( 
		_In_ const string& fileName,
		_In_ const string& section,
		_In_ const string& key,
		_In_ const string& value )
	{
		return WritePrivateProfileStringA(
			section.c_str(),
			key.c_str(),
			value.c_str(),
			fileName.c_str() 
		);
	}

	bool write_profileW( 
		_In_ const wstring& fileName, 
		_In_ const wstring& section, 
		_In_ const wstring& key, 
		_In_ const wstring& value )
	{
		return WritePrivateProfileStringW(
			section.c_str(),
			key.c_str(),
			value.c_str(),
			fileName.c_str()
		);
	}

	string read_profileA(
		_In_ const string& fileName,
		_In_ const string& section,
		_In_ const string& key,
		_In_ const string& defaultValue,
		_In_ DWORD defaultBufferSize /* = 256 */)
	{
		string rtn;
		DWORD cur_buffer_size = defaultBufferSize;
		char* buffer = new char[cur_buffer_size];
		//ZeroMemory( buffer, cur_buffer_size );
		DWORD result = 0;
		do 
		{
			if ( result != 0 ) { //说明已经取过一次了，但是缓冲区不够
				cur_buffer_size += 256; //扩容
				delete[] buffer; //释放旧的缓冲区
				buffer = new char[cur_buffer_size]; //重新分配扩容
				//ZeroMemory( buffer, cur_buffer_size );
			}
			result = GetPrivateProfileStringA(
				section.c_str(),
				key.c_str(),
				defaultValue.c_str(),
				buffer,
				cur_buffer_size,
				fileName.c_str()
			);
			//COUT << result << endl;
		} while ( result == cur_buffer_size - 1 && buffer[result] == '\0' );
		rtn.assign( buffer );
		delete[] buffer;
		return rtn;
	}

	std::wstring read_profileW( 
		_In_ const wstring& fileName,
		_In_ const wstring& section, 
		_In_ const wstring& key, 
		_In_ const wstring& defaultValue, 
		_In_ DWORD defaultBufferSize /* = 256 */ )
	{
		wstring rtn;
		DWORD cur_buffer_size = defaultBufferSize;
		wchar_t* buffer = new wchar_t[cur_buffer_size];
		//ZeroMemory( buffer, cur_buffer_size );
		DWORD result = 0;
		do
		{
			if ( result != 0 ) { //说明已经取过一次了，但是缓冲区不够
				cur_buffer_size += 256; //扩容
				delete[] buffer; //释放旧的缓冲区
				buffer = new wchar_t[cur_buffer_size]; //重新分配扩容
				//ZeroMemory( buffer, cur_buffer_size );
			}
			result = GetPrivateProfileStringW(
				section.c_str(),
				key.c_str(),
				defaultValue.c_str(),
				buffer,
				cur_buffer_size,
				fileName.c_str()
			);
			//COUT << result << endl;
		} while ( result == cur_buffer_size - 1 && buffer[result] == '\0' );
		rtn.assign( buffer );
		delete[] buffer;
		return rtn;
	}

	bool write_structA( 
		_In_ const string& fileName, 
		_In_ const string& section, 
		_In_ const string& key, 
		_In_ void* lpStruct, 
		_In_ UINT uSizeStruct )
	{
		return WritePrivateProfileStructA(
			section.c_str(),
			key.c_str(),
			lpStruct,
			uSizeStruct,
			fileName.c_str()
		);
	}

	bool write_structW( 
		_In_ const wstring& fileName, 
		_In_ const wstring& section, 
		_In_ const wstring& key, 
		_In_ void* lpStruct,
		_In_ UINT uSizeStruct )
	{
		return WritePrivateProfileStructW(
			section.c_str(),
			key.c_str(),
			lpStruct,
			uSizeStruct,
			fileName.c_str()
		);
	}

	bool read_structA( 
		_In_ const string& fileName, 
		_In_ const string& section,
		_In_ const string& key, 
		_Out_ void* lpStruct, 
		_In_ UINT uSizeStruct )
	{
		return GetPrivateProfileStructA(
			section.c_str(),
			key.c_str(),
			lpStruct,
			uSizeStruct,
			fileName.c_str()
		);
	}

	bool read_structW( 
		_In_ const wstring& fileName, 
		_In_ const wstring& section,
		_In_ const wstring& key, 
		_Out_ void* lpStruct, 
		_In_ UINT uSizeStruct )
	{
		return GetPrivateProfileStructW(
			section.c_str(),
			key.c_str(),
			lpStruct,
			uSizeStruct,
			fileName.c_str()
		);
	}

}