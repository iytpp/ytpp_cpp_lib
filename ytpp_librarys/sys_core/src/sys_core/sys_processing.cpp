#include "sys_core/sys_processing.h"

#include <shellapi.h>

namespace ytpp::sys_core::sys_processing {

SingleInstanceGuard::SingleInstanceGuard(_In_ const std::wstring& applicationName, _In_ MutexNamespace mutexNamespace) {
    const std::wstring mutexName = BuildMutexName(applicationName, mutexNamespace);
    mutex_ = ::CreateMutexW(nullptr, TRUE, mutexName.c_str());
    lastError_ = ::GetLastError();
    if (mutex_ == nullptr) {
        isFirstInstance_ = false;
        return;
    }
    isFirstInstance_ = lastError_ != ERROR_ALREADY_EXISTS;
}

SingleInstanceGuard::~SingleInstanceGuard() {
    if (mutex_ != nullptr) {
        ::ReleaseMutex(mutex_);
        ::CloseHandle(mutex_);
        mutex_ = nullptr;
    }
}

bool SingleInstanceGuard::IsFirstInstance() const noexcept {
    return isFirstInstance_;
}

bool SingleInstanceGuard::IsValid() const noexcept {
    return mutex_ != nullptr;
}

DWORD SingleInstanceGuard::GetLastError() const noexcept {
    return lastError_;
}

std::wstring SingleInstanceGuard::BuildMutexName(_In_ const std::wstring& name, _In_ MutexNamespace mutexNamespace) {
    switch (mutexNamespace) {
    case MutexNamespace::DefaultLocal:
        return name;
    case MutexNamespace::Local:
        return L"Local\\" + name;
    case MutexNamespace::Global:
        return L"Global\\" + name;
    }
    return name;
}

BOOL SetCursorPosition(_In_ int x, _In_ int y) {
    return ::SetCursorPos(x, y);
}

BOOL UpdateCursorPosition(_In_ std::optional<int> x, _In_ std::optional<int> y) {
    POINT currentPosition{};
    if (!::GetCursorPos(&currentPosition)) {
        return FALSE;
    }
    const int targetX = x.value_or(currentPosition.x);
    const int targetY = y.value_or(currentPosition.y);
    return ::SetCursorPos(targetX, targetY);
}

BOOL GetCursorPosition(_Out_ LONG* x, _Out_ LONG* y) {
    if (x == nullptr || y == nullptr) {
        return FALSE;
    }

    POINT position{};
    if (!::GetCursorPos(&position)) {
        *x = 0;
        *y = 0;
        return FALSE;
    }
    *x = position.x;
    *y = position.y;
    return TRUE;
}

LONG GetCursorPositionY() {
    POINT position{};
    return ::GetCursorPos(&position) ? position.y : -1;
}

LONG GetCursorPositionX() {
    POINT position{};
    return ::GetCursorPos(&position) ? position.x : -1;
}

int GetPrimaryScreenWidth() {
    return ::GetSystemMetrics(SM_CXSCREEN);
}

int GetPrimaryScreenHeight() {
    return ::GetSystemMetrics(SM_CYSCREEN);
}

bool IsRunningAsAdministrator() {
    HANDLE token = nullptr;
    if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return false;
    }

    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    const BOOL succeeded = ::GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size);
    ::CloseHandle(token);
    return succeeded != FALSE && elevation.TokenIsElevated != 0;
}

bool RestartAsAdministrator() {
    wchar_t executablePath[MAX_PATH]{};
    if (::GetModuleFileNameW(nullptr, executablePath, MAX_PATH) == 0) {
        return false;
    }

    std::wstring directory = executablePath;
    const std::size_t separator = directory.find_last_of(L"\\/");
    if (separator != std::wstring::npos) {
        directory.resize(separator);
    }

    SHELLEXECUTEINFOW executeInfo{};
    executeInfo.cbSize = sizeof(executeInfo);
    executeInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
    executeInfo.lpVerb = L"runas";
    executeInfo.lpFile = executablePath;
    executeInfo.lpDirectory = directory.c_str();
    executeInfo.nShow = SW_SHOWNORMAL;
    if (!::ShellExecuteExW(&executeInfo)) {
        return false;
    }
    if (executeInfo.hProcess != nullptr) {
        ::CloseHandle(executeInfo.hProcess);
    }
    return true;
}

} // namespace ytpp::sys_core::sys_processing
