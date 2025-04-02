#include "stringEx.h"

#include <sstream>
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
		std::vector<std::string> str_split(const std::string& s, char delimiter)
		{
			std::vector<std::string> tokens;
			std::string token;
			std::istringstream tokenStream(s);
			while (std::getline(tokenStream, token, delimiter))
			{
				tokens.push_back(token);
			}
			return tokens;
		}

		/// <summary>
		/// 分割文本
		/// </summary>
		/// <param name="s"></param>
		/// <param name="delimiter"></param>
		/// <returns></returns>
		std::vector<std::string> str_split(const std::string& s, const std::string& delimiter) {
			std::vector<std::string> tokens;
			size_t start = 0;
			size_t end = 0;

			while (true) {
				end = s.find(delimiter, start);
				if (end == std::string::npos) {
					tokens.push_back(s.substr(start));
					break;
				}
				tokens.push_back(s.substr(start, end - start));
				start = end + delimiter.length();
			}

			return tokens;
		}

		/// <summary>
		/// 替换子字符串，，将主串中所有oldSubStr替换为newSubStr
		/// </summary>
		/// <param name="s"></param>
		/// <param name="oldSubStr"></param>
		/// <param name="newSubStr"></param>
		/// <returns></returns>
		std::string str_replace_subString(const std::string& s, const std::string& oldSubStr, const std::string& newSubStr)
		{
			std::string result = s;
			size_t pos = result.find(oldSubStr);
			while (pos != std::string::npos) {
				result.replace(pos, oldSubStr.length(), newSubStr);
				pos = result.find(oldSubStr, pos + newSubStr.length());
			}
			return result;
		}

		/// <summary>
		/// 去除字符串首尾空格
		/// </summary>
		/// <param name="s"></param>
		/// <returns></returns>
		std::string str_trim(const std::string& s)
		{
			std::string result = s;
			result.erase(0, result.find_first_not_of(" "));
			result.erase(result.find_last_not_of(" ") + 1);
			return result;
		}





    }
}

