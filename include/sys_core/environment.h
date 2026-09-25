#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ytpp::sys_core
{
	struct EnvEntryA
	{
		std::string name;
		std::string value;
	};

	struct EnvEntryW
	{
		std::wstring name;
		std::wstring value;
	};

	using EnvListA = std::vector<EnvEntryA>;
	using EnvListW = std::vector<EnvEntryW>;
	using PathListA = std::vector<std::string>;
	using PathListW = std::vector<std::wstring>;

#define YTPP_DECLARE_ENV_API(CLASS_NAME) \
	class CLASS_NAME final \
	{ \
	public: \
		CLASS_NAME() = delete; \
		static bool TryGetW(std::wstring_view name, std::wstring& value, DWORD* error = nullptr); \
		static bool TryGetA(std::string_view name, std::string& value, DWORD* error = nullptr); \
		[[nodiscard]] static std::optional<std::wstring> GetW(std::wstring_view name); \
		[[nodiscard]] static std::optional<std::string> GetA(std::string_view name); \
		[[nodiscard]] static std::wstring GetOrW(std::wstring_view name, std::wstring_view fallback = {}); \
		[[nodiscard]] static std::string GetOrA(std::string_view name, std::string_view fallback = {}); \
		[[nodiscard]] static bool ExistsW(std::wstring_view name); \
		[[nodiscard]] static bool ExistsA(std::string_view name); \
		static bool SetW(std::wstring_view name, std::wstring_view value, DWORD* error = nullptr); \
		static bool SetA(std::string_view name, std::string_view value, DWORD* error = nullptr); \
		static bool RemoveW(std::wstring_view name, DWORD* error = nullptr); \
		static bool RemoveA(std::string_view name, DWORD* error = nullptr); \
		static bool TryExpandW(std::wstring_view value, std::wstring& result, DWORD* error = nullptr); \
		static bool TryExpandA(std::string_view value, std::string& result, DWORD* error = nullptr); \
		[[nodiscard]] static std::optional<std::wstring> ExpandW(std::wstring_view value); \
		[[nodiscard]] static std::optional<std::string> ExpandA(std::string_view value); \
		static bool TryListW(EnvListW& result, bool includeHidden = false, DWORD* error = nullptr); \
		static bool TryListA(EnvListA& result, bool includeHidden = false, DWORD* error = nullptr); \
		[[nodiscard]] static EnvListW ListW(bool includeHidden = false); \
		[[nodiscard]] static EnvListA ListA(bool includeHidden = false); \
		static bool TryGetPathW(PathListW& result, DWORD* error = nullptr); \
		static bool TryGetPathA(PathListA& result, DWORD* error = nullptr); \
		static bool TryGetExpandedPathW(PathListW& result, DWORD* error = nullptr); \
		static bool TryGetExpandedPathA(PathListA& result, DWORD* error = nullptr); \
		static bool TryGetUniquePathW(PathListW& result, DWORD* error = nullptr); \
		static bool TryGetUniquePathA(PathListA& result, DWORD* error = nullptr); \
		static bool TryGetExpandedUniquePathW(PathListW& result, DWORD* error = nullptr); \
		static bool TryGetExpandedUniquePathA(PathListA& result, DWORD* error = nullptr); \
		static bool TryGetExistingPathW(PathListW& result, DWORD* error = nullptr); \
		static bool TryGetExistingPathA(PathListA& result, DWORD* error = nullptr); \
		static bool TryGetExpandedExistingPathW(PathListW& result, DWORD* error = nullptr); \
		static bool TryGetExpandedExistingPathA(PathListA& result, DWORD* error = nullptr); \
		[[nodiscard]] static PathListW GetPathW(); \
		[[nodiscard]] static PathListA GetPathA(); \
		[[nodiscard]] static PathListW GetExpandedPathW(); \
		[[nodiscard]] static PathListA GetExpandedPathA(); \
		[[nodiscard]] static PathListW GetUniquePathW(); \
		[[nodiscard]] static PathListA GetUniquePathA(); \
		[[nodiscard]] static PathListW GetExpandedUniquePathW(); \
		[[nodiscard]] static PathListA GetExpandedUniquePathA(); \
		[[nodiscard]] static PathListW GetExistingPathW(); \
		[[nodiscard]] static PathListA GetExistingPathA(); \
		[[nodiscard]] static PathListW GetExpandedExistingPathW(); \
		[[nodiscard]] static PathListA GetExpandedExistingPathA(); \
	};

	YTPP_DECLARE_ENV_API(ProcessEnv)
		YTPP_DECLARE_ENV_API(UserEnv)
		YTPP_DECLARE_ENV_API(MachineEnv)

#undef YTPP_DECLARE_ENV_API
}
