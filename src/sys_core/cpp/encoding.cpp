#include "encoding.h"

namespace ytpp {
	
	string encoding_ANSI_to_UTF8( _In_ const string& str )
	{
		string rtn;
		WCHAR* p = nullptr;
		int _len = MultiByteToWideChar( CP_ACP, NULL, str.c_str(), -1, NULL, 0 );
		p = new WCHAR[_len];
		MultiByteToWideChar( CP_ACP, NULL, str.c_str(), -1, p, _len );

		CHAR* p2 = nullptr;
		_len = WideCharToMultiByte( CP_UTF8, NULL, p, -1, NULL, 0, NULL, NULL );
		p2 = new CHAR[_len];
		WideCharToMultiByte( CP_UTF8, NULL, p, -1, p2, _len, NULL, NULL );

		rtn.assign( p2 );
		delete[] p;
		delete[] p2;

		return rtn;
	}

	string encoding_UTF8_to_ANSI( _In_ const string& str )
	{
		string rtn;
		WCHAR* p = nullptr;
		int _len = MultiByteToWideChar( CP_UTF8, NULL, str.c_str(), -1, NULL, 0 );
		p = new WCHAR[_len];
		MultiByteToWideChar( CP_UTF8, NULL, str.c_str(), -1, p, _len );

		CHAR* p2 = nullptr;
		_len = WideCharToMultiByte( CP_ACP, NULL, p, -1, NULL, 0, NULL, NULL );
		p2 = new CHAR[_len];
		WideCharToMultiByte( CP_ACP, NULL, p, -1, p2, _len, NULL, NULL );

		rtn.assign( p2 );
		delete[] p;
		delete[] p2;

		return rtn;
	}

	std::wstring encoding_ANSIstring_to_Widestring( _In_ const string& str )
	{
		wstring rtn;

		WCHAR* p = nullptr;
		int _len = MultiByteToWideChar( CP_ACP, NULL, str.c_str(), -1, NULL, 0 );
		p = new WCHAR[_len];
		MultiByteToWideChar( CP_ACP, NULL, str.c_str(), -1, p, _len );

		rtn.assign( p );
		delete[] p;
		return rtn;
	}

	std::string encoding_Widestring_to_ANSIstring( _In_ const wstring& str )
	{
		string rtn;

		CHAR* p = nullptr;
		int _len = WideCharToMultiByte( CP_ACP, NULL, str.c_str(), -1, NULL, 0, NULL, NULL );
		p = new CHAR[_len];
		WideCharToMultiByte( CP_ACP, NULL, str.c_str(), -1, p, _len, NULL, NULL );

		rtn.assign( p );
		delete[] p;
		return rtn;
	}

	std::wstring encoding_UTF8string_to_Widestring( _In_ const string& str )
	{
		wstring rtn;

		WCHAR* p = nullptr;
		int _len = MultiByteToWideChar( CP_UTF8, NULL, str.c_str(), -1, NULL, 0 );
		p = new WCHAR[_len];
		MultiByteToWideChar( CP_UTF8, NULL, str.c_str(), -1, p, _len );

		rtn.assign( p );
		delete[] p;

		return rtn;
	}

	std::string encoding_Widestring_to_UTF8string( _In_ const wstring& str )
	{
		string rtn;

		CHAR* p = nullptr;
		int _len = WideCharToMultiByte( CP_UTF8, NULL, str.c_str(), -1, NULL, 0, NULL, NULL );
		p = new CHAR[_len];
		WideCharToMultiByte( CP_UTF8, NULL, str.c_str(), -1, p, _len, NULL, NULL );

		rtn.assign( p );
		delete[] p;

		return rtn;
	}

}