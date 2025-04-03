#ifndef _STRINGEX_H_
#define _STRINGEX_H_

#include <vector>
#include <string>

namespace ytpp {
	namespace sys_core {
		/// <summary>
		/// �ָ��ı�
		/// </summary>
		/// <param name="s"></param>
		/// <param name="delimiter"></param>
		/// <returns></returns>
		std::vector<std::string> str_split(const std::string& s, char delimiter);

		/// <summary>
		/// �ָ��ı�
		/// </summary>
		/// <param name="s"></param>
		/// <param name="delimiter"></param>
		/// <returns></returns>
		std::vector<std::string> str_split(const std::string& s, const std::string& delimiter);

		/// <summary>
		/// �滻���ַ�����������������oldSubStr�滻ΪnewSubStr
		/// </summary>
		/// <param name="s"></param>
		/// <param name="oldSubStr"></param>
		/// <param name="newSubStr"></param>
		/// <returns></returns>
		std::string str_replace_subString(const std::string& s, const std::string& oldSubStr, const std::string& newSubStr);

		/// <summary>
		/// ȥ���ַ�����β�ո�
		/// </summary>
		/// <param name="s"></param>
		/// <returns></returns>
		std::string str_trim(const std::string& s);

		/// <summary>
		/// ���ַ���ת��Ϊ��д
		/// </summary>
		/// <param name="s"></param>
		/// <returns></returns>
		std::string str_toupper(const std::string& s);

		/// <summary>
		/// ���ַ���ת��ΪСд
		/// </summary>
		/// <param name="s"></param>
		/// <returns></returns>
		std::string str_tolower(const std::string& s);

	}
}


#endif /* _STRINGEX_H_ */