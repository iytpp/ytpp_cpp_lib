#ifndef _STRINGEX_H_
#define _STRINGEX_H_

#include <vector>
#include <string>

namespace ytpp {
	namespace sys_core {
		/// <summary>
		/// 分割文本
		/// </summary>
		/// <param name="s"></param>
		/// <param name="delimiter"></param>
		/// <returns></returns>
		std::vector<std::string> str_split(const std::string& s, char delimiter);

		/// <summary>
		/// 分割文本
		/// </summary>
		/// <param name="s"></param>
		/// <param name="delimiter"></param>
		/// <returns></returns>
		std::vector<std::string> str_split(const std::string& s, const std::string& delimiter);

		/// <summary>
		/// 替换子字符串，将主串中所有oldSubStr替换为newSubStr
		/// </summary>
		/// <param name="s"></param>
		/// <param name="oldSubStr"></param>
		/// <param name="newSubStr"></param>
		/// <returns></returns>
		std::string str_replace_subString(const std::string& s, const std::string& oldSubStr, const std::string& newSubStr);

		/// <summary>
		/// 去除字符串首尾空格
		/// </summary>
		/// <param name="s"></param>
		/// <returns></returns>
		std::string str_trim(const std::string& s);

		/// <summary>
		/// 将字符串转换为大写
		/// </summary>
		/// <param name="s"></param>
		/// <returns></returns>
		std::string str_toupper(const std::string& s);

		/// <summary>
		/// 将字符串转换为小写
		/// </summary>
		/// <param name="s"></param>
		/// <returns></returns>
		std::string str_tolower(const std::string& s);

	}
}


#endif /* _STRINGEX_H_ */