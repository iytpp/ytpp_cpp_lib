#ifndef _HASH_H_
#define _HASH_H_

#include <string>
#include <openssl/evp.h>
#include <iomanip>
#include <sstream>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>
#include <chrono>
#include <atomic>
#include <thread>
#include <functional>


namespace ytpp {
	namespace sys_core {

		enum class HashType {
			MD5 = 1,
			SHA1,
			SHA256,
			SHA512
		};

		/// <summary>
		/// SHA256计算
		/// </summary>
		/// <param name="input"></param>
		/// <returns></returns>
		std::string get_hash(const std::string& input, HashType hash_type = HashType::SHA256);

		/*
		* @brief 生成伪随机数据，侧重点是完全随机的数据，无任何规律
		* @param dataLength 随机数据长度
		* @return
		*/
		std::vector<std::uint8_t> CreateRandomData(std::size_t dataLength);

		/*
		* @brief 生成伪随机数据，侧重点是完全随机的数据，无任何规律，适用于对速度要求更高的场景，由调用者提供缓冲区，避免频繁分配和释放
		* @param dataLength 随机数据长度
		* @return
		*/
		void CreateRandomData( void* buffer, std::size_t dataLength );

	}
}








#endif /* _HASH_H_ */