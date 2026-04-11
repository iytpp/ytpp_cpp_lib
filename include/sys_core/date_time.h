#pragma once

#include <string>

namespace ytpp::sys_core::date_time {
	struct DateTime
	{
		int year = 0;
		int month = 0;
		int day = 0;
		int hour = 0;
		int minute = 0;
		int second = 0;
		int millisecond = 0;
	};

	// 日期时间的部分
	enum class DateTimePart
	{
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


	// 这里采用“UTC偏移时区”模型
	// 优点：覆盖全球主要时区，结构简单
	// 注意：当前版本不处理夏令时 DST
	enum class TimeZone
	{
		UTC_Minus12 = -12,
		UTC_Minus11 = -11,
		UTC_Minus10 = -10,
		UTC_Minus9 = -9,
		UTC_Minus8 = -8,
		UTC_Minus7 = -7,
		UTC_Minus6 = -6,
		UTC_Minus5 = -5,
		UTC_Minus4 = -4,
		UTC_Minus3 = -3,
		UTC_Minus2 = -2,
		UTC_Minus1 = -1,
		UTC_0 = 0,
		UTC_Plus1 = 1,
		UTC_Plus2 = 2,
		UTC_Plus3 = 3,
		UTC_Plus4 = 4,
		UTC_Plus5 = 5,
		UTC_Plus6 = 6,
		UTC_Plus7 = 7,
		UTC_Plus8 = 8,   // 北京时间
		UTC_Plus9 = 9,
		UTC_Plus10 = 10,
		UTC_Plus11 = 11,
		UTC_Plus12 = 12,
		UTC_Plus13 = 13,
		UTC_Plus14 = 14
	};

	// 获取某年某月天数
	int GetDaysInMonth(int year, int month);

	// DateTime 转字符串
	std::string DateTimeToString(const DateTime& dt);

	// 获取星期几
	// 返回：1=星期一, 2=星期二, ..., 7=星期日
	int GetWeekDay(const DateTime& dt);

	// 对 DateTime 进行增减
	// value > 0 表示增加
	// value < 0 表示减少
	void DateTimeAdd(DateTime& dt, DateTimePart part, int value);

	// 获取两个 DateTime 的时间间隔
	// 返回值 = dt1 - dt2
	int GetDateTimeInterval( const DateTime& dt1, const DateTime& dt2, DateTimePart part);

	// 为了兼容“默认北京时间”的使用习惯
	static constexpr TimeZone TimeZone_Beijing = TimeZone::UTC_Plus8;

	// 联网获取 UTC 时间
	// 目标：https://www.baidu.com/
	// 方法：HEAD
	// 从 Date 响应头中读取 GMT/UTC 时间
	// expectedCertSha256 为空时：仅依赖系统默认 HTTPS 证书校验
	// expectedCertSha256 非空时：额外进行服务器证书 SHA-256 pinning
	DateTime GetDateTime_UTC(const std::wstring& expectedCertSha256 = L"");

	// UTC -> 指定时区
	DateTime ConvertUtcToTimeZone(
		const DateTime& utcDateTime,
		TimeZone tz = TimeZone_Beijing);

	// 对外主函数：联网取时间并转换到指定时区
	DateTime GetDateTime_NetWork(
		TimeZone tz = TimeZone_Beijing,
		const std::wstring& expectedCertSha256 = L"");

	// 获取百度当前服务器证书 SHA-256 指纹
	std::wstring GetServerCertSha256_Baidu();

	// 获取本地日期时间
	DateTime GetDateTime_Local();

}