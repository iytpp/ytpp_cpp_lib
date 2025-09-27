#pragma once

#include <string>
#include <functional>
#include <optional>
#include <map>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <iomanip>

#include <curl/curl.h>

namespace ytpp {
	namespace curl_ex {
		std::string UrlEncode(const std::string& input);
		std::string UrlDecode(const std::string& input);

#pragma region UrlParams
		class UrlParams {
		public:
			UrlParams();
			explicit UrlParams(const std::string& query);

			void parse(const std::string& query);
			std::string toString() const;

			std::string get(const std::string& key) const;
			void set(const std::string& key, const std::string& value);
			void add(const std::string& key, const std::string& value);
			void remove(const std::string& key);
			bool has(const std::string& key) const;

			size_t size() const;
			std::vector<std::string> getAllParams() const;
			std::vector<std::string> getAllParamNames() const;  // 新增
			void clear();

			// 迭代器支持
			auto begin() { return params.begin(); }
			auto end() { return params.end(); }
			auto begin() const { return params.begin(); }
			auto end() const { return params.end(); }

			// 重载[]运算符
			std::string& operator[](const std::string& key);

		private:
			std::unordered_map<std::string, std::vector<std::string>> params;

			static std::string urlEncode(const std::string& value);
			static std::string urlDecode(const std::string& value);
		};
#pragma endregion UrlParams

#pragma region HttpHeaderWrapper
		class HttpHeadersWrapper {
		public:
			HttpHeadersWrapper();
			// 从响应头中解析，并构造
			HttpHeadersWrapper(_In_ std::string responseHeaders);
			// 从响应头中解析，并设置默认头
			HttpHeadersWrapper(const std::map<std::string, std::string>& defaultHeaders, const std::string& responseHeaders);

			// 从响应头中解析
			void ParseHeaders(_In_ std::string responseHeaders);

			// 获取头的值
			std::string GetHeaderValue(const std::string& key);

			// 获取所有头的值，并整理成多行头的格式
			std::string GetAllHeaders();

			// 设置头，如已存在则覆盖
			bool SetHeader(const std::string& key, const std::string& value);

			// 设置默认头，不存在时才有效，已存在时不覆盖
			bool SetDefaultHeader(const std::string& key, const std::string& value);

			// 在值的后面附加数据，而不是直接覆盖
			bool AppendHeader(const std::string& key, const std::string& value, const std::string& delimiter = ", ");
			
			// 删除头
			bool EraseHeader(const std::string& key);

			// 判断头是否存在
			bool IsExist(const std::string& key);

			// 获取所有头的键
			std::vector<std::string> GetKeys();

		private:
			static std::string Trim(const std::string& str);

			struct CaseInsensitiveCompare {
				bool operator()(const std::string& a, const std::string& b) const {
					return std::lexicographical_compare(
						a.begin(), a.end(), b.begin(), b.end(),
						[](unsigned char c1, unsigned char c2) {
						return std::tolower(c1) < std::tolower(c2);
					});
				}
			};

			std::map<std::string, std::string, CaseInsensitiveCompare> m_headers;
		};
#pragma endregion HttpHeaderWrapper

#pragma region HttpCookiesWrapper
		struct Cookie {
			std::string name;
			std::string value;
			std::optional<std::string> path;
			std::optional<std::string> domain;
			std::optional<std::string> expires;
			std::optional<int> maxAge;
			bool secure = false;
			bool httpOnly = false;
			std::optional<std::string> sameSite;

			std::string ToSetCookieString() const;
		};

		class HttpCookiesWrapper {
		public:
			HttpCookiesWrapper();
			// 从Set-Cookie 多行头中解析
			HttpCookiesWrapper(const std::vector<std::string>& setCookieHeaders);
			// 从cookie字符串中解析，例如 name1=value1; name2=value2
			HttpCookiesWrapper(const std::string& cookieString);
			// 拷贝
			HttpCookiesWrapper(const HttpCookiesWrapper& other);

			// 合并到当前对象
			void Merge(const HttpCookiesWrapper& other, bool overwrite = true);
			// 返回合并后的新对象
			HttpCookiesWrapper MergedWith(const HttpCookiesWrapper& other, bool overwrite) const;

			// 从响应头的 Set-Cookie 多行头中解析，可用静态函数ExtractSetCookieHeaders将多行Set-Cookie从原始响应头提取出来
			void ParseFromSetCookieHeaders(const std::vector<std::string>& setCookieHeaders);
			// 从cookie字符串中解析，例如 name1=value1; name2=value2
			void ParseFromCookieString(const std::string& cookieString);

			// 从headers中提取 Set-Cookie 多行头，结果通常传入ParseFromSetCookieHeaders函数中
			static std::vector<std::string> ExtractSetCookieHeaders(const std::string& rawHeaders);

			// 设置 Cookie
			void SetCookie(const Cookie& cookie);
			// 设置 Cookie
			void SetCookie(const std::string& name, const std::string& value);

			// 删除
			bool EraseCookie(const std::string& name);
			// 是否存在
			bool IsExist(const std::string& name) const;
			// 删除value为空的Cookie
			void RemoveEmptyCookies();

			// 获取Cookie值
			std::string GetCookieValue(const std::string& name) const;

			// 导出所有名字
			std::vector<std::string> GetAllKeys(bool ignoreNull = false) const;

			// 转换为请求头格式
			std::string ToRequestCookieString(bool ignoreNull = false) const;

			// 转换为 Set-Cookie 多行头
			std::vector<std::string> ToSetCookieHeaders(bool ignoreNull = false) const;

			// 获取所有 Cookie 对象
			std::vector<Cookie> GetAllCookies(bool ignoreNull = false) const;

		private:
			std::map<std::string, Cookie> m_cookies;
			static std::string Trim(const std::string& str);
		};
#pragma endregion HttpCookiesWrapper

#pragma region HttpRequest

		/// <summary>
		/// HTTP响应数据结构体
		/// </summary>
		struct HttpResponse {
			bool success = true;
			std::string error = "";
			std::string content = "";
			int code = 0; // 响应状态码
			std::string org_headers = ""; // 原始多行响应头
			HttpHeadersWrapper headers;
			HttpCookiesWrapper cookies;
			int curl_code = 0; // curl结果码
		};

		class HttpRequest {
		public:
			/// <summary>
			/// 发送GET请求
			/// </summary>
			/// <param name="url">要请求的地址</param>
			/// <param name="headersEx">每个头占一行，单个头格式例如 Content-Type: application/json</param>
			/// <param name="proxy">代理地址</param>
			/// <param name="ssl">是否开启ssl验证。默认为true</param>
			/// <param name="lpfnCurlOptions">额外设置函数回调，参数一为CURL*指针</param>
			/// <returns>HttpResponse</returns>
			static HttpResponse Get(
				_In_ std::string url,
				_In_ std::string headersEx = "",
				_In_ std::string proxy = "",
				_In_ bool ssl = true,
				_In_ std::function<void(CURL*)> lpfnCurlOptions = nullptr);

			/// <summary>
			/// 发送POST请求
			/// </summary>
			/// <param name="url">要请求的地址</param>
			/// <param name="postData">POST发送的数据</param>
			/// <param name="headersEx">每个头占一行，单个头格式例如 Content-Type: application/json</param>
			/// <param name="proxy">代理地址</param>
			/// <param name="ssl">是否开启ssl验证，默认为true</param>
			/// <param name="lpfnCurlOptions">额外设置函数回调，参数一为CURL*指针</param>
			/// <returns></returns>
			static HttpResponse Post(
				_In_ std::string url,
				_In_ std::string postData = "",
				_In_ std::string headersEx = "",
				_In_ std::string proxy = "",
				_In_ bool ssl = true,
				_In_ std::function<void(CURL*)> lpfnCurlOptions = nullptr);
		};
#pragma endregion HttpRequest

	}
}