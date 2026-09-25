#include "sys_core/string_ex.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string_view>

namespace ytpp::sys_core::string_ex {

std::string ExtractBetween(_In_ const std::string& source, _In_ const std::string& left, _In_ const std::string& right,
                           _In_ std::size_t startPosition, _Inout_ std::size_t* endPosition) {
    if (endPosition != nullptr) {
        *endPosition = std::string::npos;
    }
    if (startPosition >= source.size()) {
        return {};
    }

    std::size_t contentStart = source.find(left, startPosition);
    if (contentStart == std::string::npos) {
        return {};
    }
    contentStart += left.length();

    const std::size_t contentEnd = source.find(right, contentStart);
    if (contentEnd == std::string::npos) {
        return {};
    }
    if (endPosition != nullptr) {
        *endPosition = contentEnd;
    }
    return source.substr(contentStart, contentEnd - contentStart);
}

std::vector<std::string> Split(_In_ const std::string& text, _In_ char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(text);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::vector<std::string> Split(_In_ const std::string& text, _In_ const std::string& delimiter) {
    if (delimiter.empty()) {
        return {text};
    }

    std::vector<std::string> tokens;
    std::size_t start = 0;
    while (true) {
        const std::size_t end = text.find(delimiter, start);
        if (end == std::string::npos) {
            tokens.push_back(text.substr(start));
            break;
        }
        tokens.push_back(text.substr(start, end - start));
        start = end + delimiter.length();
    }
    return tokens;
}

std::string ReplaceAll(_In_ const std::string& text, _In_ const std::string& oldSubstring,
                       _In_ const std::string& newSubstring) {
    if (oldSubstring.empty()) {
        return text;
    }

    std::string result = text;
    std::size_t position = 0;
    while ((position = result.find(oldSubstring, position)) != std::string::npos) {
        result.replace(position, oldSubstring.length(), newSubstring);
        position += newSubstring.length();
    }
    return result;
}

std::string Trim(_In_ const std::string& text) {
    constexpr std::string_view kWhitespace = " \t\r\n\f\v";
    const std::size_t first = text.find_first_not_of(kWhitespace);
    if (first == std::string::npos) {
        return {};
    }
    const std::size_t last = text.find_last_not_of(kWhitespace);
    return text.substr(first, last - first + 1);
}

std::string ToUpper(_In_ const std::string& text) {
    std::string result = text;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char value) { return static_cast<char>(std::toupper(value)); });
    return result;
}

std::string ToLower(_In_ const std::string& text) {
    std::string result = text;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return result;
}

} // namespace ytpp::sys_core::string_ex
