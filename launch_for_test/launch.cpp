#include <iostream>
#include <windows.h>

#include "launch.h"

#include "sys_core.h"
#include "jsoncpp_ex.h"


using namespace std;
using namespace ytpp;


int main() {
	Json::Value root;
	string js = u8"{\"name\":\"ytpp 韩文：한국어\",\"age\":18}";
	
	if ( !json_fromString( js, root ) ) {
		cout << "json_fromString error" << endl;
	} else {
		root["num"] = 123;
		cout << "json_fromString success" << endl;
		cout << root["num"].asString() << endl;
	}


	write_to_file( _T( "test.txt" ), root["name"].asCString() );
	
	
	system( "pause" );

	return 0;
}