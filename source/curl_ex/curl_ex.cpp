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
				if (pair.empty()) continue;

				auto pos = pair.find('=');
				if (pos != std::string::npos) {
					key = urlDecode(pair.substr(0, pos));
					value = urlDecode(pair.substr(pos + 1));
				} else {
					key = urlDecode(pair);
					value = "";
				}
				params[key].push_back(value);
			}
		}

		std::string UrlParams::toString() const {
			std::ostringstream oss;
			bool first = true;
			for (const auto& kv : params) {
				for (const auto& v : kv.second) {
					if (!first) {
						oss << "&";
					}
					oss << urlEncode(kv.first);
					if (!v.empty()) {
						oss << "=" << urlEncode(v);
					}
					first = false;
				}
			}
			return oss.str();
		}

		std::string UrlParams::get(const std::string& key) const {
			auto it = params.find(key);
			if (it != params.end() && !it->second.empty()) {
				return it->second.front();
			}
			return "";
		}

		void UrlParams::set(const std::string& key, const std::string& value) {
			params[key] = { value };
		}

		void UrlParams::add(const std::string& key, const std::string& value) {
			params[key].push_back(value);
		}

		void UrlParams::remove(const std::string& key) {
			params.erase(key);
		}

		bool UrlParams::has(const std::string& key) const {
			return params.find(key) != params.end();
		}

		size_t UrlParams::size() const {
			size_t total = 0;
			for (const auto& kv : params) {
				total += kv.second.size();
			}
			return total;
		}

		std::vector<std::string> UrlParams::getAllParams() const {
			std::vector<std::string> result;
			for (const auto& kv : params) {
				for (const auto& v : kv.second) {
					if (v.empty()) {
						result.push_back(kv.first);
					} else {
						result.push_back(kv.first + "=" + v);
					}
				}
			}
			return result;
		}

		std::vector<std::string> UrlParams::getAllParamNames() const {
			std::vector<std::string> names;
			names.reserve(params.size());
			for (const auto& kv : params) {
				names.push_back(kv.first);
			}
			return names;
		}

		void UrlParams::clear() {
			params.clear();
		}

		std::string& UrlParams::operator[](const std::string& key) {
			auto& vec = params[key];
			if (vec.empty()) {
				vec.push_back(""); // 如果不存在则新建
			}
			return vec.front();
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
					if (!(iss >> std::hex >> hexValue)) {
						throw std::runtime_error("Invalid URL encoding");
					}
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

		// 固定参数常量
		const long CONNECT_TIMEOUT = 6L; // 连接超时时间
		const long TIMEOUT = 25L; // 超时时间
		const long LOW_SPEED_TIME = 4L; // 4 秒内速度小于 1 Byte/s 则认为超时
		const long LOW_SPEED_LIMIT = 1L;

		/* 发送GET请求 */
		HttpResponse HttpRequest::Get(
			_In_ std::string url,
			_In_ std::string headersEx /*= ""*/,// 技术QQ：1282543064
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
			headers = curl_slist_append(headers, "Accept: ");
			headers = curl_slist_append(headers, "Expect:");
			headers = curl_slist_append(headers, headersEx.c_str());


			// ---- 基本配置 ----
			curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ret.content); // 获取响应内容
			curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
			curl_easy_setopt(curl, CURLOPT_HEADERDATA, &ret.org_headers); // 获取响应头
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers); // 设置请求头
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);     // 自动跳转
			curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);         // 最多跳转 10 次
			curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1L);        // 自动更新 Referer
			curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");    // 自动支持 gzip/deflate/br
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, TIMEOUT);                   // 整体超时
			curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, CONNECT_TIMEOUT);    // 连接超时
			curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, LOW_SPEED_TIME);     // LOW_SPEED_TIME秒内速度小于 LOW_SPEED_LIMIT B/s 则认为超时
			curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, LOW_SPEED_LIMIT);

			// ---- SSL 兼容性 ----
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, ssl ? 1L : 0L);     // 不验证证书（可改为 1L）
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, ssl ? 1L : 0L);
			curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_DEFAULT);  // 自动选择 TLS 版本
			curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);              // 全部启用 SSL 支持

			// ---- HTTP 兼容性 ----
			curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1); // 优先 HTTP/1.1
			//curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (libcurl universal client)");
			curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, ""); // 自动解压支持
			curl_easy_setopt(curl, CURLOPT_HTTP_CONTENT_DECODING, 1L);

			// ---- 自动 Cookie 处理 ----
			curl_easy_setopt(curl, CURLOPT_COOKIEFILE, ""); // 内存 cookie
			//curl_easy_setopt(curl, CURLOPT_COOKIEJAR, "cookies.txt");

			// ---- 代理配置 ----
			if (!proxy.empty())
			{
				curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());
				// 自动判断代理协议类型（推荐 AUTO）
				//curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);
				// 如果代理字符串带协议前缀，libcurl 会自动识别（如 socks5://）
				// 可选支持：
				// CURLPROXY_HTTP, CURLPROXY_HTTPS, CURLPROXY_SOCKS4, CURLPROXY_SOCKS5, CURLPROXY_SOCKS5_HOSTNAME
			}

			// ---- IPv6/IPv4 自动切换 ----
			curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_WHATEVER);

			// ---- 自动重试配置 ----
			curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L);
			curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_ALL);
			curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_ALL);  // 支持所有协议（HTTP/HTTPS/FTP/FILE等）

			//curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_slist_append(NULL, "Expect:"));

			//curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
			//curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
			//curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ret.content); // 获取响应内容
			//curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
			//curl_easy_setopt(curl, CURLOPT_HEADERDATA, &ret.org_headers); // 获取响应头
			//curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers); // 设置请求头
			//curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, ssl ? 1L : 0L); // 验证服务器 SSL 证书
			//curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, ssl ? 2L : 0L); // 验证服务器主机名
			//curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // 支持重定向// Apple协议算法定制QQ：1282543064
			//curl_easy_setopt(curl, CURLOPT_COOKIEFILE, ""); // 启用Cookie机制，在自动重定向的时候，cookie会自动保存到文件中

			//// 超时
			//curl_easy_setopt(curl, CURLOPT_TIMECONDITION, 30L);
			//curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

			//if (!proxy.empty()) {
			//	curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str()); // 设置代理
			//}


			// 技术QQ：1282543064
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

		/* 发送POST请求 */// 技术QQ：1282543064
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
			// Apple协议算法定制QQ：1282543064
			struct curl_slist* headers = NULL;
			headers = curl_slist_append(headers, "Content-Type: ");
			headers = curl_slist_append(headers, "Accept: ");
			headers = curl_slist_append(headers, "Expect:");
			headers = curl_slist_append(headers, headersEx.c_str());

			curl_version_info_data* data = curl_version_info(CURLVERSION_NOW);
			//std::cout << "SSL backend: " << (data->ssl_version ? data->ssl_version : "none") << std::endl;

			// ---- 基本配置 ----
			curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
			curl_easy_setopt(curl, CURLOPT_POST, 1L);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ret.content); // 获取响应内容
			curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
			curl_easy_setopt(curl, CURLOPT_HEADERDATA, &ret.org_headers); // 获取响应头
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers); // 设置请求头
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);     // 自动跳转
			curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);         // 最多跳转 10 次
			curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1L);        // 自动更新 Referer
			curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");    // 自动支持 gzip/deflate/br
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, TIMEOUT);                   // 整体超时
			curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, CONNECT_TIMEOUT);    // 连接超时
			curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, LOW_SPEED_TIME);     // LOW_SPEED_TIME秒内速度小于 LOW_SPEED_LIMIT B/s 则认为超时
			curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, LOW_SPEED_LIMIT);


			// ---- SSL 兼容性 ----
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, ssl ? 1L : 0L);     // 不验证证书（可改为 1L）
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, ssl ? 1L : 0L);
			curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_DEFAULT);  // 自动选择 TLS 版本
			curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);              // 全部启用 SSL 支持

			// ---- HTTP 兼容性 ----
			curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1); // 优先 HTTP/1.1
			//curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (libcurl universal client)");
			curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, ""); // 自动解压支持
			curl_easy_setopt(curl, CURLOPT_HTTP_CONTENT_DECODING, 1L);

			// ---- 自动 Cookie 处理 ----
			curl_easy_setopt(curl, CURLOPT_COOKIEFILE, ""); // 内存 cookie
			//curl_easy_setopt(curl, CURLOPT_COOKIEJAR, "cookies.txt");

			// ---- 代理配置 ----
			if (!proxy.empty())
			{
				curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());
				// 自动判断代理协议类型（推荐 AUTO）
				curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);
				// 如果代理字符串带协议前缀，libcurl 会自动识别（如 socks5://）
				// 可选支持：
				// CURLPROXY_HTTP, CURLPROXY_HTTPS, CURLPROXY_SOCKS4, CURLPROXY_SOCKS5, CURLPROXY_SOCKS5_HOSTNAME
			}

			// ---- IPv6/IPv4 自动切换 ----
			curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_WHATEVER);

			// ---- 自动重试配置 ----
			curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L);
			curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_ALL);
			curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_ALL);  // 支持所有协议（HTTP/HTTPS/FTP/FILE等）

			//curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_slist_append(NULL, "Expect:"));

			//curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); // 调试
			//
			//curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
			//curl_easy_setopt(curl, CURLOPT_POST, 1L);
			//curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
			//curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);// 技术QQ：1282543064
			//curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ret.content); // 获取响应内容
			//curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
			//curl_easy_setopt(curl, CURLOPT_HEADERDATA, &ret.org_headers); // 获取响应头
			//curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers); // 设置请求头
			//curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, ssl ? 1L : 0L);
			//curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, ssl ? 2L : 0L);
			//curl_easy_setopt(curl, CURLOPT_COOKIEFILE, ""); // 启用Cookie机制，在自动重定向的时候，cookie会自动保存到文件中

			//// 超时
			//curl_easy_setopt(curl, CURLOPT_TIMECONDITION, 30L);
			//curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

			//curl_easy_setopt(curl, CURLOPT_PROXYAUTH, CURLAUTH_ANY); // 设置代理认证方式
			////curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
			////curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NO_REVOKE);
			//curl_easy_setopt(curl, CURLOPT_PROXYAUTH, CURLAUTH_BASIC | CURLAUTH_NTLM | CURLAUTH_DIGEST);
			////curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_HTTPS); // 默认值，但最好显式指定

			//if (!proxy.empty())
			//{
			//	curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());
			//}

			// 技术QQ：1282543064


			if (lpfnCurlOptions) {
				lpfnCurlOptions(curl); // 调用回调函数，设置其他选项
			}// Apple协议算法定制QQ：1282543064

			CURLcode res = curl_easy_perform(curl);
			ret.curl_code = res;
			if (res != CURLE_OK) {
				ret.success = false;
				ret.error = "CURLcode:" + std::to_string(res) + ", " + std::string(curl_easy_strerror(res));
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
			return ret;// 技术QQ：1282543064
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