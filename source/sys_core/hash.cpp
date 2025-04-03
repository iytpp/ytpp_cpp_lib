#include "sys_core/hash.h"
#include "sys_core/sys_core.h"

#include <openssl/evp.h>
#include <iomanip>
#include <sstream>



namespace ytpp {
	namespace sys_core {

		/// <summary>
		/// 计算哈希值
		/// </summary>
		/// <param name="input"></param>
		/// <returns></returns>
		std::string get_hash(const std::string& input, HashType hash_type) {
			EVP_MD_CTX* ctx = EVP_MD_CTX_new();
			if (!ctx) {
				return "";
			}

			const EVP_MD* hash_md;
			switch (hash_type)
			{
				case HashType::MD5:
					hash_md = EVP_md5();
					break;
				case HashType::SHA1:
					hash_md = EVP_sha1();
					break;
				case HashType::SHA256:
					hash_md = EVP_sha256();
					break;
				case HashType::SHA512:
					hash_md = EVP_sha512();
					break;
				default:
					hash_md = EVP_md5();
			}

			// 初始化sha256摘要算法
			if (EVP_DigestInit_ex(ctx, hash_md, nullptr) != 1) {
				EVP_MD_CTX_free(ctx);
				return "";
			}

			// 更新摘要上下文以包含输入数据
			if (EVP_DigestUpdate(ctx, input.c_str(), input.size()) != 1) {
				EVP_MD_CTX_free(ctx);
				return "";
			}

			unsigned char sha256_result[EVP_MAX_MD_SIZE];
			unsigned int result_len = EVP_MD_size(hash_md);

			// 获取最终的sha256摘要结果
			if (EVP_DigestFinal_ex(ctx, sha256_result, &result_len) != 1) {
				EVP_MD_CTX_free(ctx);
				return "";
			}
			EVP_MD_CTX_free(ctx);

			// 将二进制结果转换为十六进制字符串表示
			std::stringstream ss;
			for (unsigned int i = 0; i < result_len; ++i) {
				ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(sha256_result[i]);
			}

			return ss.str();
		}


	}
}