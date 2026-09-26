#pragma once

#include <optional>
#include <sal.h>
#include <string>
#include <windows.h>

namespace ytpp::sys_core::sys_processing {

/// @brief 使用命名互斥体判断当前应用是否为首个实例。
class SingleInstanceGuard {
  public:
    /// @brief 指定互斥体使用的Windows对象命名空间。
    enum class MutexNamespace {
        DefaultLocal,
        Local,
        Global
    };

    /// @brief 创建单实例守卫并尝试取得命名互斥体。
    /// @param[in] applicationName 用于构造互斥体名称的应用名称。
    /// @param[in] mutexNamespace Windows对象命名空间。
    explicit SingleInstanceGuard(_In_ const std::wstring& applicationName,
                                 _In_ MutexNamespace mutexNamespace = MutexNamespace::Local);

    /// @brief 释放当前实例持有的互斥体句柄。
    ~SingleInstanceGuard();

    /// @brief 禁止复制守卫对象。
    /// @param[in] other 不允许复制的源对象。
    SingleInstanceGuard(_In_ const SingleInstanceGuard& other) = delete;

    /// @brief 禁止复制赋值。
    /// @param[in] other 不允许复制的源对象。
    /// @return 当前对象引用；该函数已删除，不能调用。
    SingleInstanceGuard& operator=(_In_ const SingleInstanceGuard& other) = delete;

    /// @brief 判断当前进程是否创建了该名称的首个实例。
    /// @return 当前进程为首个实例时返回true。
    [[nodiscard]] bool IsFirstInstance() const noexcept;

    /// @brief 判断互斥体句柄是否成功创建。
    /// @return 守卫有效时返回true。
    [[nodiscard]] bool IsValid() const noexcept;

    /// @brief 获取创建互斥体后的Windows错误码。
    /// @return `CreateMutexW`调用后的错误码。
    [[nodiscard]] DWORD GetLastError() const noexcept;

  private:
    /// @brief 构造包含Windows命名空间前缀的互斥体名称。
    /// @param[in] name 基础名称。
    /// @param[in] mutexNamespace Windows对象命名空间。
    /// @return 完整互斥体名称。
    static std::wstring BuildMutexName(_In_ const std::wstring& name, _In_ MutexNamespace mutexNamespace);

    HANDLE mutex_ = nullptr;
    bool isFirstInstance_ = false;
    DWORD lastError_ = ERROR_SUCCESS;
};

/// @brief 直接设置鼠标光标位置。
/// @param[in] x 水平屏幕坐标。
/// @param[in] y 垂直屏幕坐标。
/// @return Windows `SetCursorPos`的结果。
BOOL SetCursorPosition(_In_ int x, _In_ int y);

/// @brief 仅更新给出的光标坐标分量。
/// @param[in] x 可选水平坐标；空值表示保留当前位置。
/// @param[in] y 可选垂直坐标；空值表示保留当前位置。
/// @return 成功读取并设置光标位置时返回TRUE。
BOOL UpdateCursorPosition(_In_ std::optional<int> x, _In_ std::optional<int> y);

/// @brief 获取当前鼠标光标位置。
/// @param[out] x 接收水平屏幕坐标。
/// @param[out] y 接收垂直屏幕坐标。
/// @return 获取成功时返回TRUE。
BOOL GetCursorPosition(_Out_ LONG* x, _Out_ LONG* y);

/// @brief 获取当前鼠标光标的垂直坐标。
/// @return 成功时返回坐标，失败时返回-1。
LONG GetCursorPositionY();

/// @brief 获取当前鼠标光标的水平坐标。
/// @return 成功时返回坐标，失败时返回-1。
LONG GetCursorPositionX();

/// @brief 获取主显示器宽度。
/// @return 主显示器宽度，单位为像素。
int GetPrimaryScreenWidth();

/// @brief 获取主显示器高度。
/// @return 主显示器高度，单位为像素。
int GetPrimaryScreenHeight();

/// @brief 判断当前进程是否以管理员权限运行。
/// @return 进程令牌已提升时返回true。
bool IsRunningAsAdministrator();

/// @brief 通过UAC请求以管理员权限重新启动当前程序。
/// @return 成功发起提升进程时返回true。
bool RestartAsAdministrator();

} // namespace ytpp::sys_core::sys_processing
