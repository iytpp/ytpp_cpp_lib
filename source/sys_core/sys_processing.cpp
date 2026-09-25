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

		bool is_admin() {
			HANDLE token = nullptr;
			if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;

			TOKEN_ELEVATION elevation{};
			DWORD size = 0;
			BOOL ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size);
			CloseHandle(token);

			return ok && elevation.TokenIsElevated;
		}

		bool restart_as_admin() {
			wchar_t exePath[MAX_PATH]{};
			if (!GetModuleFileNameW(nullptr, exePath, MAX_PATH)) return false;

			std::wstring dir = exePath;
			size_t pos = dir.find_last_of(L"\\/");
			if (pos != std::wstring::npos) dir.resize(pos);

			SHELLEXECUTEINFOW sei{};
			sei.cbSize = sizeof(sei);
			sei.fMask = SEE_MASK_NOCLOSEPROCESS;
			sei.lpVerb = L"runas";
			sei.lpFile = exePath;
			sei.lpDirectory = dir.c_str();
			sei.nShow = SW_SHOWNORMAL;

			if (!ShellExecuteExW(&sei)) return false;

			if (sei.hProcess) CloseHandle(sei.hProcess);
			return true;
		}

	} /* namespace sys_core */
} /* namespace ytpp */