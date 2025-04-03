#ifndef _HASH_H_
#define _HASH_H_

#include <string>

namespace ytpp {
	namespace sys_core {

		enum class HashType {
			MD5 = 1,
			SHA1,
			SHA256,
			SHA512
		};

		/// <summary>
		/// SHA256º∆À„
		/// </summary>
		/// <param name="input"></param>
		/// <returns></returns>
		std::string get_hash(const std::string& input, HashType hash_type = HashType::SHA256);


	}
}








#endif /* _HASH_H_ */