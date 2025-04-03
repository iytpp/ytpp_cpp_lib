#ifdef _DEBUG //调试的时候使用标准输出进行调试
#include <iostream>
#define COUT std::cout
#else
#define COUT /##/std::cout
#endif // _DEBUG