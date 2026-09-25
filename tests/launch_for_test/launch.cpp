#include <iostream>
#include <filesystem>
#include <windows.h>

#include "launch.h"

#include "sys_core/sys_core.h"
#include "jsoncpp_ex/jsoncpp_ex.h"
#include "sys_core/machine_feature.h"
#include "curl_ex/curl_ex.h"

using namespace ytpp::sys_core;
using namespace ytpp::curl_ex;

int main() {
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    // string a = R"(  我爱编程||是的||啊啊   )";
    // auto s = Split(a, "||");
    // for (auto& i : s) {
    //	cout << i << endl;
    // }

    // hw.shanchendaili.com:1000

    // UrlParameters url_params("param1=111&param2=222");
    // url_params.set("param3", "333");
    // cout << url_params["param1"] << endl;
    // cout << url_params.toString() << endl;
    // cout << url_params.size() << endl;

    // auto a = url_params.getAllParamNames();
    // for (auto& i : a) {
    //	cout << i << endl;
    // }

    // auto result = GetNetworkDateTime();
    // cout << result.year << "-" << result.month << "-" << result.day << " " << result.hour << ":" << result.minute <<
    // ":" << result.second << endl;

    // cout << GetExePath().parent_path().string();

    // cout << GeneratePseudoRandomBytes(100).size() << endl;

    // string a = EncryptAes("123456哈哈哈哈", "啊哈哈哈");
    // string b = DecryptAes(a, "啊哈哈哈");
    // cout << b << endl;

    // RequestOptions options;
    // options.proxy = ProxyOptions{};
    // options.proxy->url = "127.0.0.1:8888";
    // auto result = HttpRequest::Head("https://www.baidu.com", options);
    // if (result.Ok()) {
    //	cout << "success" << endl;
    //	cout << "headers = \n" << result.org_headers << endl;
    // }else{
    //	cout << "error_curl_code = " << result.curl_code << endl;
    //	cout << "error_code = " << result.code << endl;
    //	cout << "error = " << result.error << endl;
    // }

    // cout << GetKnownFolderPathUtf8(FOLDERID_LocalAppData) << endl;

    // auto result_response = HttpRequest::Get("https://www.baidu.com");
    // cout << result_response.curl_code << endl;
    // cout << result_response.error << endl;
    // cout << result_response.content << endl;

    system("pause");

    return 0;
}
