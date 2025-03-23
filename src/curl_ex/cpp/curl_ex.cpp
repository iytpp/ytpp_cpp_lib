#include "curl_ex.h"

#include <string>
#include <functional>
#include <sstream>
#include <vector>
using namespace std;

#include <curl/curl.h>

#pragma comment(lib, "Crypt32.lib")


namespace ytpp {
	namespace curl_ex {

		static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output);
		static size_t HeaderCallback(void* contents, size_t size, size_t nmemb, std::string* header);

		/* 发送GET请求 */
		HttpResponse HttpRequest::Get(
			_In_ std::string url,
			_In_ std::string headersEx /*= ""*/,
			_In_ std::string proxy /*= ""*/,
			_In_ bool ssl /*= true*/,
			_In_ std::function<void(CURL*)> lpfnCurlOptions /*= nullptr*/)
		{
			HttpResponse ret;

			CURL* curl = curl_easy_init();
			if (!curl) {
				ret.success = false;
				ret.error = "CURL初始化失败";
				return ret; // 初始化失败
			}

			struct curl_slist* headers = NULL;
			headers = curl_slist_append(headers, "Content-Type: "); // 删除Content-Type，自己添加
			headers = curl_slist_append(headers, headersEx.c_str());

			curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ret.content); // 获取响应内容
			curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
			curl_easy_setopt(curl, CURLOPT_HEADERDATA, &ret.headers); // 获取响应头
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers); // 设置请求头
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, ssl ? 1L : 0L); // 验证服务器 SSL 证书
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, ssl ? 2L : 0L); // 验证服务器主机名
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // 支持重定向
			if (!proxy.empty()) {
				curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str()); // 设置代理
			}

			if (lpfnCurlOptions) {
				lpfnCurlOptions(curl); // 调用回调函数，设置其他选项
			}


			CURLcode res = curl_easy_perform(curl);
			if (res != CURLE_OK) {
				ret.success = false;
				ret.error = std::string(curl_easy_strerror(res));
			} else {
				ret.success = true;
				// 获取响应码
				long response_code = 0;
				curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
				ret.code = response_code;
				// 响应内容和响应头直接用回调函数获取了，这里不用处理
				// 解析Cookies
				ret.cookies.ParseCookies(ret.headers);
			}

			curl_slist_free_all(headers);
			curl_easy_cleanup(curl);
			return ret;
		}

		/* 发送POST请求 */
		HttpResponse HttpRequest::Post(
			_In_ std::string url,
			_In_ std::string postData /*= ""*/,
			_In_ std::string headersEx /*= ""*/,
			_In_ std::string proxy /*= ""*/,
			_In_ bool ssl /*= true*/,
			_In_ std::function<void(CURL*)> lpfnCurlOptions /*= nullptr*/)
		{
			HttpResponse ret;

			CURL* curl = curl_easy_init();
			if (!curl) {
				ret.success = false;
				ret.error = "CURL初始化失败";
				return ret; // 初始化失败
			}

			struct curl_slist* headers = NULL;
			headers = curl_slist_append(headers, "Content-Type: ");
			headers = curl_slist_append(headers, headersEx.c_str());

			curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
			curl_easy_setopt(curl, CURLOPT_POST, 1L);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ret.content); // 获取响应内容
			curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
			curl_easy_setopt(curl, CURLOPT_HEADERDATA, &ret.headers); // 获取响应头
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers); // 设置请求头
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, ssl ? 1L : 0L);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, ssl ? 2L : 0L);
			if (!proxy.empty()) {
				curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str()); // 设置代理
			}

			CURLcode res = curl_easy_perform(curl);
			if (res != CURLE_OK) {
				ret.success = false;
				ret.error = std::string(curl_easy_strerror(res));
			} else {
				ret.success = true;
				// 获取响应码
				long response_code = 0;
				curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
				ret.code = response_code;
				// 响应内容和响应头直接用回调函数获取了，这里不用处理
				// 解析Cookies
				ret.cookies.ParseCookies(ret.headers);
			}

			curl_slist_free_all(headers);
			curl_easy_cleanup(curl);
			return ret;
		}

		/* 回调函数：将传回的数据写入output */
		static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
			size_t totalSize = size * nmemb;
			output->append((char*)contents, totalSize);
			return totalSize;
		}
		/* 回调函数：用于写入接收到的HTTP响应头数据 */
		static size_t HeaderCallback(void* contents, size_t size, size_t nmemb, std::string* header) {
			size_t totalSize = size * nmemb;
			header->append((char*)contents, totalSize);
			return totalSize;
		}

		/* Cookies构造器 */
		HttpCookiesWrapper::HttpCookiesWrapper(_In_ std::string responseHeaders)
		{
			ParseCookies(responseHeaders);
		}

		/* 默认构造 */
		HttpCookiesWrapper::HttpCookiesWrapper()
		{
			//啥也不写
		}

		/* 解析Cookies */
		void HttpCookiesWrapper::ParseCookies(_In_ std::string responseHeaders)
		{
			this->cookies.clear(); // 清空之前的cookie

			std::istringstream stream(responseHeaders);
			std::string line;
			std::string lastValue = "";  // 存储最后一个非空的值

			while (std::getline(stream, line)) {
				// 只处理 Set-Cookie 行
				if (line.find("set-cookie:") == 0 || line.find("Set-Cookie:") == 0) {
					size_t start = line.find(":") + 1;
					std::string cookie = line.substr(start);

					// 取出 key=value
					cookie = cookie.substr(0, cookie.find(";"));
					cookie = Trim(cookie);  // 去除前后空格

					// 查找 key
					size_t equalPos = cookie.find("=");
					if (equalPos != std::string::npos) {
						std::string cookieKey = cookie.substr(0, equalPos);
						std::string cookieValue = cookie.substr(equalPos + 1);

						if (!cookieValue.empty()) {
							lastValue = cookieValue;  // 只有当 value 不为空时才更新
							this->cookies[cookieKey] = cookieValue;
						}
					}
				}
			}
		}

		/* 去除前后的空格 */
		std::string HttpCookiesWrapper::Trim(_In_ const std::string& str) {
			size_t first = str.find_first_not_of(" \t\r\n");
			size_t last = str.find_last_not_of(" \t\r\n");
			return (first == std::string::npos) ? "" : str.substr(first, last - first + 1);
		}

		/* 获取单个Cookie值 */
		std::string HttpCookiesWrapper::GetCookieValue(_In_ const std::string& key)
		{
			return this->cookies[key];
		}

		/* 将Cookies整理成key=value;key=value;的形式 */
		std::string HttpCookiesWrapper::GetAllCookies()
		{
			std::string result;
			for (const auto& cookie : this->cookies) {
				result += cookie.first + "=" + cookie.second + ";";
			}
			return result;
		}




	}
}