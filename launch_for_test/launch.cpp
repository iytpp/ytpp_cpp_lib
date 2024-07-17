#include <iostream>
#include <windows.h>

#include "launch.h"

#include "sys_core.h"
#include "jsoncpp_ex.h"


using namespace std;
using namespace ytpp;

int main() {
	Json::Value root;
	string aa = "にほんご";
	cout << aa << endl;
	//中文 ほんご 한국어
	string str = u8"中文 ほんご 한국어";

	root[u8"test"] = str;
	root[u8"test2"] = str;
	root[u8"test3"] = str;
	
	string rst = json_toString( root, false );
	cout << rst << endl;

	write_to_file( _T( "test.json" ), rst.c_str() );
	
	
	system( "pause" );

	return 0;
}