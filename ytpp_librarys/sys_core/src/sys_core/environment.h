#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <optional>
#include <sal.h>
#include <string>
#include <string_view>
#include <vector>

namespace ytpp::sys_core::environment {
struct EnvironmentEntryA {
    std::string name;
    std::string value;
};

struct EnvironmentEntryW {
    std::wstring name;
    std::wstring value;
};

using EnvironmentListA = std::vector<EnvironmentEntryA>;
using EnvironmentListW = std::vector<EnvironmentEntryW>;
using PathListA = std::vector<std::string>;
using PathListW = std::vector<std::wstring>;

#define YTPP_DECLARE_ENV_API(CLASS_NAME)                                                                                                       \
    class CLASS_NAME final {                                                                                                                   \
      public:                                                                                                                                  \
        /** @brief 禁止创建仅包含静态环境变量操作的工具类实例。 */                                                                             \
        CLASS_NAME() = delete;                                                                                                                 \
        /** @brief 读取宽字符环境变量。@param[in] name 变量名。@param[out] value 接收变量值。@param[out] error             \
         * 可选错误码。@return 成功返回true。 */                                                                                    \
        static bool TryGetW(_In_ std::wstring_view name, _Out_ std::wstring& value, _Out_opt_ DWORD* error = nullptr);                         \
        /** @brief 读取窄字符环境变量。@param[in] name 变量名。@param[out] value 接收变量值。@param[out] error             \
         * 可选错误码。@return 成功返回true。 */                                                                                    \
        static bool TryGetA(_In_ std::string_view name, _Out_ std::string& value, _Out_opt_ DWORD* error = nullptr);                           \
        /** @brief 读取宽字符环境变量。@param[in] name 变量名。@return 存在时返回变量值。 */                                                   \
        [[nodiscard]] static std::optional<std::wstring> GetW(_In_ std::wstring_view name);                                                    \
        /** @brief 读取窄字符环境变量。@param[in] name 变量名。@return 存在时返回变量值。 */                                                   \
        [[nodiscard]] static std::optional<std::string> GetA(_In_ std::string_view name);                                                      \
        /** @brief 读取宽字符环境变量或返回默认值。@param[in] name 变量名。@param[in] fallback 默认值。@return         \
         * 变量值或默认值。 */                                                                                                         \
        [[nodiscard]] static std::wstring GetOrW(_In_ std::wstring_view name, _In_ std::wstring_view fallback = {});                           \
        /** @brief 读取窄字符环境变量或返回默认值。@param[in] name 变量名。@param[in] fallback 默认值。@return         \
         * 变量值或默认值。 */                                                                                                         \
        [[nodiscard]] static std::string GetOrA(_In_ std::string_view name, _In_ std::string_view fallback = {});                              \
        /** @brief 判断宽字符环境变量是否存在。@param[in] name 变量名。@return 存在时返回true。 */                                             \
        [[nodiscard]] static bool ExistsW(_In_ std::wstring_view name);                                                                        \
        /** @brief 判断窄字符环境变量是否存在。@param[in] name 变量名。@return 存在时返回true。 */                                             \
        [[nodiscard]] static bool ExistsA(_In_ std::string_view name);                                                                         \
        /** @brief 设置宽字符环境变量。@param[in] name 变量名。@param[in] value 新值。@param[out] error                       \
         * 可选错误码。@return 成功返回true。 */                                                                                    \
        static bool SetW(_In_ std::wstring_view name, _In_ std::wstring_view value, _Out_opt_ DWORD* error = nullptr);                         \
        /** @brief 设置窄字符环境变量。@param[in] name 变量名。@param[in] value 新值。@param[out] error                       \
         * 可选错误码。@return 成功返回true。 */                                                                                    \
        static bool SetA(_In_ std::string_view name, _In_ std::string_view value, _Out_opt_ DWORD* error = nullptr);                           \
        /** @brief 删除宽字符环境变量。@param[in] name 变量名。@param[out] error 可选错误码。@return 成功返回true。 */                         \
        static bool RemoveW(_In_ std::wstring_view name, _Out_opt_ DWORD* error = nullptr);                                                    \
        /** @brief 删除窄字符环境变量。@param[in] name 变量名。@param[out] error 可选错误码。@return 成功返回true。 */                         \
        static bool RemoveA(_In_ std::string_view name, _Out_opt_ DWORD* error = nullptr);                                                     \
        /** @brief 展开宽字符环境变量引用。@param[in] value 原始文本。@param[out] result 展开结果。@param[out] error     \
         * 可选错误码。@return 成功返回true。 */                                                                                    \
        static bool TryExpandW(_In_ std::wstring_view value, _Out_ std::wstring& result,                                                       \
                               _Out_opt_ DWORD* error = nullptr);                                                                              \
        /** @brief 展开窄字符环境变量引用。@param[in] value 原始文本。@param[out] result 展开结果。@param[out] error     \
         * 可选错误码。@return 成功返回true。 */                                                                                    \
        static bool TryExpandA(_In_ std::string_view value, _Out_ std::string& result,                                                         \
                               _Out_opt_ DWORD* error = nullptr);                                                                              \
        /** @brief 展开宽字符环境变量引用。@param[in] value 原始文本。@return 成功时返回展开结果。 */                                          \
        [[nodiscard]] static std::optional<std::wstring> ExpandW(_In_ std::wstring_view value);                                                \
        /** @brief 展开窄字符环境变量引用。@param[in] value 原始文本。@return 成功时返回展开结果。 */                                          \
        [[nodiscard]] static std::optional<std::string> ExpandA(_In_ std::string_view value);                                                  \
        /** @brief 枚举宽字符环境变量。@param[out] result 接收变量列表。@param[in] includeHidden                              \
         * 是否包含隐藏项。@param[out] error 可选错误码。@return 成功返回true。 */                                          \
        static bool TryListW(_Out_ EnvironmentListW& result, _In_ bool includeHidden = false,                                                  \
                             _Out_opt_ DWORD* error = nullptr);                                                                                \
        /** @brief 枚举窄字符环境变量。@param[out] result 接收变量列表。@param[in] includeHidden                              \
         * 是否包含隐藏项。@param[out] error 可选错误码。@return 成功返回true。 */                                          \
        static bool TryListA(_Out_ EnvironmentListA& result, _In_ bool includeHidden = false,                                                  \
                             _Out_opt_ DWORD* error = nullptr);                                                                                \
        /** @brief 返回宽字符环境变量列表。@param[in] includeHidden 是否包含隐藏项。@return 环境变量列表。 */                                  \
        [[nodiscard]] static EnvironmentListW ListW(_In_ bool includeHidden = false);                                                          \
        /** @brief 返回窄字符环境变量列表。@param[in] includeHidden 是否包含隐藏项。@return 环境变量列表。 */                                  \
        [[nodiscard]] static EnvironmentListA ListA(_In_ bool includeHidden = false);                                                          \
        /** @brief 读取宽字符PATH列表。@param[out] result 接收路径。@param[out] error 可选错误码。@return                   \
         * 成功返回true。 */                                                                                                              \
        static bool TryGetPathW(_Out_ PathListW& result, _Out_opt_ DWORD* error = nullptr);                                                    \
        /** @brief 读取窄字符PATH列表。@param[out] result 接收路径。@param[out] error 可选错误码。@return                   \
         * 成功返回true。 */                                                                                                              \
        static bool TryGetPathA(_Out_ PathListA& result, _Out_opt_ DWORD* error = nullptr);                                                    \
        /** @brief 读取并展开宽字符PATH列表。@param[out] result 接收路径。@param[out] error 可选错误码。@return          \
         * 成功返回true。 */                                                                                                              \
        static bool TryGetExpandedPathW(_Out_ PathListW& result, _Out_opt_ DWORD* error = nullptr);                                            \
        /** @brief 读取并展开窄字符PATH列表。@param[out] result 接收路径。@param[out] error 可选错误码。@return          \
         * 成功返回true。 */                                                                                                              \
        static bool TryGetExpandedPathA(_Out_ PathListA& result, _Out_opt_ DWORD* error = nullptr);                                            \
        /** @brief 读取去重后的宽字符PATH列表。@param[out] result 接收路径。@param[out] error 可选错误码。@return       \
         * 成功返回true。 */                                                                                                              \
        static bool TryGetUniquePathW(_Out_ PathListW& result, _Out_opt_ DWORD* error = nullptr);                                              \
        /** @brief 读取去重后的窄字符PATH列表。@param[out] result 接收路径。@param[out] error 可选错误码。@return       \
         * 成功返回true。 */                                                                                                              \
        static bool TryGetUniquePathA(_Out_ PathListA& result, _Out_opt_ DWORD* error = nullptr);                                              \
        /** @brief 读取展开并去重的宽字符PATH列表。@param[out] result 接收路径。@param[out] error 可选错误码。@return \
         * 成功返回true。 */                                                                                                              \
        static bool TryGetExpandedUniquePathW(_Out_ PathListW& result, _Out_opt_ DWORD* error = nullptr);                                      \
        /** @brief 读取展开并去重的窄字符PATH列表。@param[out] result 接收路径。@param[out] error 可选错误码。@return \
         * 成功返回true。 */                                                                                                              \
        static bool TryGetExpandedUniquePathA(_Out_ PathListA& result, _Out_opt_ DWORD* error = nullptr);                                      \
        /** @brief 读取仅包含现有目录的宽字符PATH列表。@param[out] result 接收路径。@param[out] error                     \
         * 可选错误码。@return 成功返回true。 */                                                                                    \
        static bool TryGetExistingPathW(_Out_ PathListW& result, _Out_opt_ DWORD* error = nullptr);                                            \
        /** @brief 读取仅包含现有目录的窄字符PATH列表。@param[out] result 接收路径。@param[out] error                     \
         * 可选错误码。@return 成功返回true。 */                                                                                    \
        static bool TryGetExistingPathA(_Out_ PathListA& result, _Out_opt_ DWORD* error = nullptr);                                            \
        /** @brief 读取展开、去重且存在的宽字符PATH列表。@param[out] result 接收路径。@param[out] error                  \
         * 可选错误码。@return 成功返回true。 */                                                                                    \
        static bool TryGetExpandedExistingPathW(_Out_ PathListW& result, _Out_opt_ DWORD* error = nullptr);                                    \
        /** @brief 读取展开、去重且存在的窄字符PATH列表。@param[out] result 接收路径。@param[out] error                  \
         * 可选错误码。@return 成功返回true。 */                                                                                    \
        static bool TryGetExpandedExistingPathA(_Out_ PathListA& result, _Out_opt_ DWORD* error = nullptr);                                    \
        /** @brief 返回宽字符PATH列表。@return PATH路径列表。 */                                                                               \
        [[nodiscard]] static PathListW GetPathW();                                                                                             \
        /** @brief 返回窄字符PATH列表。@return PATH路径列表。 */                                                                               \
        [[nodiscard]] static PathListA GetPathA();                                                                                             \
        /** @brief 返回展开后的宽字符PATH列表。@return PATH路径列表。 */                                                                       \
        [[nodiscard]] static PathListW GetExpandedPathW();                                                                                     \
        /** @brief 返回展开后的窄字符PATH列表。@return PATH路径列表。 */                                                                       \
        [[nodiscard]] static PathListA GetExpandedPathA();                                                                                     \
        /** @brief 返回去重后的宽字符PATH列表。@return PATH路径列表。 */                                                                       \
        [[nodiscard]] static PathListW GetUniquePathW();                                                                                       \
        /** @brief 返回去重后的窄字符PATH列表。@return PATH路径列表。 */                                                                       \
        [[nodiscard]] static PathListA GetUniquePathA();                                                                                       \
        /** @brief 返回展开并去重的宽字符PATH列表。@return PATH路径列表。 */                                                                   \
        [[nodiscard]] static PathListW GetExpandedUniquePathW();                                                                               \
        /** @brief 返回展开并去重的窄字符PATH列表。@return PATH路径列表。 */                                                                   \
        [[nodiscard]] static PathListA GetExpandedUniquePathA();                                                                               \
        /** @brief 返回仅包含现有目录的宽字符PATH列表。@return PATH路径列表。 */                                                               \
        [[nodiscard]] static PathListW GetExistingPathW();                                                                                     \
        /** @brief 返回仅包含现有目录的窄字符PATH列表。@return PATH路径列表。 */                                                               \
        [[nodiscard]] static PathListA GetExistingPathA();                                                                                     \
        /** @brief 返回展开、去重且存在的宽字符PATH列表。@return PATH路径列表。 */                                                             \
        [[nodiscard]] static PathListW GetExpandedExistingPathW();                                                                             \
        /** @brief 返回展开、去重且存在的窄字符PATH列表。@return PATH路径列表。 */                                                             \
        [[nodiscard]] static PathListA GetExpandedExistingPathA();                                                                             \
    };

YTPP_DECLARE_ENV_API(ProcessEnvironment)
YTPP_DECLARE_ENV_API(UserEnvironment)
YTPP_DECLARE_ENV_API(MachineEnvironment)

#undef YTPP_DECLARE_ENV_API
} // namespace ytpp::sys_core::environment
