# ytpp_cpp_lib

### 介绍
​	本项目计划开发出一套方便的C++综合库，主要的受益对象是中小型项目，使得在C++中能用极短的代码写出功能丰富的程序，提高开发效率，说白了就是造轮子，目标是向着“易语言”的“易”字发展。本项目已开源，本项目编译的目标文件为静态库，可以直接编译入exe中，如需DLL动态库可自行创建DLL项目并编译。

### 平台限制
​	本项目暂且不考虑跨平台，只提供Windows平台，源码中会大量调用Windows的系统API。

### 环境配置说明

 **关于源文件路径的环境变量** 

​	在任何Windows电脑上编译项目之前，需要配置源代码文件所在根目录的环境变量。创建一个名为 `YTPP_CPP_LIB` 的**系统环境变量**，值为解决方案的根目录（即 `ytpp_cpp_lib.sln` 文件所在的路径），路径尾部不带反斜杠。  



### 库说明

 **开发各个库的时候需要引入的include文件夹** 

1. `$(YTPP_CPP_LIB)\include\.common`
2. `$(YTPP_CPP_LIB)\include\.private`
3. `$(YTPP_CPP_LIB)\include`
4. `$(YTPP_CPP_LIB)\third_party\include`（如果要引用第三方库）

 **使用各个库的时候需要引入的include文件夹** 

1. `$(YTPP_CPP_LIB)\include\.common`
2. `$(YTPP_CPP_LIB)\include`

 **引用各库的方式**
	所有的库（不论是项目库还是第三方库）在 `$(YTPP_CPP_LIB)\include` 文件夹中都有以项目名为名称的文件夹，文件夹中又有对应的头文件。所以不论是项目库还是第三方库的引用方式如下。
`#include <xx/xxxx.h>`（其中xx是库名，xxxx是头文件名）

### 参与贡献

樱桃屁屁负责全部代码开发与维护

