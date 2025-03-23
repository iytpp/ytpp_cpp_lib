#pragma once

#include <string>
#include <functional>
#include <map>

#include <curl/curl.h>

namespace ytpp {
	namespace curl_ex {

		class HttpCookiesWrapper {
		public:
			HttpCookiesWrapper();
			HttpCookiesWrapper(_In_ std::string responseHeaders);

			/// <summary>
			/// 解析响应头中的cookie
			/// </summary>
			/// <param name="responseHeaders"></param>
			void ParseCookies(_In_ std::string responseHeaders);
			
			/// <summary>
			/// 获取指定cookie的值
			/// </summary>
			/// <param name="key"></param>
			/// <returns></returns>
			std::string GetCookieValue(const std::string& key);
			
			/// <summary>
			/// 获取所有cookie的值，格式为 key1=value1; key2=value2
			/// </summary>
			/// <returns></returns>
			std::string GetAllCookies();
		private:
			std::map<std::string, std::string> cookies;
			static std::string Trim(_In_ const std::string& str);// 去除字符串前后的空格
		};

		/// <summary>
		/// HTTP响应数据结构体
		/// </summary>
		struct HttpResponse {
			bool success = true;
			std::string error = "";
			std::string content = "";
			int code = 0;
			std::string headers = "";
			HttpCookiesWrapper cookies;
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


	}


}