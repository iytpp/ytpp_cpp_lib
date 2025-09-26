#include "curl_ex/curl_ex.h"

#include <string>
#include <functional>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>
using namespace std;

#include <curl/curl.h>

#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "ws2_32.lib")


namespace ytpp {
	namespace curl_ex {
		std::string UrlEncode(const std::string& input) {
			CURL* curl = curl_easy_init();
			if (!curl) return "";

			char* output = curl_easy_escape(curl, input.c_str(), static_cast<int>(input.length()));
			std::string encoded;
			if (output) {
				encoded = output;
				curl_free(output);
			}
			curl_easy_cleanup(curl);
			return encoded;
		}
		std::string UrlDecode(const std::string& input) {
			CURL* curl = curl_easy_init();
			if (!curl) return "";

			int outlength;
			char* output = curl_easy_unescape(curl, input.c_str(), static_cast<int>(input.length()), &outlength);
			std::string decoded;
			if (output) {
				decoded.assign(output, outlength);
				curl_free(output);
			}
			curl_easy_cleanup(curl);
			return decoded;
		}


#pragma region UrlParams
		UrlParams::UrlParams() = default;

		UrlParams::UrlParams(const std::string& query) {
			parse(query);
		}

		void UrlParams::parse(const std::string& query) {
			params.clear();
			std::string key, value;
			std::istringstream iss(query);
			std::string pair;

			while (std::getline(iss, pair, '&')) {
				auto pos = pair.find('=');
				if (pos != std::string::npos) {
					key = urlDecode(pair.substr(0, pos));
					value = urlDecode(pair.substr(pos + 1));
				} else {
					key = urlDecode(pair);
					value = "";
				}
				params[key] = value;
			}
		}

		std::string UrlParams::toString() const {
			std::ostringstream oss;
			bool first = true;
			for (const auto& kv : params) {
				if (!first) {
					oss << "&";
				}
				oss << urlEncode(kv.first);
				if (!kv.second.empty()) {
					oss << "=" << urlEncode(kv.second);
				}
				first = false;
			}
			return oss.str();
		}

		std::string UrlParams::get(const std::string& key) const {
			auto it = params.find(key);
			return (it != params.end()) ? it->second : "";
		}

		void UrlParams::set(const std::string& key, const std::string& value) {
			params[key] = value;
		}

		void UrlParams::remove(const std::string& key) {
			params.erase(key);
		}

		bool UrlParams::has(const std::string& key) const {
			return params.find(key) != params.end();
		}

		std::string UrlParams::urlEncode(const std::string& value) {
			std::ostringstream escaped;
			escaped.fill('0');
			escaped << std::hex << std::uppercase;

			for (unsigned char c : value) {
				if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
					escaped << c;
				} else {
					escaped << '%' << std::setw(2) << int(c);
				}
			}

			return escaped.str();
		}

		std::string UrlParams::urlDecode(const std::string& value) {
			std::ostringstream unescaped;
			for (size_t i = 0; i < value.size();) {
				if (value[i] == '%' && i + 2 < value.size()) {
					int hexValue = 0;
					std::istringstream iss(value.substr(i + 1, 2));
					iss >> std::hex >> hexValue;
					unescaped << static_cast<char>(hexValue);
					i += 3;
				} else if (value[i] == '+') {
					unescaped << ' ';
					i++;
				} else {
					unescaped << value[i];
					i++;
				}
			}
			return unescaped.str();
		}
#pragma endregion UrlParams

#pragma region HttpHeadersWrapper
		// 去除前后空格
		std::string HttpHeadersWrapper::Trim(const std::string& str) {
			std::string result = str;
			result.erase(result.begin(), std::find_if(result.begin(), result.end(), [](unsigned char ch) {
				return !std::isspace(ch);
			}));
			result.erase(std::find_if(result.rbegin(), result.rend(), [](unsigned char ch) {
				return !std::isspace(ch);
			}).base(), result.end());
			return result;
		}

		HttpHeadersWrapper::HttpHeadersWrapper() {}

		HttpHeadersWrapper::HttpHeadersWrapper(_In_ std::string responseHeaders) {
			ParseHeaders(std::move(responseHeaders));
		}

		HttpHeadersWrapper::HttpHeadersWrapper(const std::map<std::string, std::string>& defaultHeaders, const std::string& responseHeaders) {
			for (const auto& [key, value] : defaultHeaders) {
				SetDefaultHeader(key, value);
			}
			ParseHeaders(responseHeaders);
		}

		void HttpHeadersWrapper::ParseHeaders(_In_ std::string responseHeaders) {
			m_headers.clear();
			std::istringstream stream(responseHeaders);
			std::string line;

			while (std::getline(stream, line)) {
				auto pos = line.find(':');
				if (pos != std::string::npos) {
					std::string key = Trim(line.substr(0, pos));
					std::string value = Trim(line.substr(pos + 1));
					if (!key.empty()) {
						m_headers[key] = value;
					}
				}
			}
		}

		std::string HttpHeadersWrapper::GetHeaderValue(const std::string& key) {
			auto it = m_headers.find(Trim(key));
			if (it != m_headers.end()) {
				return it->second;
			}
			return {};
		}

		std::string HttpHeadersWrapper::GetAllHeaders() {
			std::ostringstream out;
			for (const auto& [key, value] : m_headers) {
				out << key << ": " << value << "\n";
			}
			std::string outStr = out.str();
			//去掉最后一个换行符
			if (!outStr.empty()) {
				outStr.pop_back();
			}
			return outStr;
		}

		bool HttpHeadersWrapper::SetHeader(const std::string& key, const std::string& value) {
			std::string trimmedKey = Trim(key);
			if (trimmedKey.empty()) return false;
			m_headers[trimmedKey] = Trim(value);
			return true;
		}

		bool HttpHeadersWrapper::SetDefaultHeader(const std::string& key, const std::string& value) {
			std::string trimmedKey = Trim(key);
			if (trimmedKey.empty()) return false;
			if (!IsExist(trimmedKey)) {
				m_headers[trimmedKey] = Trim(value);
				return true;
			}
			return false;
		}

		bool HttpHeadersWrapper::AppendHeader(const std::string& key, const std::string& value, const std::string& delimiter) {
			std::string trimmedKey = Trim(key);
			std::string trimmedValue = Trim(value);
			if (trimmedKey.empty()) return false;

			auto it = m_headers.find(trimmedKey);
			if (it != m_headers.end()) {
				it->second += delimiter + trimmedValue;
			} else {
				m_headers[trimmedKey] = trimmedValue;
			}
			return true;
		}

		bool HttpHeadersWrapper::EraseHeader(const std::string& key) {
			return m_headers.erase(Trim(key)) > 0;
		}

		bool HttpHeadersWrapper::IsExist(const std::string& key) {
			return m_headers.find(Trim(key)) != m_headers.end();
		}

		std::vector<std::string> HttpHeadersWrapper::GetKeys() {
			std::vector<std::string> keys;
			for (const auto& [key, _] : m_headers) {
				keys.push_back(key);
			}
			return keys;
		}
#pragma endregion HttpHeadersWrapper

#pragma region HttpCookiesWrapper
		std::string HttpCookiesWrapper::Trim(const std::string& str) {
			auto begin = std::find_if_not(str.begin(), str.end(), ::isspace);
			auto end = std::find_if_not(str.rbegin(), str.rend(), ::isspace).base();
			return (begin < end) ? std::string(begin, end) : std::string();
		}

		std::string Cookie::ToSetCookieString() const {
			std::ostringstream out;
			out << name << "=" << value;
			if (path) out << "; Path=" << *path;
			if (domain) out << "; Domain=" << *domain;
			if (expires) out << "; Expires=" << *expires;
			if (maxAge) out << "; Max-Age=" << *maxAge;
			if (secure) out << "; Secure";
			if (httpOnly) out << "; HttpOnly";
			if (sameSite) out << "; SameSite=" << *sameSite;
			return out.str();
		}

		HttpCookiesWrapper::HttpCookiesWrapper() {}

		HttpCookiesWrapper::HttpCookiesWrapper(const std::vector<std::string>& setCookieHeaders) {
			ParseFromSetCookieHeaders(setCookieHeaders);
		}

		HttpCookiesWrapper::HttpCookiesWrapper(const std::string& cookieString) {
			ParseFromCookieString(cookieString);
		}

		HttpCookiesWrapper::HttpCookiesWrapper(const HttpCookiesWrapper& other) {
			m_cookies = other.m_cookies;
		}

		void HttpCookiesWrapper::Merge(const HttpCookiesWrapper& other, bool overwrite) {
			for (const auto& [key, value] : other.m_cookies) {
				if (overwrite || m_cookies.find(key) == m_cookies.end()) {
					m_cookies[key] = value;
				}
			}
		}

		HttpCookiesWrapper HttpCookiesWrapper::MergedWith(const HttpCookiesWrapper& other, bool overwrite) const {
			HttpCookiesWrapper result = *this; // 拷贝当前对象
			result.Merge(other, overwrite);    // 使用已有的 Merge 方法
			return result;
		}

		std::vector<std::string> HttpCookiesWrapper::ExtractSetCookieHeaders(const std::string& rawHeaders) {
			std::vector<std::string> result;

			size_t start = 0;
			while (start < rawHeaders.size()) {
				size_t end = rawHeaders.find_first_of("\r\n", start);
				if (end == std::string::npos)
					end = rawHeaders.size();

				std::string line = Trim(rawHeaders.substr(start, end - start));
				if (line.size() >= 11) {
					std::string prefix = line.substr(0, 11);
					std::transform(prefix.begin(), prefix.end(), prefix.begin(), ::tolower);
					if (prefix == "set-cookie:") {
						result.push_back(Trim(line.substr(11)));
					}
				}

				// 处理混合换行符 \r\n、\n 或 \r
				if (end < rawHeaders.size()) {
					if (rawHeaders[end] == '\r' && rawHeaders[end + 1] == '\n') start = end + 2;
					else start = end + 1;
				} else {
					break;
				}
			}

			return result;
		}

		void HttpCookiesWrapper::ParseFromSetCookieHeaders(const std::vector<std::string>& setCookieHeaders) {
			m_cookies.clear(); // 清空之前的cookie
			for (const auto& header : setCookieHeaders) {
				std::istringstream stream(header);
				std::string segment;
				Cookie cookie;

				bool first = true;
				while (std::getline(stream, segment, ';')) {
					auto eqPos = segment.find('=');
					std::string key = Trim(segment.substr(0, eqPos));
					std::string value = (eqPos != std::string::npos) ? Trim(segment.substr(eqPos + 1)) : "";

					if (first && !key.empty()) {
						cookie.name = key;
						cookie.value = value;
						first = false;
					} else {
						std::string lowerKey = key;
						std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
						if (lowerKey == "path") cookie.path = value;
						else if (lowerKey == "domain") cookie.domain = value;
						else if (lowerKey == "expires") cookie.expires = value;
						else if (lowerKey == "max-age") cookie.maxAge = std::stoi(value);
						else if (lowerKey == "secure") cookie.secure = true;
						else if (lowerKey == "httponly") cookie.httpOnly = true;
						else if (lowerKey == "samesite") cookie.sameSite = value;
					}
				}
				if (!cookie.name.empty())
					m_cookies[cookie.name] = cookie;
			}
		}

		void HttpCookiesWrapper::ParseFromCookieString(const std::string& cookieString) {
			m_cookies.clear(); // 清空之前的cookie
			std::istringstream stream(cookieString);
			std::string token;

			while (std::getline(stream, token, ';')) {
				auto eqPos = token.find('=');
				if (eqPos != std::string::npos) {
					std::string key = Trim(token.substr(0, eqPos));
					std::string val = Trim(token.substr(eqPos + 1));
					if (!key.empty()) {
						Cookie cookie { key, val };
						m_cookies[key] = cookie;
					}
				}
			}
		}

		void HttpCookiesWrapper::SetCookie(const Cookie& cookie) {
			m_cookies[cookie.name] = cookie;
		}

		void HttpCookiesWrapper::SetCookie(const std::string& name, const std::string& value) {
			Cookie cookie { Trim(name), Trim(value) };
			m_cookies[cookie.name] = cookie;
		}

		bool HttpCookiesWrapper::EraseCookie(const std::string& name) {
			return m_cookies.erase(Trim(name)) > 0;
		}

		bool HttpCookiesWrapper::IsExist(const std::string& name) const {
			return m_cookies.find(name) != m_cookies.end();
		}

		void HttpCookiesWrapper::RemoveEmptyCookies()
		{
			for (auto it = m_cookies.begin(); it != m_cookies.end();) {
				if (it->second.value.empty()) {
					m_cookies.erase(it++);
				} else {
					++it;
				}
			}
		}

		std::string HttpCookiesWrapper::GetCookieValue(const std::string& name) const {
			auto it = m_cookies.find(name);
			return (it != m_cookies.end()) ? it->second.value : "";
		}

		std::vector<std::string> HttpCookiesWrapper::GetAllKeys(bool ignoreNull) const {
			std::vector<std::string> keys;
			for (const auto& [k, _] : m_cookies) {
				if (ignoreNull) {
					if (_.value.empty()) break;
				}
				keys.push_back(k);
			}
			return keys;
		}

		std::string HttpCookiesWrapper::ToRequestCookieString(bool ignoreNull) const {
			std::ostringstream out;
			bool first = true;
			for (const auto& [k, cookie] : m_cookies) {
				if (ignoreNull) {
					if (cookie.value.empty()) break;
				}
				if (!first) out << "; ";
				out << k << "=" << cookie.value;
				first = false;
			}
			return out.str();
		}

		std::vector<std::string> HttpCookiesWrapper::ToSetCookieHeaders(bool ignoreNull) const {
			std::vector<std::string> result;
			for (const auto& [_, cookie] : m_cookies) {
				if (ignoreNull) {
					if (cookie.value.empty()) break;
				}
				result.push_back("Set-Cookie: " + cookie.ToSetCookieString());
			}
			return result;
		}

		std::vector<Cookie> HttpCookiesWrapper::GetAllCookies(bool ignoreNull) const {
			std::vector<Cookie> all;
			for (const auto& [_, cookie] : m_cookies) {
				if (ignoreNull) {
					if (cookie.value.empty()) break;
				}
				all.push_back(cookie);
			}
			return all;
		}
#pragma endregion HttpCookiesWrapper

#pragma region HttpRequest
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
			curl_easy_setopt(curl, CURLOPT_HEADERDATA, &ret.org_headers); // 获取响应头
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
			ret.curl_code = res;
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
				ret.cookies.ParseFromSetCookieHeaders(HttpCookiesWrapper::ExtractSetCookieHeaders(ret.org_headers));
				// 解析headers
				ret.headers.ParseHeaders(ret.org_headers);
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
			curl_easy_setopt(curl, CURLOPT_HEADERDATA, &ret.org_headers); // 获取响应头
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers); // 设置请求头
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, ssl ? 1L : 0L);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, ssl ? 2L : 0L);
			if (!proxy.empty()) {
				curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str()); // 设置代理
			}

			if (lpfnCurlOptions) {
				lpfnCurlOptions(curl); // 调用回调函数，设置其他选项
			}

			CURLcode res = curl_easy_perform(curl);
			ret.curl_code = res;
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
				ret.cookies.ParseFromSetCookieHeaders(HttpCookiesWrapper::ExtractSetCookieHeaders(ret.org_headers));
				// 解析headers
				ret.headers.ParseHeaders(ret.org_headers);
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
#pragma endregion HttpRequest

	}
}