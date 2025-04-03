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
			/// ������Ӧͷ�е�cookie
			/// </summary>
			/// <param name="responseHeaders"></param>
			void ParseCookies(_In_ std::string responseHeaders);
			
			/// <summary>
			/// ��ȡָ��cookie��ֵ
			/// </summary>
			/// <param name="key"></param>
			/// <returns></returns>
			std::string GetCookieValue(const std::string& key);
			
			/// <summary>
			/// ��ȡ����cookie��ֵ����ʽΪ key1=value1; key2=value2
			/// </summary>
			/// <returns></returns>
			std::string GetAllCookies();
		private:
			std::map<std::string, std::string> cookies;
			static std::string Trim(_In_ const std::string& str);// ȥ���ַ���ǰ��Ŀո�
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


	}


}