//这是一个综合头文件，引用这一个就相当于引用整个sys_core


#ifndef _SYS_CORE_H_
#define _SYS_CORE_H_

//兼容性文件，用于兼容 MultiByte 和 WideChar
//#include "compatibility.h"

//磁盘操作
#include "sys_core/disk_manipulation.h"
//编码转换
#include "sys_core/encoding.h"
//系统处理
#include "sys_core/sys_processing.h"
//字符串处理
#include "sys_core/string_ex.h"
//hash操作
#include "sys_core/hash.h"
//机器特征
#include "sys_core/machine_feature.h"



#endif // !_SYS_CORE_H_