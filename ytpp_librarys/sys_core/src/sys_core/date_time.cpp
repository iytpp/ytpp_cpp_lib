#define _CRT_SECURE_NO_WARNINGS // 用于无视sscanf不安全的警告

#include "sys_core/date_time.h"

#include <windows.h>
#include <wincrypt.h>
#include <winhttp.h>

#include <cstdio>
#include <cwctype>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "crypt32.lib")

namespace ytpp::sys_core::date_time {

// ============================================================
// 基础工具
// ============================================================

static bool IsLeapYearChecked(_In_ int year) {
    if (year < 100 || year > 9999)
        throw std::runtime_error("Year out of supported range (100-9999).");

    return ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
}

static bool IsLeapYear(_In_ int year) {
    return ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
}

static int DaysInMonth(_In_ int year, _In_ int month) {
    switch (month) {
    case 1:
        return 31;
    case 2:
        return IsLeapYear(year) ? 29 : 28;
    case 3:
        return 31;
    case 4:
        return 30;
    case 5:
        return 31;
    case 6:
        return 30;
    case 7:
        return 31;
    case 8:
        return 31;
    case 9:
        return 30;
    case 10:
        return 31;
    case 11:
        return 30;
    case 12:
        return 31;
    default:
        throw std::runtime_error("Invalid month.");
    }
}

static void ValidateDateTime(_In_ const DateTime& dt) {
    if (dt.year < 100 || dt.year > 9999)
        throw std::runtime_error("DateTime.year out of supported range (100-9999).");

    if (dt.month < 1 || dt.month > 12)
        throw std::runtime_error("DateTime.month out of supported range (1-12).");

    const int dim = GetDaysInMonth(dt.year, dt.month);
    if (dt.day < 1 || dt.day > dim)
        throw std::runtime_error("DateTime.day is invalid.");

    if (dt.hour < 0 || dt.hour > 23)
        throw std::runtime_error("DateTime.hour out of range (0-23).");

    if (dt.minute < 0 || dt.minute > 59)
        throw std::runtime_error("DateTime.minute out of range (0-59).");

    if (dt.second < 0 || dt.second > 59)
        throw std::runtime_error("DateTime.second out of range (0-59).");

    if (dt.millisecond < 0 || dt.millisecond > 999)
        throw std::runtime_error("DateTime.millisecond out of range (0-999).");
}

static void NormalizeDateTime(_Inout_ DateTime& dt) {
    while (dt.millisecond >= 1000) {
        dt.millisecond -= 1000;
        dt.second++;
    }
    while (dt.millisecond < 0) {
        dt.millisecond += 1000;
        dt.second--;
    }

    while (dt.second >= 60) {
        dt.second -= 60;
        dt.minute++;
    }
    while (dt.second < 0) {
        dt.second += 60;
        dt.minute--;
    }

    while (dt.minute >= 60) {
        dt.minute -= 60;
        dt.hour++;
    }
    while (dt.minute < 0) {
        dt.minute += 60;
        dt.hour--;
    }

    while (dt.hour >= 24) {
        dt.hour -= 24;
        dt.day++;
    }
    while (dt.hour < 0) {
        dt.hour += 24;
        dt.day--;
    }

    while (dt.month > 12) {
        dt.month -= 12;
        dt.year++;
    }
    while (dt.month < 1) {
        dt.month += 12;
        dt.year--;
    }

    while (true) {
        int dim = GetDaysInMonth(dt.year, dt.month);

        if (dt.day >= 1 && dt.day <= dim)
            break;

        if (dt.day > dim) {
            dt.day -= dim;
            dt.month++;
            if (dt.month > 12) {
                dt.month = 1;
                dt.year++;
            }
        } else {
            dt.month--;
            if (dt.month < 1) {
                dt.month = 12;
                dt.year--;
            }
            dt.day += GetDaysInMonth(dt.year, dt.month);
        }
    }
}

static std::int64_t DaysFromCivil(_In_ int y, _In_ unsigned m, _In_ unsigned d) {
    y -= (m <= 2);
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return static_cast<std::int64_t>(era) * 146097 + static_cast<std::int64_t>(doe);
}

static std::int64_t ToEpochMilliseconds(_In_ const DateTime& dt) {
    ValidateDateTime(dt);

    const std::int64_t days = DaysFromCivil(dt.year, static_cast<unsigned>(dt.month), static_cast<unsigned>(dt.day));
    return (((days * 24LL + dt.hour) * 60LL + dt.minute) * 60LL + dt.second) * 1000LL + dt.millisecond;
}

static int CompareDateTime(_In_ const DateTime& a, _In_ const DateTime& b) {
    const std::int64_t ams = ToEpochMilliseconds(a);
    const std::int64_t bms = ToEpochMilliseconds(b);

    if (ams < bms)
        return -1;
    if (ams > bms)
        return 1;
    return 0;
}

static std::int64_t GetWholeMonthDiff(_In_ const DateTime& dt1, _In_ const DateTime& dt2) {
    std::int64_t months = static_cast<std::int64_t>(dt1.year - dt2.year) * 12 + (dt1.month - dt2.month);

    DateTime temp = dt2;
    AddDateTimePart(temp, DateTimePart::Month, static_cast<int>(months));

    if (months > 0) {
        if (CompareDateTime(temp, dt1) > 0)
            months--;
    } else if (months < 0) {
        if (CompareDateTime(temp, dt1) < 0)
            months++;
    }

    return months;
}

static void ClampDayToCurrentMonth(_Inout_ DateTime& dt) {
    int dim = DaysInMonth(dt.year, dt.month);
    if (dt.day > dim)
        dt.day = dim;
    if (dt.day < 1)
        dt.day = 1;
}

static int ParseMonthAbbreviation(_In_ const std::string& month) {
    static const std::map<std::string, int> kMonthMap = {{"Jan", 1}, {"Feb", 2},  {"Mar", 3},  {"Apr", 4},
                                                         {"May", 5}, {"Jun", 6},  {"Jul", 7},  {"Aug", 8},
                                                         {"Sep", 9}, {"Oct", 10}, {"Nov", 11}, {"Dec", 12}};

    auto it = kMonthMap.find(month);
    if (it == kMonthMap.end())
        throw std::runtime_error("Invalid month string.");

    return it->second;
}

// ============================================================
// 证书工具
// ============================================================

static std::wstring BytesToHexUpper(_In_reads_bytes_(size) const BYTE* data, _In_ DWORD size) {
    std::wstringstream wss;
    wss << std::uppercase << std::hex << std::setfill(L'0');

    for (DWORD i = 0; i < size; ++i)
        wss << std::setw(2) << static_cast<unsigned int>(data[i]);

    return wss.str();
}

static std::wstring NormalizeSha256Fingerprint(_In_ const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size());

    for (wchar_t ch : s) {
        if (ch == L':' || ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n')
            continue;

        out.push_back(static_cast<wchar_t>(std::towupper(ch)));
    }

    return out;
}

static PCCERT_CONTEXT QueryServerCertificateContext(_In_ HINTERNET requestHandle) {
    PCCERT_CONTEXT pCertContext = nullptr;
    DWORD dwSize = sizeof(pCertContext);

    if (!::WinHttpQueryOption(requestHandle, WINHTTP_OPTION_SERVER_CERT_CONTEXT, &pCertContext, &dwSize)) {
        throw std::runtime_error("WinHttpQueryOption(WINHTTP_OPTION_SERVER_CERT_CONTEXT) failed.");
    }

    if (!pCertContext)
        throw std::runtime_error("Server certificate context is null.");

    return pCertContext;
}

static std::wstring CalculateCertificateSha256Hex(_In_ PCCERT_CONTEXT certificateContext) {
    if (!certificateContext)
        throw std::runtime_error("Certificate context is null.");

    DWORD cbHash = 0;

    if (!::CryptHashCertificate2(BCRYPT_SHA256_ALGORITHM, 0, nullptr, certificateContext->pbCertEncoded,
                                 certificateContext->cbCertEncoded, nullptr, &cbHash)) {
        throw std::runtime_error("CryptHashCertificate2(size) failed.");
    }

    std::vector<BYTE> hash(cbHash);

    if (!::CryptHashCertificate2(BCRYPT_SHA256_ALGORITHM, 0, nullptr, certificateContext->pbCertEncoded,
                                 certificateContext->cbCertEncoded, hash.data(), &cbHash)) {
        throw std::runtime_error("CryptHashCertificate2(data) failed.");
    }

    return BytesToHexUpper(hash.data(), cbHash);
}

static void VerifyServerCertificateSha256Pin(_In_ HINTERNET requestHandle, _In_ const std::wstring& expectedSha256) {
    if (expectedSha256.empty())
        return;

    PCCERT_CONTEXT pCertContext = nullptr;

    try {
        pCertContext = QueryServerCertificateContext(requestHandle);

        const std::wstring actual = NormalizeSha256Fingerprint(CalculateCertificateSha256Hex(pCertContext));
        const std::wstring expected = NormalizeSha256Fingerprint(expectedSha256);

        ::CertFreeCertificateContext(pCertContext);
        pCertContext = nullptr;

        if (actual != expected)
            throw std::runtime_error("Server certificate SHA-256 pin mismatch.");
    } catch (...) {
        if (pCertContext)
            ::CertFreeCertificateContext(pCertContext);
        throw;
    }
}

// ============================================================
// HTTP Date 解析
// ============================================================

static std::string ReadHttpDateHeader(_In_ HINTERNET requestHandle) {
    wchar_t buffer[128] = {};
    DWORD size = sizeof(buffer);

    if (!::WinHttpQueryHeaders(requestHandle, WINHTTP_QUERY_DATE, WINHTTP_HEADER_NAME_BY_INDEX, buffer, &size,
                               WINHTTP_NO_HEADER_INDEX)) {
        throw std::runtime_error("WinHttpQueryHeaders(Date) failed.");
    }

    std::string dateHeader;
    for (const wchar_t character : std::wstring_view(buffer)) {
        if (character > 0x7F)
            throw std::runtime_error("WinHTTP returned a non-ASCII Date header.");
        dateHeader.push_back(static_cast<char>(character));
    }
    return dateHeader;
}

static DateTime ParseHttpDateToUtc(_In_ const std::string& dateHeader) {
    // 例如：
    // Sat, 11 Apr 2026 12:50:23 GMT
    std::istringstream iss(dateHeader);

    std::string weekDay;
    std::string dayStr;
    std::string monthStr;
    std::string yearStr;
    std::string timeStr;
    std::string gmtStr;

    iss >> weekDay >> dayStr >> monthStr >> yearStr >> timeStr >> gmtStr;

    if (dayStr.empty() || monthStr.empty() || yearStr.empty() || timeStr.empty())
        throw std::runtime_error("Invalid HTTP Date header format.");

    DateTime dt;
    dt.day = std::stoi(dayStr);
    dt.month = ParseMonthAbbreviation(monthStr);
    dt.year = std::stoi(yearStr);

    int hour = 0, minute = 0, second = 0;
    if (std::sscanf(timeStr.c_str(), "%d:%d:%d", &hour, &minute, &second) != 3)
        throw std::runtime_error("Invalid time part in Date header.");

    dt.hour = hour;
    dt.minute = minute;
    dt.second = second;
    dt.millisecond = 0;

    return dt;
}

// ================================== 以上是工具函数 ============================================================

// ============================================================
// 取百度服务器证书 SHA-256
// ============================================================

std::wstring GetBaiduServerCertificateSha256() {
    HINTERNET hSession = nullptr;
    HINTERNET hConnect = nullptr;
    HINTERNET hRequest = nullptr;
    PCCERT_CONTEXT pCertContext = nullptr;

    try {
        hSession = ::WinHttpOpen(L"NetTime/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME,
                                 WINHTTP_NO_PROXY_BYPASS, 0);

        if (!hSession)
            throw std::runtime_error("WinHttpOpen failed.");

        const int timeoutMs = 5000;
        ::WinHttpSetTimeouts(hSession, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

        hConnect = ::WinHttpConnect(hSession, L"www.baidu.com", INTERNET_DEFAULT_HTTPS_PORT, 0);

        if (!hConnect)
            throw std::runtime_error("WinHttpConnect failed.");

        hRequest = ::WinHttpOpenRequest(hConnect, L"HEAD", L"/", nullptr, WINHTTP_NO_REFERER,
                                        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);

        if (!hRequest)
            throw std::runtime_error("WinHttpOpenRequest failed.");

        if (!::WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
            throw std::runtime_error("WinHttpSendRequest failed.");
        }

        if (!::WinHttpReceiveResponse(hRequest, nullptr)) {
            throw std::runtime_error("WinHttpReceiveResponse failed.");
        }

        pCertContext = QueryServerCertificateContext(hRequest);
        std::wstring sha256 = CalculateCertificateSha256Hex(pCertContext);

        ::CertFreeCertificateContext(pCertContext);
        pCertContext = nullptr;

        if (hRequest)
            ::WinHttpCloseHandle(hRequest);
        if (hConnect)
            ::WinHttpCloseHandle(hConnect);
        if (hSession)
            ::WinHttpCloseHandle(hSession);

        return sha256;
    } catch (...) {
        if (pCertContext)
            ::CertFreeCertificateContext(pCertContext);
        if (hRequest)
            ::WinHttpCloseHandle(hRequest);
        if (hConnect)
            ::WinHttpCloseHandle(hConnect);
        if (hSession)
            ::WinHttpCloseHandle(hSession);
        throw;
    }
}

// ============================================================
// UTC -> 时区转换
// ============================================================

DateTime ConvertUtcToTimeZone(_In_ const DateTime& utcDateTime, _In_ TimeZone tz) {
    DateTime local = utcDateTime;
    local.hour += static_cast<int>(tz);
    NormalizeDateTime(local);
    return local;
}

// ============================================================
// 获取网络北京时间
// ============================================================

DateTime GetNetworkDateTime(_In_ TimeZone tz, _In_ const std::wstring& expectedCertSha256) {
    const DateTime utc = GetNetworkUtcDateTime(expectedCertSha256);
    return ConvertUtcToTimeZone(utc, tz);
}

// ============================================================
// 获取本地时间
// ============================================================
DateTime GetLocalDateTime() {
    DateTime dt{};

    // Windows 推荐方法（本地时间，带毫秒）
    SYSTEMTIME st{};
    ::GetLocalTime(&st);

    dt.year = st.wYear;
    dt.month = st.wMonth;
    dt.day = st.wDay;
    dt.hour = st.wHour;
    dt.minute = st.wMinute;
    dt.second = st.wSecond;
    dt.millisecond = st.wMilliseconds;

    return dt;
}

// ============================================================
// 联网取 UTC 时间
// ============================================================

DateTime GetNetworkUtcDateTime(_In_ const std::wstring& expectedCertSha256) {
    HINTERNET hSession = nullptr;
    HINTERNET hConnect = nullptr;
    HINTERNET hRequest = nullptr;

    try {
        hSession = ::WinHttpOpen(L"NetTime/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME,
                                 WINHTTP_NO_PROXY_BYPASS, 0);

        if (!hSession)
            throw std::runtime_error("WinHttpOpen failed.");

        const int timeoutMs = 30000;
        ::WinHttpSetTimeouts(hSession, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

        hConnect = ::WinHttpConnect(hSession, L"www.baidu.com", INTERNET_DEFAULT_HTTPS_PORT, 0);

        if (!hConnect)
            throw std::runtime_error("WinHttpConnect failed.");

        hRequest = ::WinHttpOpenRequest(hConnect, L"HEAD", L"/", nullptr, WINHTTP_NO_REFERER,
                                        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);

        if (!hRequest)
            throw std::runtime_error("WinHttpOpenRequest failed.");

        // 不设置任何忽略证书错误的安全标志
        // 使用 WinHTTP 默认 HTTPS 证书校验
        if (!::WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
            throw std::runtime_error("WinHttpSendRequest failed.");
        }

        if (!::WinHttpReceiveResponse(hRequest, nullptr)) {
            throw std::runtime_error("WinHttpReceiveResponse failed.");
        }

        // 在系统默认校验通过后，再做可选 pinning
        VerifyServerCertificateSha256Pin(hRequest, expectedCertSha256);

        const std::string dateHeader = ReadHttpDateHeader(hRequest);
        DateTime utc = ParseHttpDateToUtc(dateHeader);

        if (hRequest)
            ::WinHttpCloseHandle(hRequest);
        if (hConnect)
            ::WinHttpCloseHandle(hConnect);
        if (hSession)
            ::WinHttpCloseHandle(hSession);

        return utc;
    } catch (...) {
        if (hRequest)
            ::WinHttpCloseHandle(hRequest);
        if (hConnect)
            ::WinHttpCloseHandle(hConnect);
        if (hSession)
            ::WinHttpCloseHandle(hSession);
        throw;
    }
}

// ============================================================
// 获取日期间隔
// dt1 - dt2
// ============================================================

std::int64_t GetDateTimeDifference(_In_ const DateTime& dt1, _In_ const DateTime& dt2, _In_ DateTimePart part) {
    ValidateDateTime(dt1);
    ValidateDateTime(dt2);

    const std::int64_t diffMs = ToEpochMilliseconds(dt1) - ToEpochMilliseconds(dt2);

    switch (part) {
    case DateTimePart::Second:
        return diffMs / 1000LL;

    case DateTimePart::Minute:
        return diffMs / (60LL * 1000LL);

    case DateTimePart::Hour:
        return diffMs / (60LL * 60LL * 1000LL);

    case DateTimePart::Day:
        return diffMs / (24LL * 60LL * 60LL * 1000LL);

    case DateTimePart::Week:
        return diffMs / (7LL * 24LL * 60LL * 60LL * 1000LL);

    case DateTimePart::Month:
        return GetWholeMonthDiff(dt1, dt2);

    case DateTimePart::Quarter:
        return GetWholeMonthDiff(dt1, dt2) / 3;

    case DateTimePart::Year: {
        int years = dt1.year - dt2.year;

        DateTime temp = dt2;
        AddDateTimePart(temp, DateTimePart::Year, years);

        if (years > 0) {
            if (CompareDateTime(temp, dt1) > 0)
                years--;
        } else if (years < 0) {
            if (CompareDateTime(temp, dt1) < 0)
                years++;
        }

        return years;
    }

    case DateTimePart::Millisecond:
        return diffMs;

    default:
        throw std::runtime_error("This DateTimePart is not supported by GetDateTimeDifference.");
    }
}

// ============================================================
// AddDateTimePart
// ============================================================

void AddDateTimePart(_Inout_ DateTime& dt, _In_ DateTimePart part, _In_ int value) {
    if (value == 0)
        return;

    switch (part) {
    case DateTimePart::Year:
        dt.year += value;
        ClampDayToCurrentMonth(dt);
        break;

    case DateTimePart::Quarter:
        dt.month += value * 3;
        while (dt.month > 12) {
            dt.month -= 12;
            dt.year++;
        }
        while (dt.month < 1) {
            dt.month += 12;
            dt.year--;
        }
        ClampDayToCurrentMonth(dt);
        break;

    case DateTimePart::Month:
        dt.month += value;
        while (dt.month > 12) {
            dt.month -= 12;
            dt.year++;
        }
        while (dt.month < 1) {
            dt.month += 12;
            dt.year--;
        }
        ClampDayToCurrentMonth(dt);
        break;

    case DateTimePart::Week:
        dt.day += value * 7;
        NormalizeDateTime(dt);
        break;

    case DateTimePart::Day:
        dt.day += value;
        NormalizeDateTime(dt);
        break;

    case DateTimePart::Hour:
        dt.hour += value;
        NormalizeDateTime(dt);
        break;

    case DateTimePart::Minute:
        dt.minute += value;
        NormalizeDateTime(dt);
        break;

    case DateTimePart::Second:
        dt.second += value;
        NormalizeDateTime(dt);
        break;

    case DateTimePart::Millisecond:
        dt.millisecond += value;
        NormalizeDateTime(dt);
        break;

    default:
        throw std::runtime_error("Invalid DateTimePart.");
    }
}

// ============================================================
// FormatDateTime
// ============================================================

std::string FormatDateTime(_In_ const DateTime& dt) {
    ValidateDateTime(dt);

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(4) << dt.year << "年" << std::setw(2) << dt.month << "月" << std::setw(2)
        << dt.day << "日" << std::setw(2) << dt.hour << "时" << std::setw(2) << dt.minute << "分" << std::setw(2)
        << dt.second << "秒";

    return oss.str();
}

// ============================================================
// 星期几
// 返回：1=周一 ... 7=周日
// ============================================================

int GetWeekday(_In_ const DateTime& dt) {
    ValidateDateTime(dt);

    static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};

    int y = dt.year;
    int m = dt.month;
    int d = dt.day;

    if (m < 3)
        y -= 1;

    int w = (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;

    if (w == 0)
        return 7;

    return w;
}

// ============================================================
// 获取一个月有几天
// 返回：1=一天 ... 31=三十一天
// ============================================================

int GetDaysInMonth(_In_ int year, _In_ int month) {
    if (year < 100 || year > 9999)
        throw std::runtime_error("Year out of supported range (100-9999).");

    switch (month) {
    case 1:
        return 31;
    case 2:
        return IsLeapYearChecked(year) ? 29 : 28;
    case 3:
        return 31;
    case 4:
        return 30;
    case 5:
        return 31;
    case 6:
        return 30;
    case 7:
        return 31;
    case 8:
        return 31;
    case 9:
        return 30;
    case 10:
        return 31;
    case 11:
        return 30;
    case 12:
        return 31;
    default:
        throw std::runtime_error("Month out of supported range (1-12).");
    }
}

} // namespace ytpp::sys_core::date_time
