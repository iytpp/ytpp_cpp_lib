#include "disk_manipulation.h"


namespace ytpp {

	bool file_isExists( _In_ const _tstring& fileName )
	{
		HANDLE hFind = 0;
		WIN32_FIND_DATA wfd = {0};
		hFind = FindFirstFile( fileName.c_str(), &wfd);
		if ( hFind == INVALID_HANDLE_VALUE ) {
			return false;
		}
		FindClose( hFind );
		return ((wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0);
	}

	bool write_to_file(
		_In_ const _tstring& fileName,
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

	bool write_to_file( 
		_In_ const _tstring& fileName,
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

	bool write_to_file( 
		_In_ const _tstring& fileName,
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

}