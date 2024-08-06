#ifndef JSONCPP_EX_CPP
#define JSONCPP_EX_CPP

#include "./jsoncpp/json.h"
#include <string>

namespace ytpp {
	namespace json
	{

		using namespace std;

		/*
		* @brief 将json的value对象转换为UTF8编码的字符串
		* @param [in] value: 要转换的json对象
		* @return UTF8编码的字符串
		*/
		string json_toString(_In_ const Json::Value & value);
		/*
		* @brief 将json的value对象转换为UTF8编码的字符串，并指定是否进行格式化
		* @param [in] value: 要转换的json对象
		* @param [in] format: 是否进行格式化
		* @return UTF8编码的字符串
		*/
		string json_toString(
			_In_ const Json::Value & value,
			_In_ bool format
		);
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
			_In_ const Json::Value & value,
			_In_ bool commentStyle = "All",
			_In_ string indentation = "	",
			_In_ bool enbleYAMLCompatibility = false,
			_In_ bool dropNullPlaceholders = false,
			_In_ bool useSpecialFloats = false,
			_In_ int precision = 5,
			_In_ string precisionType = "significant",
			_In_ bool emitUTF8 = false
		);

		/*
		* @brief 将UTF8编码的字符串转换为json的value对象，默认收集注释
		* @brief 如果不想收集注释，可以使用另一个重载版本的第二个参数，填false即可
		* @param [in] jsonStr: 要转换的UTF8编码的字符串
		* @param [out] value: 转换后的json对象的引用
		* @return 成功与否
		*/
		bool json_fromString(
			_In_ const string & jsonStr,
			_Out_ Json::Value & value
		);

		/*
		* @brief 将UTF8编码的字符串转换为json的value对象，并指定是否收集注释
		* @param [in] jsonStr: 要转换的UTF8编码的字符串
		* @param [out] value: 转换后的json对象的引用
		* @param [in] collectComments: 是否收集注释
		* @return 成功与否
		*/
		bool json_fromString(
			_In_ const string & jsonStr,
			_Out_ Json::Value & value,
			_In_ bool collectComments
		);

		/*
		* @brief 【高级版本】将UTF8编码的字符串转换为json的value对象，并指定所有可选参数
		* @brief 如果不想使用默认值，可以不填参数，使用默认值
		* @param [in] jsonStr: 要转换的UTF8编码的字符串
		* @param [out] value: 转换后的json对象的引用
		* @param [out] error: 错误信息
		* @param [in] collectComments: 是否收集注释，如果allowComments为false，则此参数无效
		* @param [in] allowComments: 是否允许注释
		* @param [in] allowTrailingCommas: 是否允许数组和对象尾部有多余的逗号
		* @param [in] strictRoot: 是否严格检查根节点，如果根节点不是对象或数组，则抛出异常
		* @param [in] allowDroppedNullPlaceholders: 是否允许null占位符，和json_toStringEx函数中的dropNullPlaceholders参数对应
		* @param [in] allowNumericKeys: 是否允许数字作为键
		* @param [in] allowSingleQuotes: 是否允许字符串的键和值（即键和值使用单引号的时候）
		* @param [in] stackLimit: 栈限制，默认1024
		* @param [in] failIfExtra: 如果填true，则当输入字符串中的JSON值后面有额外的非空白时返回false
		* @param [in] rejectDupKeys: 如果填true，则当输入字符串中有重复的键时，抛出异常
		* @param [in] allowSpecialFloats: 是否允许特殊浮点数，和json_toStringEx函数中的useSpecialFloats参数对应
		* @param [in] skipBom: 如果填true，则当输入以Unicode字节顺序标记(BOM)开头时，跳过它
		* @return json的value对象
		*/
		bool json_fromStringEx(
			_In_ const string & jsonStr,
			_Out_ Json::Value & value,
			_Out_ Json::String & error,
			_In_ bool collectComments = true,
			_In_ bool allowComments = true,
			_In_ bool allowTrailingCommas = true,
			_In_ bool strictRoot = false,
			_In_ bool allowDroppedNullPlaceholders = true,
			_In_ bool allowNumericKeys = true,
			_In_ bool allowSingleQuotes = true,
			_In_ int stackLimit = 1024,
			_In_ bool failIfExtra = false,
			_In_ bool rejectDupKeys = false,
			_In_ bool allowSpecialFloats = true,
			_In_ bool skipBom = true
		);

	}
}


#endif // JSONCPP_EX_CPP