#include <iostream>
#include <windows.h>

#include "launch.h"

#include "sys_core.h"
#include "jsoncpp_ex.h"


using namespace std;
using namespace ytpp;


struct ttt {
	int a;
	int b;
	int c;
};

int main() {
	ttt t = { 1,2,3 };
	WritePrivateProfileStructA(
		"sys", "key", &t, sizeof( ttt ),
		R"(F:\project\ytpp_cpp_lib\launch_for_test\123.ini)" );

	ttt t2 = { 0 };
	GetPrivateProfileStructA(
		"sys", "key", &t2, sizeof( ttt ),
		R"(F:\project\ytpp_cpp_lib\launch_for_test\123.ini)" );
	cout << t2.a << " " << t2.b << " " << t2.c << endl;
	
	system( "pause" );

	return 0;
}