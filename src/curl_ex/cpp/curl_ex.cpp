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

		/* ����GET���� */
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
				ret.error = "CURL��ʼ��ʧ��";
				return ret; // ��ʼ��ʧ��
			}

			struct curl_slist* headers = NULL;
			headers = curl_slist_append(headers, "Content-Type: "); // ɾ��Content-Type���Լ����
			headers = curl_slist_append(headers, headersEx.c_str());

			curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ret.content); // ��ȡ��Ӧ����
			curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
			curl_easy_setopt(curl, CURLOPT_HEADERDATA, &ret.headers); // ��ȡ��Ӧͷ
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers); // ��������ͷ
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, ssl ? 1L : 0L); // ��֤������ SSL ֤��
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, ssl ? 2L : 0L); // ��֤������������
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // ֧���ض���
			if (!proxy.empty()) {
				curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str()); // ���ô���
			}

			if (lpfnCurlOptions) {
				lpfnCurlOptions(curl); // ���ûص���������������ѡ��
			}


			CURLcode res = curl_easy_perform(curl);
			if (res != CURLE_OK) {
				ret.success = false;
				ret.error = std::string(curl_easy_strerror(res));
			} else {
				ret.success = true;
				// ��ȡ��Ӧ��
				long response_code = 0;
				curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
				ret.code = response_code;
				// ��Ӧ���ݺ���Ӧͷֱ���ûص�������ȡ�ˣ����ﲻ�ô���
				// ����Cookies
				ret.cookies.ParseCookies(ret.headers);
			}

			curl_slist_free_all(headers);
			curl_easy_cleanup(curl);
			return ret;
		}

		/* ����POST���� */
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
				ret.error = "CURL��ʼ��ʧ��";
				return ret; // ��ʼ��ʧ��
			}

			struct curl_slist* headers = NULL;
			headers = curl_slist_append(headers, "Content-Type: ");
			headers = curl_slist_append(headers, headersEx.c_str());

			curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
			curl_easy_setopt(curl, CURLOPT_POST, 1L);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ret.content); // ��ȡ��Ӧ����
			curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
			curl_easy_setopt(curl, CURLOPT_HEADERDATA, &ret.headers); // ��ȡ��Ӧͷ
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers); // ��������ͷ
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, ssl ? 1L : 0L);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, ssl ? 2L : 0L);
			if (!proxy.empty()) {
				curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str()); // ���ô���
			}

			CURLcode res = curl_easy_perform(curl);
			if (res != CURLE_OK) {
				ret.success = false;
				ret.error = std::string(curl_easy_strerror(res));
			} else {
				ret.success = true;
				// ��ȡ��Ӧ��
				long response_code = 0;
				curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
				ret.code = response_code;
				// ��Ӧ���ݺ���Ӧͷֱ���ûص�������ȡ�ˣ����ﲻ�ô���
				// ����Cookies
				ret.cookies.ParseCookies(ret.headers);
			}

			curl_slist_free_all(headers);
			curl_easy_cleanup(curl);
			return ret;
		}

		/* �ص������������ص�����д��output */
		static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
			size_t totalSize = size * nmemb;
			output->append((char*)contents, totalSize);
			return totalSize;
		}
		/* �ص�����������д����յ���HTTP��Ӧͷ���� */
		static size_t HeaderCallback(void* contents, size_t size, size_t nmemb, std::string* header) {
			size_t totalSize = size * nmemb;
			header->append((char*)contents, totalSize);
			return totalSize;
		}

		/* Cookies������ */
		HttpCookiesWrapper::HttpCookiesWrapper(_In_ std::string responseHeaders)
		{
			ParseCookies(responseHeaders);
		}

		/* Ĭ�Ϲ��� */
		HttpCookiesWrapper::HttpCookiesWrapper()
		{
			//ɶҲ��д
		}

		/* ����Cookies */
		void HttpCookiesWrapper::ParseCookies(_In_ std::string responseHeaders)
		{
			this->cookies.clear(); // ���֮ǰ��cookie

			std::istringstream stream(responseHeaders);
			std::string line;
			std::string lastValue = "";  // �洢���һ���ǿյ�ֵ

			while (std::getline(stream, line)) {
				// ֻ���� Set-Cookie ��
				if (line.find("set-cookie:") == 0 || line.find("Set-Cookie:") == 0) {
					size_t start = line.find(":") + 1;
					std::string cookie = line.substr(start);

					// ȡ�� key=value
					cookie = cookie.substr(0, cookie.find(";"));
					cookie = Trim(cookie);  // ȥ��ǰ��ո�

					// ���� key
					size_t equalPos = cookie.find("=");
					if (equalPos != std::string::npos) {
						std::string cookieKey = cookie.substr(0, equalPos);
						std::string cookieValue = cookie.substr(equalPos + 1);

						if (!cookieValue.empty()) {
							lastValue = cookieValue;  // ֻ�е� value ��Ϊ��ʱ�Ÿ���
							this->cookies[cookieKey] = cookieValue;
						}
					}
				}
			}
		}

		/* ȥ��ǰ��Ŀո� */
		std::string HttpCookiesWrapper::Trim(_In_ const std::string& str) {
			size_t first = str.find_first_not_of(" \t\r\n");
			size_t last = str.find_last_not_of(" \t\r\n");
			return (first == std::string::npos) ? "" : str.substr(first, last - first + 1);
		}

		/* ��ȡ����Cookieֵ */
		std::string HttpCookiesWrapper::GetCookieValue(_In_ const std::string& key)
		{
			return this->cookies[key];
		}

		/* ��Cookies�����key=value;key=value;����ʽ */
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