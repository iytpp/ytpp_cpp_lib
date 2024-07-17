#ifndef JSONCPP_EX_CPP
#define JSONCPP_EX_CPP

#include "./jsoncpp/json.h"
#include <string>

namespace ytpp {
	using namespace std;


	/*
	* @brief 将json的value对象转换为UTF8编码的字符串
	* @param [in] value: 要转换的json对象
	* @return UTF8编码的字符串
	*/
	string json_toString( _In_ const Json::Value& value );
	/*
	* @brief 将json的value对象转换为UTF8编码的字符串，并指定是否进行格式化
	* @param [in] value: 要转换的json对象
	* @param [in] format: 是否进行格式化
	* @return UTF8编码的字符串
	*/
	string json_toString( 
		_In_ const Json::Value& value, 
		_In_ bool format );
	/*
	* @brief 【高级版本】将json的value对象转换为可选UTF8编码的字符串，并指定所有可选参数
	* @param [in] value: 要转换的json对象
	* @param [in] commentStyle: 是否包含注释，取值："None", "All"
	* @param [in] indentation: 缩进字符
	* @param [in] enbleYAMLCompatibility: 是否启用YAML兼容性，稍微改变冒号周围的空白
	* @param [in] dropNullPlaceholders: 是否删除null占位符
	* @param [in] useSpecialFloats: 是否使用特殊浮点数，为true时NaN值为"NaN"，正无穷为"infinity"，负无穷为"-infinity"
	* @param [in] precision: 小数点精度，一个整数值
	* @param [in] precisionType: 小数点精度类型，取值："significant", "decimal"
	* @param [in] emitUTF8: 是否输出UTF8编码
	* @return 转换后的字符串
	*/
	string json_toStringEx(
		_In_ const Json::Value& value,
		_In_ bool commentStyle = "All",
		_In_ string indentation = "	",
		_In_ bool enbleYAMLCompatibility = false,
		_In_ bool dropNullPlaceholders = false,
		_In_ bool useSpecialFloats = false,
		_In_ int precision = 5,
		_In_ string precisionType = "significant",
		_In_ bool emitUTF8 = false);





}


#endif // JSONCPP_EX_CPP