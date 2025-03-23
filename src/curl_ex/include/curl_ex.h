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
			HttpCookiesWrapper(std::string responseHeaders);
			//~HttpCookiesWrapper();
			void ParseCookies(std::string responseHeaders);
			std::string GetCookieValue(const std::string& key);
			std::string GetAllCookies();
		private:
			std::map<std::string, std::string> cookies;
			static std::string Trim(const std::string& str);// 去除字符串前后的空格
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
			static HttpResponse Get(std::string url, std::string headersEx = "", std::string proxy = "", bool ssl = true, std::function<void(CURL*)> lpfnCurlOptions = nullptr);
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
			static HttpResponse Post(std::string url, std::string postData = "", std::string headersEx = "", std::string proxy = "", bool ssl = true, std::function<void(CURL*)> lpfnCurlOptions = nullptr);
		};


	}


}