#include "disk_manipulation.h"

#include "debug_tools.h" //用于调试的工具



namespace ytpp {

	bool file_isExistsA( _In_ const string& fileName )
	{
		HANDLE hFind = 0;
		WIN32_FIND_DATAA wfd = {0};
		hFind = FindFirstFileA( fileName.c_str(), &wfd);
		if ( hFind == INVALID_HANDLE_VALUE ) {
			return false;
		}
		FindClose( hFind );
		return ((wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0);
	}

	bool file_isExistsW( _In_ const wstring& fileName )
	{
		HANDLE hFind = 0;
		WIN32_FIND_DATAW wfd = { 0 };
		hFind = FindFirstFileW( fileName.c_str(), &wfd );
		if ( hFind == INVALID_HANDLE_VALUE ) {
			return false;
		}
		FindClose( hFind );
		return ((wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0);
	}

	bool write_to_fileA(
		_In_ const string& fileName,
		_In_ const vector<char>& data )
	{
		//先判断data是否为空，如果为空，则返回true
		if ( data.empty() || data.size() == 0 ) {
			return true;
		}
		//data不为空，则写入文件
		ofstream out( fileName, ios::binary );
		if ( !out.is_open() ) {
			return false;
		}
		out.write( &data[0], data.size() );
		out.close();
		return true;
	}

	bool write_to_fileW( 
		_In_ const wstring& fileName, 
		_In_ const vector<char>& data )
	{
		//先判断data是否为空，如果为空，则返回true
		if ( data.empty() || data.size() == 0 ) {
			return true;
		}
		//data不为空，则写入文件
		ofstream out( fileName, ios::binary );
		if ( !out.is_open() ) {
			return false;
		}
		out.write( &data[0], data.size() );
		out.close();
		return true;
	}


	bool write_to_fileA(
		_In_ const string& fileName,
		_In_opt_ const char* data,
		_In_opt_ size_t size )
	{
		//先判断data是否为空，如果为空，则返回true
		if ( data == NULL || size == NULL ) {
			return true;
		}
		ofstream out( fileName, ios::binary );
		if ( !out.is_open() ) {
			return false;
		}
		out.write( data, size );
		out.close();
		return true;
	}

	bool write_to_fileW( 
		_In_ const wstring& fileName, 
		_In_opt_ const char* data, 
		_In_opt_ size_t size )
	{
		//先判断data是否为空，如果为空，则返回true
		if ( data == NULL || size == NULL ) {
			return true;
		}
		ofstream out( fileName, ios::binary );
		if ( !out.is_open() ) {
			return false;
		}
		out.write( data, size );
		out.close();
		return true;
	}



	bool write_to_fileA( 
		_In_ const string& fileName,
		_In_ const string& data )
	{
		//先判断data是否为空，如果为空，则返回true
		if ( data.empty() || data.size() == 0 ) {
			return true;
		}
		//data不为空，则写入文件
		ofstream out( fileName, ios::out );
		if ( !out.is_open() ) {
			return false;
		}
		out.write( &data[0], data.size() );
		out.close();
		return true;
	}

	bool write_to_fileW( 
		_In_ const wstring& fileName, 
		_In_ const string& data )
	{
				//先判断data是否为空，如果为空，则返回true
		if ( data.empty() || data.size() == 0 ) {
			return true;
		}
		//data不为空，则写入文件
		ofstream out( fileName, ios::out );
		if ( !out.is_open() ) {
			return false;
		}
		out.write( &data[0], data.size() );
		out.close();
		return true;
	}


	bool write_to_fileA( 
		_In_ const string& fileName, 
		_In_ const wstring& data )
	{
		//先判断data是否为空，如果为空，则返回true
		if ( data.empty() || data.size() == 0 ) {
			return true;
		}
		//data不为空，则写入文件
		ofstream out( fileName, ios::out );
		if ( !out.is_open() ) {
			return false;
		}
		char* buffer = nullptr;
		size_t bufferSize = data.size() * sizeof( wchar_t );
		buffer = new char[bufferSize];
		//ZeroMemory( buffer, bufferSize );
		memcpy( buffer, &data[0], bufferSize );

		out.write( buffer, bufferSize );
		out.close();

		delete[] buffer;
		return true;
	}

	bool write_to_fileW( 
		_In_ const wstring& fileName, 
		_In_ const wstring& data )
	{
		//先判断data是否为空，如果为空，则返回true
		if ( data.empty() || data.size() == 0 ) {
			return true;
		}
		//data不为空，则写入文件
		ofstream out( fileName, ios::out );
		if ( !out.is_open() ) {
			return false;
		}
		char* buffer = nullptr;
		size_t bufferSize = data.size() * sizeof( wchar_t );
		buffer = new char[bufferSize];
		//ZeroMemory( buffer, bufferSize );
		memcpy( buffer, &data[0], bufferSize );

		out.write( buffer, bufferSize );
		out.close();

		delete[] buffer;
		return true;
	}

}