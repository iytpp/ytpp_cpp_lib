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
			static std::string Trim(const std::string& str);// ȥ���ַ���ǰ��Ŀո�
		};

		/// <summary>
		/// HTTP��Ӧ���ݽṹ��
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
			/// ����GET����
			/// </summary>
			/// <param name="url">Ҫ����ĵ�ַ</param>
			/// <param name="headersEx">ÿ��ͷռһ�У�����ͷ��ʽ���� Content-Type: application/json</param>
			/// <param name="proxy">�����ַ</param>
			/// <param name="ssl">�Ƿ���ssl��֤��Ĭ��Ϊtrue</param>
			/// <param name="lpfnCurlOptions">�������ú����ص�������һΪCURL*ָ��</param>
			/// <returns>HttpResponse</returns>
			static HttpResponse Get(std::string url, std::string headersEx = "", std::string proxy = "", bool ssl = true, std::function<void(CURL*)> lpfnCurlOptions = nullptr);
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
			static HttpResponse Post(std::string url, std::string postData = "", std::string headersEx = "", std::string proxy = "", bool ssl = true, std::function<void(CURL*)> lpfnCurlOptions = nullptr);
		};


	}


}