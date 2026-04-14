#include "sys_core/disk_manipulation.h"
#include "sys_core/encoding.h"

#include ".private/debug_tools.h" //用于调试的工具

#include <iostream>

namespace ytpp {
    namespace sys_core
	{
		std::string GetExeDirA_UTF8(bool withSlash)
		{
			std::vector<wchar_t> buffer(1024);

			DWORD len = GetModuleFileNameW(nullptr, buffer.data(), (DWORD)buffer.size());
			if (len == 0)
				return "";

			std::wstring fullPath(buffer.data(), len);

			size_t pos = fullPath.find_last_of(L"\\/");
			if (pos == std::wstring::npos)
				return "";

			std::wstring dir;
			if (withSlash)
				dir = fullPath.substr(0, pos + 1);
			else
				dir = fullPath.substr(0, pos);

			return encoding_wstring_to_UTF8(dir);
		}



		std::string GetExeDirA(bool withSlash = true)
		{
			char path[MAX_PATH] = { 0 };
			GetModuleFileNameA(NULL, path, MAX_PATH);

			std::string fullPath = path;

			size_t pos = fullPath.find_last_of("\\/");
			if (pos == std::string::npos)
				return "";

			if (withSlash)
				return fullPath.substr(0, pos + 1);
			else
				return fullPath.substr(0, pos);
		}



		std::wstring GetExeDirW(bool withSlash)
		{
			std::vector<wchar_t> buffer(1024);
			DWORD len = GetModuleFileNameW(nullptr, buffer.data(), (DWORD)buffer.size());

			if (len == 0)
				return L"";

			std::wstring fullPath(buffer.data(), len);

			size_t pos = fullPath.find_last_of(L"\\/");
			if (pos == std::wstring::npos)
				return L"";

			if (withSlash)
				return fullPath.substr(0, pos + 1);
			else
				return fullPath.substr(0, pos);
		}

		bool file_isExistsA(_In_ const string & fileName)
		{
			HANDLE hFind = 0;
			WIN32_FIND_DATAA wfd = { 0 };
			hFind = FindFirstFileA(fileName.c_str(), &wfd);
			if (hFind == INVALID_HANDLE_VALUE) {
				return false;
			}
			FindClose(hFind);
			return ((wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0);
		}

		bool file_isExistsW(_In_ const wstring & fileName)
		{
			HANDLE hFind = 0;
			WIN32_FIND_DATAW wfd = { 0 };
			hFind = FindFirstFileW(fileName.c_str(), &wfd);
			if (hFind == INVALID_HANDLE_VALUE) {
				return false;
			}
			FindClose(hFind);
			return ((wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0);
		}

		bool write_to_fileA(
			_In_ const string & fileName,
			_In_ const vector<char>&data)
		{
			//先判断data是否为空，如果为空，则返回true
			if (data.empty() || data.size() == 0) {
				return true;
			}
			//data不为空，则写入文件
			ofstream out(fileName, ios::binary);
			if (!out.is_open()) {
				return false;
			}
			out.write(&data[0], data.size());
			out.close();
			return true;
		}

		bool write_to_fileW(
			_In_ const wstring & fileName,
			_In_ const vector<char>&data)
		{
			//先判断data是否为空，如果为空，则返回true
			if (data.empty() || data.size() == 0) {
				return true;
			}
			//data不为空，则写入文件
			ofstream out(fileName, ios::binary);
			if (!out.is_open()) {
				return false;
			}
			out.write(&data[0], data.size());
			out.close();
			return true;
		}


		bool write_to_fileA(
			_In_ const string & fileName,
			_In_opt_ const char* data,
			_In_opt_ size_t size)
		{
			//先判断data是否为空，如果为空，则返回true
			if (data == NULL || size == NULL) {
				return true;
			}
			ofstream out(fileName, ios::binary);
			if (!out.is_open()) {
				return false;
			}
			out.write(data, size);
			out.close();
			return true;
		}

		bool write_to_fileW(
			_In_ const wstring & fileName,
			_In_opt_ const char* data,
			_In_opt_ size_t size)
		{
			//先判断data是否为空，如果为空，则返回true
			if (data == NULL || size == NULL) {
				return true;
			}
			ofstream out(fileName, ios::binary);
			if (!out.is_open()) {
				return false;
			}
			out.write(data, size);
			out.close();
			return true;
		}



		bool write_to_fileA(
			_In_ const string & fileName,
			_In_ const string & data)
		{
			//先判断data是否为空，如果为空，则返回true
			if (data.empty() || data.size() == 0) {
				return true;
			}
			//data不为空，则写入文件
			ofstream out(fileName, ios::out);
			if (!out.is_open()) {
				return false;
			}
			out.write(&data[0], data.size());
			out.close();
			return true;
		}

		bool write_to_fileW(
			_In_ const wstring & fileName,
			_In_ const string & data)
		{
			//先判断data是否为空，如果为空，则返回true
			if (data.empty() || data.size() == 0) {
				return true;
			}
			//data不为空，则写入文件
			ofstream out(fileName, ios::out);
			if (!out.is_open()) {
				return false;
			}
			out.write(&data[0], data.size());
			out.close();
			return true;
		}


		bool write_to_fileA(
			_In_ const string & fileName,
			_In_ const wstring & data)
		{
			//先判断data是否为空，如果为空，则返回true
			if (data.empty() || data.size() == 0) {
				return true;
			}
			//data不为空，则写入文件
			ofstream out(fileName, ios::out);
			if (!out.is_open()) {
				return false;
			}
			char* buffer = nullptr;
			size_t bufferSize = data.size() * sizeof(wchar_t);
			buffer = new char[bufferSize];
			//ZeroMemory( buffer, bufferSize );
			memcpy(buffer, &data[0], bufferSize);

			out.write(buffer, bufferSize);
			out.close();

			delete[] buffer;
			return true;
		}

		bool write_to_fileW(
			_In_ const wstring & fileName,
			_In_ const wstring & data)
		{
			//先判断data是否为空，如果为空，则返回true
			if (data.empty() || data.size() == 0) {
				return true;
			}
			//data不为空，则写入文件
			ofstream out(fileName, ios::out);
			if (!out.is_open()) {
				return false;
			}
			char* buffer = nullptr;
			size_t bufferSize = data.size() * sizeof(wchar_t);
			buffer = new char[bufferSize];
			//ZeroMemory( buffer, bufferSize );
			memcpy(buffer, &data[0], bufferSize);

			out.write(buffer, bufferSize);
			out.close();

			delete[] buffer;
			return true;
		}

		bool write_resource_fileA(
			_In_ HMODULE hModule, 
			_In_ int resID,
			_In_ string resType,
			_In_ string outPath) {
			HRSRC hResource = FindResourceA(hModule, MAKEINTRESOURCEA(resID), resType.c_str());
			if (hResource == NULL) {
				//std::cout << "WriteResourceFile - 寻找必要资源失败" << std::endl;
				return false;
			}
			HGLOBAL hGlobal = LoadResource(hModule, hResource);
			if (hGlobal == NULL) {
				//std::cout << "WriteResourceFile - 加载必要资源失败" << std::endl;
				return false;
			}
			DWORD dwSize = SizeofResource(hModule, hResource);
			if (dwSize == 0) {
				//std::cout << "WriteResourceFile - 获取必要资源大小失败" << std::endl;
				return false;
			}
			void* pResource = LockResource(hGlobal);
			if (pResource == NULL) {
				//std::cout << "WriteResourceFile - 锁定必要资源失败" << std::endl;
				return false;
			}
			FILE* pFile = nullptr;
			fopen_s(&pFile, outPath.c_str(), "wb");
			if (pFile == NULL) {
				//std::cout << "WriteResourceFile - 写入资源文件失败" << std::endl;
				return false;
			}
			fwrite(pResource, 1, dwSize, pFile);
			fclose(pFile);
			UnlockResource(pResource);
			return true;
		}

		bool write_resource_fileW(
			_In_ HMODULE hModule,
			_In_ int resID,
			_In_ wstring resType,
			_In_ wstring outPath) {
			HRSRC hResource = FindResourceW(hModule, MAKEINTRESOURCEW(resID), resType.c_str());
			if (hResource == NULL) {
				//std::cout << "WriteResourceFile - 寻找必要资源失败" << std::endl;
				return false;
			}
			HGLOBAL hGlobal = LoadResource(hModule, hResource);
			if (hGlobal == NULL) {
				//std::cout << "WriteResourceFile - 加载必要资源失败" << std::endl;
				return false;
			}
			DWORD dwSize = SizeofResource(hModule, hResource);
			if (dwSize == 0) {
				//std::cout << "WriteResourceFile - 获取必要资源大小失败" << std::endl;
				return false;
			}
			void* pResource = LockResource(hGlobal);
			if (pResource == NULL) {
				//std::cout << "WriteResourceFile - 锁定必要资源失败" << std::endl;
				return false;
			}
			FILE* pFile = nullptr;
			_wfopen_s(&pFile, outPath.c_str(), L"wb");
			if (pFile == NULL) {
				//std::cout << "WriteResourceFile - 写入资源文件失败" << std::endl;
				return false;
			}
			fwrite(pResource, 1, dwSize, pFile);
			fclose(pFile);
			UnlockResource(pResource);
			return true;
		}
	}
}