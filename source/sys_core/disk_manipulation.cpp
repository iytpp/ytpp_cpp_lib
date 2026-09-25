#include "sys_core/disk_manipulation.h"
#include "sys_core/encoding.h"

#include ".private/debug_tools.h" //用于调试的工具

#include <iostream>
#include <filesystem>

namespace ytpp {
    namespace sys_core
	{
		std::wstring GetKnownFolderPathW(
			REFKNOWNFOLDERID folderId,
			bool trailingSlash)
		{
			PWSTR path = nullptr;

			const HRESULT hr = SHGetKnownFolderPath(
				folderId,
				KF_FLAG_DEFAULT,
				nullptr,
				&path
			);

			if (FAILED(hr) || path == nullptr)
				return {};

			std::wstring result(path);

			CoTaskMemFree(path);

			if (trailingSlash)
			{
				if (!result.empty() &&
					result.back() != L'\\' &&
					result.back() != L'/')
				{
					result.push_back(L'\\');
				}
			} else
			{
				while (result.size() > 1 &&
					(result.back() == L'\\' ||
					result.back() == L'/'))
				{
					result.pop_back();
				}
			}

			return result;
		}

		std::string GetKnownFolderPathU8(
			REFKNOWNFOLDERID folderId,
			bool trailingSlash)
		{
			const std::wstring widePath =
				GetKnownFolderPathW(folderId, trailingSlash);

			if (widePath.empty())
				return {};

			const int requiredSize = WideCharToMultiByte(
				CP_UTF8,
				0,
				widePath.data(),
				static_cast<int>(widePath.size()),
				nullptr,
				0,
				nullptr,
				nullptr
			);

			if (requiredSize <= 0)
				return {};

			std::string result(
				static_cast<std::size_t>(requiredSize),
				'\0'
			);

			WideCharToMultiByte(
				CP_UTF8,
				0,
				widePath.data(),
				static_cast<int>(widePath.size()),
				result.data(),
				requiredSize,
				nullptr,
				nullptr
			);

			return result;
		}

		std::filesystem::path GetExePath()
		{
			wchar_t buffer[MAX_PATH]{};

			DWORD length = GetModuleFileNameW(
				nullptr,
				buffer,
				MAX_PATH
			);

			if (length == 0)
				return {};

			return std::filesystem::path(buffer);
		}

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

		std::string GetExeDirA(bool withSlash)
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
			_In_ LPCSTR resName, 
			_In_ LPCSTR resType, 
			_In_ const std::string& outPath) {
			HRSRC hResource = FindResourceA(hModule, resName, resType);
			if (!hResource) return false;

			DWORD size = SizeofResource(hModule, hResource);
			if (!size) return false;

			HGLOBAL hGlobal = LoadResource(hModule, hResource);
			if (!hGlobal) return false;

			const void* data = LockResource(hGlobal);
			if (!data) return false;

			FILE* file = nullptr;
			if (fopen_s(&file, outPath.c_str(), "wb") != 0 || !file) return false;

			size_t written = fwrite(data, 1, size, file);
			fclose(file);
			return written == size;
		}

		bool write_resource_fileW(
			_In_ HMODULE hModule, 
			_In_ LPCWSTR resName, 
			_In_ LPCWSTR resType, 
			_In_ const std::wstring& outPath) {
			HRSRC hResource = FindResourceW(hModule, resName, resType);
			if (!hResource) return false;

			DWORD size = SizeofResource(hModule, hResource);
			if (!size) return false;

			HGLOBAL hGlobal = LoadResource(hModule, hResource);
			if (!hGlobal) return false;

			const void* data = LockResource(hGlobal);
			if (!data) return false;

			FILE* file = nullptr;
			if (_wfopen_s(&file, outPath.c_str(), L"wb") != 0 || !file) return false;

			size_t written = fwrite(data, 1, size, file);
			fclose(file);
			return written == size;
		}

		bool write_profileA(
			_In_ const string& fileName,
			_In_ const string& section,
			_In_ const string& key,
			_In_ const string& value)
		{
			return WritePrivateProfileStringA(
				section.c_str(),
				key.c_str(),
				value.c_str(),
				fileName.c_str()
			);
		} /* write_profileA */

		bool write_profileW(
			_In_ const wstring& fileName,
			_In_ const wstring& section,
			_In_ const wstring& key,
			_In_ const wstring& value)
		{
			return WritePrivateProfileStringW(
				section.c_str(),
				key.c_str(),
				value.c_str(),
				fileName.c_str()
			);
		} /* write_profileW */

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
				if (result != 0) { //说明已经取过一次了，但是缓冲区不够
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
			} while (result == cur_buffer_size - 1 && buffer[result] == '\0');
			rtn.assign(buffer);
			delete[] buffer;
			return rtn;
		} /* read_profileA */

		std::wstring read_profileW(
			_In_ const wstring& fileName,
			_In_ const wstring& section,
			_In_ const wstring& key,
			_In_ const wstring& defaultValue,
			_In_ DWORD defaultBufferSize /* = 256 */)
		{
			wstring rtn;
			DWORD cur_buffer_size = defaultBufferSize;
			wchar_t* buffer = new wchar_t[cur_buffer_size];
			//ZeroMemory( buffer, cur_buffer_size );
			DWORD result = 0;
			do
			{
				if (result != 0) { //说明已经取过一次了，但是缓冲区不够
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
			} while (result == cur_buffer_size - 1 && buffer[result] == '\0');
			rtn.assign(buffer);
			delete[] buffer;
			return rtn;
		} /* read_profileW */

		bool write_structA(
			_In_ const string& fileName,
			_In_ const string& section,
			_In_ const string& key,
			_In_ void* lpStruct,
			_In_ UINT uSizeStruct)
		{
			return WritePrivateProfileStructA(
				section.c_str(),
				key.c_str(),
				lpStruct,
				uSizeStruct,
				fileName.c_str()
			);
		} /* write_structA */

		bool write_structW(
			_In_ const wstring& fileName,
			_In_ const wstring& section,
			_In_ const wstring& key,
			_In_ void* lpStruct,
			_In_ UINT uSizeStruct)
		{
			return WritePrivateProfileStructW(
				section.c_str(),
				key.c_str(),
				lpStruct,
				uSizeStruct,
				fileName.c_str()
			);
		} /* write_structW */

		bool read_structA(
			_In_ const string& fileName,
			_In_ const string& section,
			_In_ const string& key,
			_Out_ void* lpStruct,
			_In_ UINT uSizeStruct)
		{
			return GetPrivateProfileStructA(
				section.c_str(),
				key.c_str(),
				lpStruct,
				uSizeStruct,
				fileName.c_str()
			);
		} /* read_structA */

		bool read_structW(
			_In_ const wstring& fileName,
			_In_ const wstring& section,
			_In_ const wstring& key,
			_Out_ void* lpStruct,
			_In_ UINT uSizeStruct)
		{
			return GetPrivateProfileStructW(
				section.c_str(),
				key.c_str(),
				lpStruct,
				uSizeStruct,
				fileName.c_str()
			);
		} /* read_structW */

	}
}