#include "sys_core/encoding.h"

#include <windows.h>

namespace ytpp::sys_core::encoding {

namespace {

std::wstring MultiByteToWide(_In_ const std::string& text, _In_ UINT codePage) {
    if (text.empty()) {
        return {};
    }

    const int length = ::MultiByteToWideChar(codePage, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (length <= 0) {
        return {};
    }

    std::wstring result(static_cast<std::size_t>(length), L'\0');
    if (::MultiByteToWideChar(codePage, 0, text.data(), static_cast<int>(text.size()), result.data(), length) <= 0) {
        return {};
    }
    return result;
}

std::string WideToMultiByte(_In_ const std::wstring& text, _In_ UINT codePage) {
    if (text.empty()) {
        return {};
    }

    const int length =
        ::WideCharToMultiByte(codePage, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (length <= 0) {
        return {};
    }

    std::string result(static_cast<std::size_t>(length), '\0');
    if (::WideCharToMultiByte(codePage, 0, text.data(), static_cast<int>(text.size()), result.data(), length, nullptr,
                              nullptr) <= 0) {
        return {};
    }
    return result;
}

} // namespace

std::string AnsiToUtf8(_In_ const std::string& text) {
    return WideToMultiByte(MultiByteToWide(text, CP_ACP), CP_UTF8);
}

std::string Utf8ToAnsi(_In_ const std::string& text) {
    return WideToMultiByte(MultiByteToWide(text, CP_UTF8), CP_ACP);
}

std::wstring AnsiToWide(_In_ const std::string& text) {
    return MultiByteToWide(text, CP_ACP);
}

std::string WideToAnsi(_In_ const std::wstring& text) {
    return WideToMultiByte(text, CP_ACP);
}

std::wstring Utf8ToWide(_In_ const std::string& text) {
    return MultiByteToWide(text, CP_UTF8);
}

std::string WideToUtf8(_In_ const std::wstring& text) {
    return WideToMultiByte(text, CP_UTF8);
}

} // namespace ytpp::sys_core::encoding
