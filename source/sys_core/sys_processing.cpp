#include "sys_core/sys_processing.h"

#include ".private/debug_tools.h" //用于调试的工具


namespace ytpp::sys_core {
	SingleInstanceGuard::SingleInstanceGuard(const std::wstring& appName,
		Namespace ns)
	{
		std::wstring mutexName = buildMutexName(appName, ns);

		m_mutex = CreateMutexW(nullptr, TRUE, mutexName.c_str());
		m_lastError = GetLastError();

		if (!m_mutex)
		{
			m_isFirstInstance = false;
			return;
		}

		m_isFirstInstance = (m_lastError != ERROR_ALREADY_EXISTS);
	}

	SingleInstanceGuard::~SingleInstanceGuard()
	{
		if (m_mutex)
		{
			ReleaseMutex(m_mutex);
			CloseHandle(m_mutex);
			m_mutex = nullptr;
		}
	}

	bool SingleInstanceGuard::isFirstInstance() const
	{
		return m_isFirstInstance;
	}

	bool SingleInstanceGuard::isValid() const
	{
		return m_mutex != nullptr;
	}

	DWORD SingleInstanceGuard::lastError() const
	{
		return m_lastError;
	}

	std::wstring SingleInstanceGuard::buildMutexName(const std::wstring& name,
		Namespace ns)
	{
		switch (ns)
		{
		case Namespace::DefaultLocal:
			return name;

		case Namespace::Local:
			return L"Local\\" + name;

		case Namespace::Global:
			return L"Global\\" + name;
		}

		return name;
	}
}

namespace ytpp {
	namespace sys_core {

		BOOL set_cursorPosOrig(
			_In_ int x,
			_In_ int y)
		{
			return SetCursorPos(x, y);
		} /* set_cursorPosOrig */

		BOOL set_cursorPos(
			_In_ int x, 
			_In_ int y)
		{
			POINT pt;
			GetCursorPos(&pt);
			POINT ptNew = { pt.x, pt.y };
			if (x)ptNew.x = x;
			if (y)ptNew.y = y;
			return SetCursorPos(ptNew.x, ptNew.y);
		} /* set_cursorPos */

		BOOL get_cursorPos(
			_Out_ LONG* x,
			_Out_ LONG* y)
		{
			if (x == NULL || y == NULL) return false;
			POINT pt;
			if (GetCursorPos(&pt)) {
				*x = pt.x; *y = pt.y;
				return true;
			} else {
				*x = 0; *y = 0;
				return false;
			}
			/* return NULL; */
		} /* get_cursorPos */

		LONG get_cursorPosY()
		{
			POINT pt;
			if (GetCursorPos(&pt)) {
				return pt.y;
			} else {
				return -1;
			}
		} /* get_cursorPosY */

		LONG get_cursorPosX()
		{
			POINT pt;
			if (GetCursorPos(&pt)) {
				return pt.x;
			} else {
				return -1;
			}
		} /* get_cursorPosX */

		int get_screenWidth()
		{
			return GetSystemMetrics(SM_CXSCREEN);
		} /* get_screenWidth */

		int get_screenHeight()
		{
			return GetSystemMetrics(SM_CYSCREEN);
		} /* get_screenHeight */

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


		bool is_admin() {
			BOOL isMember = FALSE;
			SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
			PSID adminGroup = nullptr;

			if (!AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
				DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup))
				return false;

			CheckTokenMembership(nullptr, adminGroup, &isMember);
			FreeSid(adminGroup);
			return isMember == TRUE;
		}

		bool restart_as_admin() {
			wchar_t exePath[MAX_PATH]{};
			if (!GetModuleFileNameW(nullptr, exePath, MAX_PATH)) return false;

			SHELLEXECUTEINFOW sei{};
			sei.cbSize = sizeof(sei);
			sei.lpVerb = L"runas";
			sei.lpFile = exePath;
			sei.nShow = SW_SHOWNORMAL;

			if (!ShellExecuteExW(&sei)) return false;
			return true;
		}

	} /* namespace sys_core */
} /* namespace ytpp */