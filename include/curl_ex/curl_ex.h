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
			std::vector<std::string> getAllParamNames() const;  // ����
			void clear();

			// ������֧��
			auto begin() { return params.begin(); }
			auto end() { return params.end(); }
			auto begin() const { return params.begin(); }
			auto end() const { return params.end(); }

			// ����[]�����
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
			// ����Ӧͷ�н�����������
			HttpHeadersWrapper(_In_ std::string responseHeaders);
			// ����Ӧͷ�н�����������Ĭ��ͷ
			HttpHeadersWrapper(const std::map<std::string, std::string>& defaultHeaders, const std::string& responseHeaders);

			// ����Ӧͷ�н���
			void ParseHeaders(_In_ std::string responseHeaders);

			// ��ȡͷ��ֵ
			std::string GetHeaderValue(const std::string& key);

			// ��ȡ����ͷ��ֵ��������ɶ���ͷ�ĸ�ʽ
			std::string GetAllHeaders();

			// ����ͷ�����Ѵ����򸲸�
			bool SetHeader(const std::string& key, const std::string& value);

			// ����Ĭ��ͷ��������ʱ����Ч���Ѵ���ʱ������
			bool SetDefaultHeader(const std::string& key, const std::string& value);

			// ��ֵ�ĺ��渽�����ݣ�������ֱ�Ӹ���
			bool AppendHeader(const std::string& key, const std::string& value, const std::string& delimiter = ", ");
			
			// ɾ��ͷ
			bool EraseHeader(const std::string& key);

			// �ж�ͷ�Ƿ����
			bool IsExist(const std::string& key);

			// ��ȡ����ͷ�ļ�
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
			// ��Set-Cookie ����ͷ�н���
			HttpCookiesWrapper(const std::vector<std::string>& setCookieHeaders);
			// ��cookie�ַ����н��������� name1=value1; name2=value2
			HttpCookiesWrapper(const std::string& cookieString);
			// ����
			HttpCookiesWrapper(const HttpCookiesWrapper& other);

			// �ϲ�����ǰ����
			void Merge(const HttpCookiesWrapper& other, bool overwrite = true);
			// ���غϲ�����¶���
			HttpCookiesWrapper MergedWith(const HttpCookiesWrapper& other, bool overwrite) const;

			// ����Ӧͷ�� Set-Cookie ����ͷ�н��������þ�̬����ExtractSetCookieHeaders������Set-Cookie��ԭʼ��Ӧͷ��ȡ����
			void ParseFromSetCookieHeaders(const std::vector<std::string>& setCookieHeaders);
			// ��cookie�ַ����н��������� name1=value1; name2=value2
			void ParseFromCookieString(const std::string& cookieString);

			// ��headers����ȡ Set-Cookie ����ͷ�����ͨ������ParseFromSetCookieHeaders������
			static std::vector<std::string> ExtractSetCookieHeaders(const std::string& rawHeaders);

			// ���� Cookie
			void SetCookie(const Cookie& cookie);
			// ���� Cookie
			void SetCookie(const std::string& name, const std::string& value);

			// ɾ��
			bool EraseCookie(const std::string& name);
			// �Ƿ����
			bool IsExist(const std::string& name) const;
			// ɾ��valueΪ�յ�Cookie
			void RemoveEmptyCookies();

			// ��ȡCookieֵ
			std::string GetCookieValue(const std::string& name) const;

			// ������������
			std::vector<std::string> GetAllKeys(bool ignoreNull = false) const;

			// ת��Ϊ����ͷ��ʽ
			std::string ToRequestCookieString(bool ignoreNull = false) const;

			// ת��Ϊ Set-Cookie ����ͷ
			std::vector<std::string> ToSetCookieHeaders(bool ignoreNull = false) const;

			// ��ȡ���� Cookie ����
			std::vector<Cookie> GetAllCookies(bool ignoreNull = false) const;

		private:
			std::map<std::string, Cookie> m_cookies;
			static std::string Trim(const std::string& str);
		};
#pragma endregion HttpCookiesWrapper

#pragma region HttpRequest

		/// <summary>
		/// HTTP��Ӧ���ݽṹ��
		/// </summary>
		struct HttpResponse {
			bool success = true;
			std::string error = "";
			std::string content = "";
			int code = 0; // ��Ӧ״̬��
			std::string org_headers = ""; // ԭʼ������Ӧͷ
			HttpHeadersWrapper headers;
			HttpCookiesWrapper cookies;
			int curl_code = 0; // curl�����
		};

		class HttpRequest {
		public:
			/// <summary>
			/// ����GET����
			/// </summary>
			/// <param name="url">Ҫ����ĵ�ַ</param>
			/// <param name="headersEx">ÿ��ͷռһ�У�����ͷ��ʽ���� Content-Type: application/json</param>
			/// <param name="proxy">�����ַ</param>
			/// <param name="ssl">�Ƿ���ssl��֤��Ĭ��Ϊtrue</param>
			/// <param name="lpfnCurlOptions">�������ú����ص�������һΪCURL*ָ��</param>
			/// <returns>HttpResponse</returns>
			static HttpResponse Get(
				_In_ std::string url,
				_In_ std::string headersEx = "",
				_In_ std::string proxy = "",
				_In_ bool ssl = true,
				_In_ std::function<void(CURL*)> lpfnCurlOptions = nullptr);

			/// <summary>
			/// ����POST����
			/// </summary>
			/// <param name="url">Ҫ����ĵ�ַ</param>
			/// <param name="postData">POST���͵�����</param>
			/// <param name="headersEx">ÿ��ͷռһ�У�����ͷ��ʽ���� Content-Type: application/json</param>
			/// <param name="proxy">�����ַ</param>
			/// <param name="ssl">�Ƿ���ssl��֤��Ĭ��Ϊtrue</param>
			/// <param name="lpfnCurlOptions">�������ú����ص�������һΪCURL*ָ��</param>
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