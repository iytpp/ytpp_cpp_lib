# ytpp_cpp_lib

### 介绍
​	本项目计划开发出一套方便的综合库，主要的受益对象是中小型项目，使得在C++中能用极短的代码写出功能丰富的程序，提高开发效率，说白了就是造轮子，造一个和易语言生态媲美的轮子。本项目暂时仅提供静态库，可以直接编译入exe中。

### 平台限制
​	项目目前不考虑跨平台，只提供Windows的WIN32和WIN64平台的库。

### 安装教程

 **关于源文件路径的环境变量** 

​	在任何电脑上编译项目之前，需要配置源代码文件所在根目录的环境变量。创建一个名为 YTPP_CPP_LIB 的环境变量，值为源代码文件的根目录，通常是 ytpp_cpp_lib 文件夹的路径，后面不带反斜杠。  



### 库说明

 **开发各个库的时候需要引入的include文件夹** 

1. &#36;(YTPP_CPP_LIB)\src\\\.private\include
2. &#36;(YTPP_CPP_LIB)\src\\\.common\include
3. 各个库自己的include文件夹，例如：&#36;(YTPP_CPP_LIB)\\src\\xxxxxx\\include

 **使用各个库的时候需要引入的include文件夹** 

1. &#36;(YTPP_CPP_LIB)\src\\\.common\include
2. 各个库文件夹中的include文件夹，例如：&#36;(YTPP_CPP_LIB)\\src\\xxxxxx\\include

### 参与贡献

樱桃屁屁负责全部代码维护

