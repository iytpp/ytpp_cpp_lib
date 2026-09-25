#pragma once

#include <cstdint>
#include <sal.h>
#include <string>

namespace ytpp::sys_core::date_time {

/// @brief 表示精确到毫秒的公历日期时间。
struct DateTime {
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    int millisecond = 0;
};

/// @brief 指定日期时间运算使用的单位。
enum class DateTimePart {
    Year,
    Quarter,
    Month,
    Week,
    Day,
    Hour,
    Minute,
    Second,
    Millisecond
};

/// @brief 表示不考虑夏令时的固定UTC小时偏移。
enum class TimeZone {
    UtcMinus12 = -12,
    UtcMinus11 = -11,
    UtcMinus10 = -10,
    UtcMinus9 = -9,
    UtcMinus8 = -8,
    UtcMinus7 = -7,
    UtcMinus6 = -6,
    UtcMinus5 = -5,
    UtcMinus4 = -4,
    UtcMinus3 = -3,
    UtcMinus2 = -2,
    UtcMinus1 = -1,
    Utc0 = 0,
    UtcPlus1 = 1,
    UtcPlus2 = 2,
    UtcPlus3 = 3,
    UtcPlus4 = 4,
    UtcPlus5 = 5,
    UtcPlus6 = 6,
    UtcPlus7 = 7,
    UtcPlus8 = 8,
    UtcPlus9 = 9,
    UtcPlus10 = 10,
    UtcPlus11 = 11,
    UtcPlus12 = 12,
    UtcPlus13 = 13,
    UtcPlus14 = 14
};

/// @brief 默认使用的北京时间固定偏移。
inline constexpr TimeZone kBeijingTimeZone = TimeZone::UtcPlus8;

/// @brief 获取指定年月包含的天数。
/// @param[in] year 年份，支持100至9999。
/// @param[in] month 月份，范围为1至12。
/// @return 指定月份包含的天数。
/// @param year 传递给 GetDaysInMonth 的 year 参数。
/// @param month 传递给 GetDaysInMonth 的 month 参数。
int GetDaysInMonth(_In_ int year, _In_ int month);

/// @brief 将日期时间格式化为中文年月日时分秒文本。
/// @param[in] dateTime 要格式化的日期时间。
/// @return 格式化后的UTF-8字符串。
/// @param dateTime 传递给 FormatDateTime 的 dateTime 参数。
std::string FormatDateTime(_In_ const DateTime& dateTime);

/// @brief 获取指定日期对应的星期序号。
/// @param[in] dateTime 要查询的日期。
/// @return 星期一至星期日分别返回1至7。
/// @param dateTime 传递给 GetWeekday 的 dateTime 参数。
int GetWeekday(_In_ const DateTime& dateTime);

/// @brief 按指定单位原地增加或减少日期时间。
/// @param[in,out] dateTime 要修改的日期时间。
/// @param[in] part 运算单位。
/// @param[in] value 增减量，负值表示减少。
/// @param dateTime 传递给 AddDateTimePart 的 dateTime 参数。
/// @param part 传递给 AddDateTimePart 的 part 参数。
/// @param value 要读取、写入或处理的值。
void AddDateTimePart(_Inout_ DateTime& dateTime, _In_ DateTimePart part, _In_ int value);

/// @brief 计算两个日期时间在指定单位下的差值。
/// @param[in] first 被减日期时间。
/// @param[in] second 减数日期时间。
/// @param[in] part 返回值使用的时间单位。
/// @return first减second的完整单位数量。
/// @param first 传递给 GetDateTimeDifference 的 first 参数。
/// @param second 传递给 GetDateTimeDifference 的 second 参数。
/// @param part 传递给 GetDateTimeDifference 的 part 参数。
std::int64_t GetDateTimeDifference(_In_ const DateTime& first, _In_ const DateTime& second, _In_ DateTimePart part);

/// @brief 联网读取服务器时间并返回UTC时间。
/// @param[in] expectedCertificateSha256 可选的服务器证书SHA-256指纹；为空时仅使用系统证书校验。
/// @return 从HTTP Date响应头解析得到的UTC时间。
/// @param expectedCertificateSha256 传递给 GetNetworkUtcDateTime 的 expectedCertificateSha256 参数。
DateTime GetNetworkUtcDateTime(_In_ const std::wstring& expectedCertificateSha256 = L"");

/// @brief 将UTC时间转换为固定偏移时区时间。
/// @param[in] utcDateTime UTC日期时间。
/// @param[in] timeZone 目标固定偏移时区。
/// @return 转换后的日期时间。
/// @param utcDateTime 传递给 ConvertUtcToTimeZone 的 utcDateTime 参数。
/// @param timeZone 传递给 ConvertUtcToTimeZone 的 timeZone 参数。
DateTime ConvertUtcToTimeZone(_In_ const DateTime& utcDateTime, _In_ TimeZone timeZone = kBeijingTimeZone);

/// @brief 联网读取服务器时间并转换到指定固定偏移时区。
/// @param[in] timeZone 目标时区，默认北京时间。
/// @param[in] expectedCertificateSha256 可选的服务器证书SHA-256指纹。
/// @return 转换后的网络时间。
/// @param timeZone 传递给 GetNetworkDateTime 的 timeZone 参数。
/// @param expectedCertificateSha256 传递给 GetNetworkDateTime 的 expectedCertificateSha256 参数。
DateTime GetNetworkDateTime(_In_ TimeZone timeZone = kBeijingTimeZone,
                            _In_ const std::wstring& expectedCertificateSha256 = L"");

/// @brief 获取当前百度HTTPS服务器证书的SHA-256指纹。
/// @return 大写十六进制证书指纹。
std::wstring GetBaiduServerCertificateSha256();

/// @brief 获取当前Windows本地日期时间。
/// @return 精确到毫秒的本地日期时间。
DateTime GetLocalDateTime();

} // namespace ytpp::sys_core::date_time
