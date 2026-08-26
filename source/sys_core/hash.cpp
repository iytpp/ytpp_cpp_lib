#include "sys_core/hash.h"
#include "sys_core/sys_core.h"





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


        // 生成随机数据，需要提供缓冲区，速度更快
		void CreateRandomData( void* buffer, std::size_t dataLength) {
			if (!buffer || dataLength == 0)
				return;

			static thread_local std::uint64_t s[4]{};
			static thread_local bool initialized = false;

			auto rotl = [](std::uint64_t x, int k) -> std::uint64_t
			{
				return (x << k) | (x >> (64 - k));
			};

			auto splitmix64 = [](std::uint64_t& x) -> std::uint64_t
			{
				std::uint64_t z = (x += 0x9E3779B97F4A7C15ULL);
				z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
				z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
				return z ^ (z >> 31);
			};

			if (!initialized)
			{
				std::uint64_t seed =
					static_cast<std::uint64_t>(
					std::chrono::high_resolution_clock::now()
					.time_since_epoch()
					.count()
					);

				seed ^=
					static_cast<std::uint64_t>(
					std::hash<std::thread::id>{}(
					std::this_thread::get_id()
					)
					);

				seed ^=
					reinterpret_cast<std::uintptr_t>(&seed);

				for (auto& v : s)
					v = splitmix64(seed);

				initialized = true;
			}

			auto next64 = [&]() -> std::uint64_t
			{
				const std::uint64_t value =
					rotl(s[1] * 5, 7) * 9;

				const std::uint64_t t =
					s[1] << 17;

				s[2] ^= s[0];
				s[3] ^= s[1];
				s[1] ^= s[2];
				s[0] ^= s[3];

				s[2] ^= t;
				s[3] = rotl(s[3], 45);

				return value;
			};

			auto* out =
				static_cast<std::uint8_t*>(buffer);

			std::size_t offset = 0;

			while (
				offset + sizeof(std::uint64_t)
				<= dataLength
				)
			{
				const std::uint64_t value =
					next64();

				std::memcpy(
					out + offset,
					&value,
					sizeof(value)
				);

				offset += sizeof(value);
			}

			if (offset < dataLength)
			{
				const std::uint64_t value =
					next64();

				std::memcpy(
					out + offset,
					&value,
					dataLength - offset
				);
			}
		}

		// 生成随机数据，返回vector，使用更方便
		std::vector<std::uint8_t> CreateRandomData(std::size_t dataLength)
		{
			std::vector<std::uint8_t> result(
				dataLength
			);

			if (!result.empty())
			{
				CreateRandomData(
					result.data(),
					result.size()
				);
			}

			return result;
		}

	}
}