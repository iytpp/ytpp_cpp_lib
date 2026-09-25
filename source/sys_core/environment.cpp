#include "sys_core/environment.h"

#include <algorithm>
#include <cwchar>
#include <limits>
#include <utility>

namespace ytpp::sys_core
{
	namespace
	{
		enum class EnvScope
		{
			Process,
			User,
			Machine
		};

		enum class PathMode
		{
			Raw,
			Expanded,
			Unique,
			ExpandedUnique,
			Existing,
			ExpandedExisting
		};

		constexpr int kMaxReadRetry = 8;
		constexpr int kMaxExpandDepth = 16;
		constexpr wchar_t kUserEnvKey[] = L"Environment";
		constexpr wchar_t kMachineEnvKey[] =
			L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment";

		void SetErrorImpl(DWORD* error, DWORD value) noexcept
		{
			if (error)
				*error = value;
			::SetLastError(value);
		}

		void SetSuccessImpl(DWORD* error) noexcept
		{
			if (error)
				*error = ERROR_SUCCESS;
			::SetLastError(ERROR_SUCCESS);
		}

		DWORD NormalizeRegistryErrorImpl(LSTATUS status) noexcept
		{
			if (status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND)
				return ERROR_ENVVAR_NOT_FOUND;
			return static_cast<DWORD>(status);
		}

		bool ContainsNullImpl(std::wstring_view value) noexcept
		{
			return value.find(L'\0') != std::wstring_view::npos;
		}

		bool ValidNameImpl(std::wstring_view name) noexcept
		{
			return !name.empty() &&
				!ContainsNullImpl(name) &&
				name.find(L'=') == std::wstring_view::npos;
		}

		bool EqualIImpl(std::wstring_view a, std::wstring_view b) noexcept
		{
			constexpr auto maxInt = static_cast<size_t>((std::numeric_limits<int>::max)());
			if (a.size() > maxInt || b.size() > maxInt)
				return false;

			return ::CompareStringOrdinal(
				a.data(), static_cast<int>(a.size()),
				b.data(), static_cast<int>(b.size()),
				TRUE) == CSTR_EQUAL;
		}

		bool TryAtoWImpl(std::string_view value, std::wstring& result, DWORD* error)
		{
			result.clear();
			if (value.empty())
			{
				SetSuccessImpl(error);
				return true;
			}

			constexpr auto maxInt = static_cast<size_t>((std::numeric_limits<int>::max)());
			if (value.size() > maxInt)
			{
				SetErrorImpl(error, ERROR_INVALID_PARAMETER);
				return false;
			}

			const int size = ::MultiByteToWideChar(
				CP_ACP, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);

			if (size <= 0)
			{
				SetErrorImpl(error, ::GetLastError());
				return false;
			}

			result.resize(static_cast<size_t>(size));
			if (::MultiByteToWideChar(
				CP_ACP, 0, value.data(), static_cast<int>(value.size()),
				result.data(), size) != size)
			{
				SetErrorImpl(error, ::GetLastError());
				result.clear();
				return false;
			}

			SetSuccessImpl(error);
			return true;
		}

		bool TryWtoAImpl(std::wstring_view value, std::string& result, DWORD* error)
		{
			result.clear();
			if (value.empty())
			{
				SetSuccessImpl(error);
				return true;
			}

			constexpr auto maxInt = static_cast<size_t>((std::numeric_limits<int>::max)());
			if (value.size() > maxInt)
			{
				SetErrorImpl(error, ERROR_INVALID_PARAMETER);
				return false;
			}

			const int size = ::WideCharToMultiByte(
				CP_ACP, 0, value.data(), static_cast<int>(value.size()),
				nullptr, 0, nullptr, nullptr);

			if (size <= 0)
			{
				SetErrorImpl(error, ::GetLastError());
				return false;
			}

			result.resize(static_cast<size_t>(size));
			if (::WideCharToMultiByte(
				CP_ACP, 0, value.data(), static_cast<int>(value.size()),
				result.data(), size, nullptr, nullptr) != size)
			{
				SetErrorImpl(error, ::GetLastError());
				result.clear();
				return false;
			}

			SetSuccessImpl(error);
			return true;
		}

		HKEY RegistryRootImpl(EnvScope scope) noexcept
		{
			return scope == EnvScope::User ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
		}

		const wchar_t* RegistryPathImpl(EnvScope scope) noexcept
		{
			return scope == EnvScope::User ? kUserEnvKey : kMachineEnvKey;
		}

		bool TryGetProcessWImpl(std::wstring_view name, std::wstring& value, DWORD* error)
		{
			value.clear();
			if (!ValidNameImpl(name))
			{
				SetErrorImpl(error, ERROR_INVALID_PARAMETER);
				return false;
			}

			const std::wstring key(name);

			for (int retry = 0; retry < kMaxReadRetry; ++retry)
			{
				::SetLastError(ERROR_SUCCESS);
				const DWORD required = ::GetEnvironmentVariableW(key.c_str(), nullptr, 0);

				if (!required)
				{
					const DWORD e = ::GetLastError();
					if (e == ERROR_SUCCESS)
					{
						value.clear();
						SetSuccessImpl(error);
						return true;
					}

					SetErrorImpl(error, e);
					return false;
				}

				std::wstring buffer(required, L'\0');
				::SetLastError(ERROR_SUCCESS);

				const DWORD written = ::GetEnvironmentVariableW(
					key.c_str(), buffer.data(), static_cast<DWORD>(buffer.size()));

				if (!written)
				{
					const DWORD e = ::GetLastError();
					if (e == ERROR_SUCCESS)
					{
						value.clear();
						SetSuccessImpl(error);
						return true;
					}

					SetErrorImpl(error, e);
					return false;
				}

				if (written < buffer.size())
				{
					buffer.resize(written);
					value = std::move(buffer);
					SetSuccessImpl(error);
					return true;
				}
			}

			SetErrorImpl(error, ERROR_RETRY);
			return false;
		}

		bool SetProcessWImpl(std::wstring_view name, std::wstring_view value, DWORD* error)
		{
			if (!ValidNameImpl(name) || ContainsNullImpl(value))
			{
				SetErrorImpl(error, ERROR_INVALID_PARAMETER);
				return false;
			}

			const std::wstring n(name);
			const std::wstring v(value);

			if (!::SetEnvironmentVariableW(n.c_str(), v.c_str()))
			{
				SetErrorImpl(error, ::GetLastError());
				return false;
			}

			SetSuccessImpl(error);
			return true;
		}

		bool RemoveProcessWImpl(std::wstring_view name, DWORD* error)
		{
			if (!ValidNameImpl(name))
			{
				SetErrorImpl(error, ERROR_INVALID_PARAMETER);
				return false;
			}

			const std::wstring n(name);
			if (!::SetEnvironmentVariableW(n.c_str(), nullptr))
			{
				SetErrorImpl(error, ::GetLastError());
				return false;
			}

			SetSuccessImpl(error);
			return true;
		}

		bool TryGetRegistryWImpl(EnvScope scope, std::wstring_view name, std::wstring& value, DWORD* error)
		{
			value.clear();
			if (!ValidNameImpl(name))
			{
				SetErrorImpl(error, ERROR_INVALID_PARAMETER);
				return false;
			}

			HKEY key = nullptr;
			LSTATUS status = ::RegOpenKeyExW(
				RegistryRootImpl(scope), RegistryPathImpl(scope),
				0, KEY_QUERY_VALUE, &key);

			if (status != ERROR_SUCCESS)
			{
				SetErrorImpl(error, NormalizeRegistryErrorImpl(status));
				return false;
			}

			const std::wstring n(name);
			DWORD type = 0;
			DWORD bytes = 0;

			status = ::RegQueryValueExW(key, n.c_str(), nullptr, &type, nullptr, &bytes);
			if (status != ERROR_SUCCESS)
			{
				::RegCloseKey(key);
				SetErrorImpl(error, NormalizeRegistryErrorImpl(status));
				return false;
			}

			if (type != REG_SZ && type != REG_EXPAND_SZ)
			{
				::RegCloseKey(key);
				SetErrorImpl(error, ERROR_DATATYPE_MISMATCH);
				return false;
			}

			std::wstring buffer(static_cast<size_t>(bytes / sizeof(wchar_t)) + 1, L'\0');
			DWORD readBytes = static_cast<DWORD>(buffer.size() * sizeof(wchar_t));

			status = ::RegQueryValueExW(
				key, n.c_str(), nullptr, &type,
				reinterpret_cast<LPBYTE>(buffer.data()), &readBytes);

			::RegCloseKey(key);

			if (status != ERROR_SUCCESS)
			{
				SetErrorImpl(error, NormalizeRegistryErrorImpl(status));
				return false;
			}

			size_t chars = readBytes / sizeof(wchar_t);
			while (chars && buffer[chars - 1] == L'\0')
				--chars;

			buffer.resize(chars);
			value = std::move(buffer);
			SetSuccessImpl(error);
			return true;
		}

		bool HasExpandableReferenceImpl(std::wstring_view value) noexcept
		{
			size_t pos = 0;
			while ((pos = value.find(L'%', pos)) != std::wstring_view::npos)
			{
				const size_t end = value.find(L'%', pos + 1);
				if (end == std::wstring_view::npos)
					return false;
				if (end > pos + 1)
					return true;
				pos = end + 1;
			}

			return false;
		}

		void BroadcastEnvironmentChangeImpl() noexcept
		{
			DWORD_PTR result = 0;
			::SendMessageTimeoutW(
				HWND_BROADCAST, WM_SETTINGCHANGE, 0,
				reinterpret_cast<LPARAM>(L"Environment"),
				SMTO_ABORTIFHUNG, 1500, &result);
		}

		bool SetRegistryWImpl(EnvScope scope, std::wstring_view name, std::wstring_view value, DWORD* error)
		{
			if (!ValidNameImpl(name) || ContainsNullImpl(value))
			{
				SetErrorImpl(error, ERROR_INVALID_PARAMETER);
				return false;
			}

			constexpr auto maxDword = static_cast<size_t>((std::numeric_limits<DWORD>::max)());
			if (value.size() > (maxDword / sizeof(wchar_t)) - 1)
			{
				SetErrorImpl(error, ERROR_INVALID_PARAMETER);
				return false;
			}

			HKEY key = nullptr;
			LSTATUS status = ::RegCreateKeyExW(
				RegistryRootImpl(scope), RegistryPathImpl(scope),
				0, nullptr, REG_OPTION_NON_VOLATILE,
				KEY_SET_VALUE, nullptr, &key, nullptr);

			if (status != ERROR_SUCCESS)
			{
				SetErrorImpl(error, static_cast<DWORD>(status));
				return false;
			}

			const std::wstring n(name);
			const std::wstring v(value);
			const DWORD type = HasExpandableReferenceImpl(value) ? REG_EXPAND_SZ : REG_SZ;
			const DWORD bytes = static_cast<DWORD>((v.size() + 1) * sizeof(wchar_t));

			status = ::RegSetValueExW(
				key, n.c_str(), 0, type,
				reinterpret_cast<const BYTE*>(v.c_str()), bytes);

			::RegCloseKey(key);

			if (status != ERROR_SUCCESS)
			{
				SetErrorImpl(error, static_cast<DWORD>(status));
				return false;
			}

			BroadcastEnvironmentChangeImpl();
			SetSuccessImpl(error);
			return true;
		}

		bool RemoveRegistryWImpl(EnvScope scope, std::wstring_view name, DWORD* error)
		{
			if (!ValidNameImpl(name))
			{
				SetErrorImpl(error, ERROR_INVALID_PARAMETER);
				return false;
			}

			HKEY key = nullptr;
			LSTATUS status = ::RegOpenKeyExW(
				RegistryRootImpl(scope), RegistryPathImpl(scope),
				0, KEY_SET_VALUE, &key);

			if (status != ERROR_SUCCESS)
			{
				SetErrorImpl(error, NormalizeRegistryErrorImpl(status));
				return false;
			}

			const std::wstring n(name);
			status = ::RegDeleteValueW(key, n.c_str());
			::RegCloseKey(key);

			if (status != ERROR_SUCCESS)
			{
				SetErrorImpl(error, NormalizeRegistryErrorImpl(status));
				return false;
			}

			BroadcastEnvironmentChangeImpl();
			SetSuccessImpl(error);
			return true;
		}

		bool TryGetScopeWImpl(EnvScope scope, std::wstring_view name, std::wstring& value, DWORD* error)
		{
			if (scope == EnvScope::Process)
				return TryGetProcessWImpl(name, value, error);
			return TryGetRegistryWImpl(scope, name, value, error);
		}

		bool SetScopeWImpl(EnvScope scope, std::wstring_view name, std::wstring_view value, DWORD* error)
		{
			if (scope == EnvScope::Process)
				return SetProcessWImpl(name, value, error);
			return SetRegistryWImpl(scope, name, value, error);
		}

		bool RemoveScopeWImpl(EnvScope scope, std::wstring_view name, DWORD* error)
		{
			if (scope == EnvScope::Process)
				return RemoveProcessWImpl(name, error);
			return RemoveRegistryWImpl(scope, name, error);
		}

		bool ResolveVariableImpl(
			EnvScope scope, std::wstring_view name,
			std::wstring& value, bool& found, DWORD* error)
		{
			found = false;

			auto tryScope = [&](EnvScope candidate) -> bool
			{
				DWORD e = ERROR_SUCCESS;
				if (TryGetScopeWImpl(candidate, name, value, &e))
				{
					found = true;
					return true;
				}

				if (e == ERROR_ENVVAR_NOT_FOUND)
					return true;

				SetErrorImpl(error, e);
				return false;
			};

			if (scope == EnvScope::Process)
			{
				if (!tryScope(EnvScope::Process))
					return false;
			} else if (scope == EnvScope::User)
			{
				if (!tryScope(EnvScope::User))
					return false;
				if (!found && !tryScope(EnvScope::Machine))
					return false;
				if (!found && !tryScope(EnvScope::Process))
					return false;
			} else
			{
				if (!tryScope(EnvScope::Machine))
					return false;
				if (!found && !tryScope(EnvScope::Process))
					return false;
			}

			SetSuccessImpl(error);
			return true;
		}

		bool ExpandRecursiveImpl(
			EnvScope scope, std::wstring_view input,
			std::wstring& output, std::vector<std::wstring>& stack,
			int depth, DWORD* error)
		{
			if (depth > kMaxExpandDepth)
			{
				SetErrorImpl(error, ERROR_CIRCULAR_DEPENDENCY);
				return false;
			}

			output.clear();
			output.reserve(input.size());

			size_t pos = 0;
			while (pos < input.size())
			{
				const size_t begin = input.find(L'%', pos);
				if (begin == std::wstring_view::npos)
				{
					output.append(input.substr(pos));
					break;
				}

				output.append(input.substr(pos, begin - pos));
				const size_t end = input.find(L'%', begin + 1);

				if (end == std::wstring_view::npos)
				{
					output.append(input.substr(begin));
					break;
				}

				const auto name = input.substr(begin + 1, end - begin - 1);
				if (name.empty())
				{
					output.append(L"%%");
					pos = end + 1;
					continue;
				}

				std::wstring raw;
				bool found = false;

				if (!ResolveVariableImpl(scope, name, raw, found, error))
					return false;

				if (!found)
				{
					output.append(input.substr(begin, end - begin + 1));
					pos = end + 1;
					continue;
				}

				for (const auto& active : stack)
				{
					if (EqualIImpl(active, name))
					{
						SetErrorImpl(error, ERROR_CIRCULAR_DEPENDENCY);
						return false;
					}
				}

				stack.emplace_back(name);

				std::wstring expanded;
				if (!ExpandRecursiveImpl(
					scope, raw, expanded, stack, depth + 1, error))
				{
					stack.pop_back();
					return false;
				}

				stack.pop_back();
				output.append(expanded);
				pos = end + 1;
			}

			SetSuccessImpl(error);
			return true;
		}

		bool TryExpandScopeWImpl(
			EnvScope scope, std::wstring_view value,
			std::wstring& result, DWORD* error)
		{
			if (ContainsNullImpl(value))
			{
				SetErrorImpl(error, ERROR_INVALID_PARAMETER);
				return false;
			}

			std::vector<std::wstring> stack;
			return ExpandRecursiveImpl(scope, value, result, stack, 0, error);
		}

		bool TryListProcessWImpl(EnvListW& result, bool includeHidden, DWORD* error)
		{
			result.clear();

			LPWCH block = ::GetEnvironmentStringsW();
			if (!block)
			{
				SetErrorImpl(error, ::GetLastError());
				return false;
			}

			try
			{
				for (const wchar_t* current = block; *current;
					 current += std::wcslen(current) + 1)
				{
					const std::wstring_view item(current);
					size_t separator = std::wstring_view::npos;

					if (item.front() == L'=')
					{
						if (!includeHidden)
							continue;
						separator = item.find(L'=', 1);
					} else
					{
						separator = item.find(L'=');
					}

					if (separator == std::wstring_view::npos)
						continue;

					result.push_back({
						std::wstring(item.substr(0, separator)),
						std::wstring(item.substr(separator + 1))
									 });
				}
			}
			catch (...)
			{
				::FreeEnvironmentStringsW(block);
				throw;
			}

			::FreeEnvironmentStringsW(block);
			SetSuccessImpl(error);
			return true;
		}

		bool TryListRegistryWImpl(
			EnvScope scope, EnvListW& result,
			bool includeHidden, DWORD* error)
		{
			(void)includeHidden;
			result.clear();

			HKEY key = nullptr;
			LSTATUS status = ::RegOpenKeyExW(
				RegistryRootImpl(scope), RegistryPathImpl(scope),
				0, KEY_QUERY_VALUE, &key);

			if (status != ERROR_SUCCESS)
			{
				SetErrorImpl(error, NormalizeRegistryErrorImpl(status));
				return false;
			}

			DWORD maxName = 0;
			DWORD maxData = 0;

			status = ::RegQueryInfoKeyW(
				key,
				nullptr, nullptr, nullptr,
				nullptr, nullptr, nullptr,
				nullptr, &maxName, &maxData,
				nullptr, nullptr);

			if (status != ERROR_SUCCESS)
			{
				::RegCloseKey(key);
				SetErrorImpl(error, static_cast<DWORD>(status));
				return false;
			}

			std::vector<wchar_t> name(static_cast<size_t>(maxName) + 2);
			std::vector<wchar_t> data(static_cast<size_t>(maxData / sizeof(wchar_t)) + 2);

			for (DWORD index = 0;;)
			{
				DWORD nameLen = static_cast<DWORD>(name.size());
				DWORD dataBytes = static_cast<DWORD>(data.size() * sizeof(wchar_t));
				DWORD type = 0;

				status = ::RegEnumValueW(
					key, index, name.data(), &nameLen,
					nullptr, &type,
					reinterpret_cast<LPBYTE>(data.data()), &dataBytes);

				if (status == ERROR_NO_MORE_ITEMS)
					break;

				if (status == ERROR_MORE_DATA)
				{
					DWORD newMaxName = 0;
					DWORD newMaxData = 0;

					const LSTATUS infoStatus = ::RegQueryInfoKeyW(
						key,
						nullptr, nullptr, nullptr,
						nullptr, nullptr, nullptr,
						nullptr, &newMaxName, &newMaxData,
						nullptr, nullptr);

					if (infoStatus != ERROR_SUCCESS)
					{
						::RegCloseKey(key);
						SetErrorImpl(error, static_cast<DWORD>(infoStatus));
						return false;
					}

					name.resize(static_cast<size_t>(newMaxName) + 2);
					data.resize(static_cast<size_t>(newMaxData / sizeof(wchar_t)) + 2);
					continue;
				}

				if (status != ERROR_SUCCESS)
				{
					::RegCloseKey(key);
					SetErrorImpl(error, static_cast<DWORD>(status));
					return false;
				}

				++index;

				if (type != REG_SZ && type != REG_EXPAND_SZ)
					continue;

				size_t chars = dataBytes / sizeof(wchar_t);
				while (chars && data[chars - 1] == L'\0')
					--chars;

				result.push_back({
					std::wstring(name.data(), nameLen),
					std::wstring(data.data(), chars)
								 });
			}

			::RegCloseKey(key);
			SetSuccessImpl(error);
			return true;
		}

		bool TryListScopeWImpl(
			EnvScope scope, EnvListW& result,
			bool includeHidden, DWORD* error)
		{
			if (scope == EnvScope::Process)
				return TryListProcessWImpl(result, includeHidden, error);
			return TryListRegistryWImpl(scope, result, includeHidden, error);
		}

		std::wstring TrimPathItemImpl(std::wstring_view value)
		{
			size_t first = 0;
			size_t last = value.size();

			auto isSpace = [](wchar_t c) noexcept
			{
				return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n';
			};

			while (first < last && isSpace(value[first]))
				++first;
			while (last > first && isSpace(value[last - 1]))
				--last;

			std::wstring result(value.substr(first, last - first));

			if (result.size() >= 2 &&
				result.front() == L'"' &&
				result.back() == L'"')
			{
				result.erase(result.begin());
				result.pop_back();
			}

			return result;
		}

		PathListW SplitPathImpl(std::wstring_view value)
		{
			PathListW result;
			size_t begin = 0;
			bool quoted = false;

			for (size_t i = 0; i <= value.size(); ++i)
			{
				if (i < value.size() && value[i] == L'"')
					quoted = !quoted;

				if (i != value.size() && (value[i] != L';' || quoted))
					continue;

				auto item = TrimPathItemImpl(value.substr(begin, i - begin));
				if (!item.empty())
					result.emplace_back(std::move(item));

				begin = i + 1;
			}

			return result;
		}

		bool IsDriveRootImpl(std::wstring_view value) noexcept
		{
			return value.size() == 3 &&
				((value[0] >= L'A' && value[0] <= L'Z') ||
				 (value[0] >= L'a' && value[0] <= L'z')) &&
				value[1] == L':' &&
				(value[2] == L'\\' || value[2] == L'/');
		}

		bool IsExtendedDriveRootImpl(std::wstring_view value) noexcept
		{
			if (value.size() != 7)
				return false;

			if (!(value[0] == L'\\' && value[1] == L'\\' &&
				(value[2] == L'?' || value[2] == L'.') &&
				value[3] == L'\\'))
			{
				return false;
			}

			return ((value[4] >= L'A' && value[4] <= L'Z') ||
					(value[4] >= L'a' && value[4] <= L'z')) &&
				value[5] == L':' &&
				(value[6] == L'\\' || value[6] == L'/');
		}

		std::wstring NormalizePathForCompareImpl(std::wstring_view value)
		{
			std::wstring result = TrimPathItemImpl(value);

			for (auto& c : result)
			{
				if (c == L'/')
					c = L'\\';
			}

			while (result.size() > 1 &&
				   result.back() == L'\\' &&
				   !IsDriveRootImpl(result) &&
				   !IsExtendedDriveRootImpl(result))
			{
				result.pop_back();
			}

			return result;
		}

		bool PathEqualImpl(std::wstring_view a, std::wstring_view b)
		{
			const auto na = NormalizePathForCompareImpl(a);
			const auto nb = NormalizePathForCompareImpl(b);
			return EqualIImpl(na, nb);
		}

		bool ContainsPathImpl(const PathListW& list, std::wstring_view value)
		{
			return std::any_of(
				list.begin(), list.end(),
				[&](const std::wstring& existing)
			{
				return PathEqualImpl(existing, value);
			});
		}

		bool IsDirectoryImpl(std::wstring_view path)
		{
			if (path.empty() || ContainsNullImpl(path))
				return false;

			const std::wstring p(path);
			const DWORD attr = ::GetFileAttributesW(p.c_str());

			return attr != INVALID_FILE_ATTRIBUTES &&
				(attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
		}

		bool TryBuildPathWImpl(
			EnvScope scope, PathMode mode,
			PathListW& result, DWORD* error)
		{
			result.clear();

			std::wstring rawPath;
			if (!TryGetScopeWImpl(scope, L"Path", rawPath, error))
				return false;

			const auto rawItems = SplitPathImpl(rawPath);

			const bool outputExpanded =
				mode == PathMode::Expanded ||
				mode == PathMode::ExpandedUnique ||
				mode == PathMode::ExpandedExisting;

			const bool unique =
				mode == PathMode::Unique ||
				mode == PathMode::ExpandedUnique ||
				mode == PathMode::Existing ||
				mode == PathMode::ExpandedExisting;

			const bool existing =
				mode == PathMode::Existing ||
				mode == PathMode::ExpandedExisting;

			const bool compareExpanded =
				mode == PathMode::ExpandedUnique ||
				mode == PathMode::Existing ||
				mode == PathMode::ExpandedExisting;

			PathListW comparison;

			for (const auto& raw : rawItems)
			{
				const bool needExpanded = outputExpanded || existing || compareExpanded;
				std::wstring expanded = raw;

				if (needExpanded &&
					!TryExpandScopeWImpl(scope, raw, expanded, error))
				{
					return false;
				}

				if (outputExpanded && expanded.empty())
					continue;

				if (existing && !IsDirectoryImpl(expanded))
					continue;

				const std::wstring& compare = compareExpanded ? expanded : raw;

				if (unique && ContainsPathImpl(comparison, compare))
					continue;

				if (unique)
					comparison.emplace_back(compare);

				result.emplace_back(outputExpanded ? expanded : raw);
			}

			SetSuccessImpl(error);
			return true;
		}

		bool TryGetAImpl(
			EnvScope scope, std::string_view name,
			std::string& value, DWORD* error)
		{
			std::wstring wn;
			if (!TryAtoWImpl(name, wn, error))
				return false;

			std::wstring wv;
			if (!TryGetScopeWImpl(scope, wn, wv, error))
				return false;

			return TryWtoAImpl(wv, value, error);
		}

		bool SetAImpl(
			EnvScope scope, std::string_view name,
			std::string_view value, DWORD* error)
		{
			std::wstring wn;
			std::wstring wv;

			if (!TryAtoWImpl(name, wn, error) ||
				!TryAtoWImpl(value, wv, error))
			{
				return false;
			}

			return SetScopeWImpl(scope, wn, wv, error);
		}

		bool RemoveAImpl(EnvScope scope, std::string_view name, DWORD* error)
		{
			std::wstring wn;
			if (!TryAtoWImpl(name, wn, error))
				return false;

			return RemoveScopeWImpl(scope, wn, error);
		}

		bool TryExpandAImpl(
			EnvScope scope, std::string_view value,
			std::string& result, DWORD* error)
		{
			std::wstring wv;
			if (!TryAtoWImpl(value, wv, error))
				return false;

			std::wstring wr;
			if (!TryExpandScopeWImpl(scope, wv, wr, error))
				return false;

			return TryWtoAImpl(wr, result, error);
		}

		bool TryListAImpl(
			EnvScope scope, EnvListA& result,
			bool includeHidden, DWORD* error)
		{
			result.clear();

			EnvListW wide;
			if (!TryListScopeWImpl(scope, wide, includeHidden, error))
				return false;

			for (const auto& entry : wide)
			{
				EnvEntryA item;

				if (!TryWtoAImpl(entry.name, item.name, error) ||
					!TryWtoAImpl(entry.value, item.value, error))
				{
					result.clear();
					return false;
				}

				result.emplace_back(std::move(item));
			}

			SetSuccessImpl(error);
			return true;
		}

		bool TryPathAImpl(
			EnvScope scope, PathMode mode,
			PathListA& result, DWORD* error)
		{
			result.clear();

			PathListW wide;
			if (!TryBuildPathWImpl(scope, mode, wide, error))
				return false;

			for (const auto& path : wide)
			{
				std::string item;
				if (!TryWtoAImpl(path, item, error))
				{
					result.clear();
					return false;
				}

				result.emplace_back(std::move(item));
			}

			SetSuccessImpl(error);
			return true;
		}
	}

#define YTPP_ENV_IMPL(CLASS, SCOPE) \
	bool CLASS::TryGetW(std::wstring_view n, std::wstring& v, DWORD* e) { return TryGetScopeWImpl(SCOPE, n, v, e); } \
	bool CLASS::TryGetA(std::string_view n, std::string& v, DWORD* e) { return TryGetAImpl(SCOPE, n, v, e); } \
	std::optional<std::wstring> CLASS::GetW(std::wstring_view n) { std::wstring v; if (!TryGetW(n, v)) return std::nullopt; return v; } \
	std::optional<std::string> CLASS::GetA(std::string_view n) { std::string v; if (!TryGetA(n, v)) return std::nullopt; return v; } \
	std::wstring CLASS::GetOrW(std::wstring_view n, std::wstring_view f) { std::wstring v; return TryGetW(n, v) ? v : std::wstring(f); } \
	std::string CLASS::GetOrA(std::string_view n, std::string_view f) { std::string v; return TryGetA(n, v) ? v : std::string(f); } \
	bool CLASS::ExistsW(std::wstring_view n) { std::wstring v; return TryGetW(n, v); } \
	bool CLASS::ExistsA(std::string_view n) { std::string v; return TryGetA(n, v); } \
	bool CLASS::SetW(std::wstring_view n, std::wstring_view v, DWORD* e) { return SetScopeWImpl(SCOPE, n, v, e); } \
	bool CLASS::SetA(std::string_view n, std::string_view v, DWORD* e) { return SetAImpl(SCOPE, n, v, e); } \
	bool CLASS::RemoveW(std::wstring_view n, DWORD* e) { return RemoveScopeWImpl(SCOPE, n, e); } \
	bool CLASS::RemoveA(std::string_view n, DWORD* e) { return RemoveAImpl(SCOPE, n, e); } \
	bool CLASS::TryExpandW(std::wstring_view v, std::wstring& r, DWORD* e) { return TryExpandScopeWImpl(SCOPE, v, r, e); } \
	bool CLASS::TryExpandA(std::string_view v, std::string& r, DWORD* e) { return TryExpandAImpl(SCOPE, v, r, e); } \
	std::optional<std::wstring> CLASS::ExpandW(std::wstring_view v) { std::wstring r; if (!TryExpandW(v, r)) return std::nullopt; return r; } \
	std::optional<std::string> CLASS::ExpandA(std::string_view v) { std::string r; if (!TryExpandA(v, r)) return std::nullopt; return r; } \
	bool CLASS::TryListW(EnvListW& r, bool h, DWORD* e) { return TryListScopeWImpl(SCOPE, r, h, e); } \
	bool CLASS::TryListA(EnvListA& r, bool h, DWORD* e) { return TryListAImpl(SCOPE, r, h, e); } \
	EnvListW CLASS::ListW(bool h) { EnvListW r; TryListW(r, h); return r; } \
	EnvListA CLASS::ListA(bool h) { EnvListA r; TryListA(r, h); return r; } \
	bool CLASS::TryGetPathW(PathListW& r, DWORD* e) { return TryBuildPathWImpl(SCOPE, PathMode::Raw, r, e); } \
	bool CLASS::TryGetPathA(PathListA& r, DWORD* e) { return TryPathAImpl(SCOPE, PathMode::Raw, r, e); } \
	bool CLASS::TryGetExpandedPathW(PathListW& r, DWORD* e) { return TryBuildPathWImpl(SCOPE, PathMode::Expanded, r, e); } \
	bool CLASS::TryGetExpandedPathA(PathListA& r, DWORD* e) { return TryPathAImpl(SCOPE, PathMode::Expanded, r, e); } \
	bool CLASS::TryGetUniquePathW(PathListW& r, DWORD* e) { return TryBuildPathWImpl(SCOPE, PathMode::Unique, r, e); } \
	bool CLASS::TryGetUniquePathA(PathListA& r, DWORD* e) { return TryPathAImpl(SCOPE, PathMode::Unique, r, e); } \
	bool CLASS::TryGetExpandedUniquePathW(PathListW& r, DWORD* e) { return TryBuildPathWImpl(SCOPE, PathMode::ExpandedUnique, r, e); } \
	bool CLASS::TryGetExpandedUniquePathA(PathListA& r, DWORD* e) { return TryPathAImpl(SCOPE, PathMode::ExpandedUnique, r, e); } \
	bool CLASS::TryGetExistingPathW(PathListW& r, DWORD* e) { return TryBuildPathWImpl(SCOPE, PathMode::Existing, r, e); } \
	bool CLASS::TryGetExistingPathA(PathListA& r, DWORD* e) { return TryPathAImpl(SCOPE, PathMode::Existing, r, e); } \
	bool CLASS::TryGetExpandedExistingPathW(PathListW& r, DWORD* e) { return TryBuildPathWImpl(SCOPE, PathMode::ExpandedExisting, r, e); } \
	bool CLASS::TryGetExpandedExistingPathA(PathListA& r, DWORD* e) { return TryPathAImpl(SCOPE, PathMode::ExpandedExisting, r, e); } \
	PathListW CLASS::GetPathW() { PathListW r; TryGetPathW(r); return r; } \
	PathListA CLASS::GetPathA() { PathListA r; TryGetPathA(r); return r; } \
	PathListW CLASS::GetExpandedPathW() { PathListW r; TryGetExpandedPathW(r); return r; } \
	PathListA CLASS::GetExpandedPathA() { PathListA r; TryGetExpandedPathA(r); return r; } \
	PathListW CLASS::GetUniquePathW() { PathListW r; TryGetUniquePathW(r); return r; } \
	PathListA CLASS::GetUniquePathA() { PathListA r; TryGetUniquePathA(r); return r; } \
	PathListW CLASS::GetExpandedUniquePathW() { PathListW r; TryGetExpandedUniquePathW(r); return r; } \
	PathListA CLASS::GetExpandedUniquePathA() { PathListA r; TryGetExpandedUniquePathA(r); return r; } \
	PathListW CLASS::GetExistingPathW() { PathListW r; TryGetExistingPathW(r); return r; } \
	PathListA CLASS::GetExistingPathA() { PathListA r; TryGetExistingPathA(r); return r; } \
	PathListW CLASS::GetExpandedExistingPathW() { PathListW r; TryGetExpandedExistingPathW(r); return r; } \
	PathListA CLASS::GetExpandedExistingPathA() { PathListA r; TryGetExpandedExistingPathA(r); return r; }

	YTPP_ENV_IMPL(ProcessEnv, EnvScope::Process)
		YTPP_ENV_IMPL(UserEnv, EnvScope::User)
		YTPP_ENV_IMPL(MachineEnv, EnvScope::Machine)

#undef YTPP_ENV_IMPL
}
