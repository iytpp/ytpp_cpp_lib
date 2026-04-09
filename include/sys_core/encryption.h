#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace ytpp::sys_core::encryption {

using Bytes = std::vector<unsigned char>;

struct CipherPack {
    Bytes iv;
    Bytes ciphertext;
    Bytes tag;
};

struct PemKeyPair {
    std::string public_key_pem;
    std::string private_key_pem;
};

struct FileEncryptResult {
    Bytes iv;
    Bytes tag;
    std::uint64_t plain_size = 0;
    std::uint64_t cipher_size = 0;
};

struct PasswordAesKeyInfo {
    Bytes key;
    Bytes iv;
    Bytes salt;
};

struct PasswordFileEncryptResult : FileEncryptResult {
    Bytes salt;
    int iterations = 0;
};

class OpenSslException : public std::runtime_error {
public:
    explicit OpenSslException(const std::string& msg);
};

std::string GetOpenSslErrors();
[[noreturn]] void ThrowLastError(const std::string& where);

Bytes ToBytes(std::string_view s);
std::string ToString(const Bytes& data);
std::string HexEncode(const Bytes& data, bool upper = false);
Bytes HexDecode(std::string_view hex);
std::string Base64Encode(const Bytes& data, bool no_new_line = true);
Bytes Base64Decode(std::string_view base64, bool no_new_line = true);
std::string BytesToBase64(const Bytes& data);
Bytes Base64ToBytes(std::string_view s);
Bytes RandomBytes(std::size_t n);
std::string RandomHex(std::size_t n);

Bytes MD5(std::string_view s);
Bytes SHA1(std::string_view s);
Bytes SHA224(std::string_view s);
Bytes SHA256(std::string_view s);
Bytes SHA384(std::string_view s);
Bytes SHA512(std::string_view s);
#if !defined(OPENSSL_NO_SM3)
Bytes SM3(std::string_view s);
#endif

Bytes HMAC_MD5(std::string_view data, std::string_view key);
Bytes HMAC_SHA1(std::string_view data, std::string_view key);
Bytes HMAC_SHA256(std::string_view data, std::string_view key);
Bytes HMAC_SHA512(std::string_view data, std::string_view key);

Bytes PBKDF2_HMAC_SHA256(std::string_view password,
                         const Bytes& salt,
                         int iterations,
                         std::size_t out_len);
Bytes PBKDF2_HMAC_SHA512(std::string_view password,
                         const Bytes& salt,
                         int iterations,
                         std::size_t out_len);
Bytes Scrypt(std::string_view password,
             const Bytes& salt,
             std::uint64_t N,
             std::uint64_t r,
             std::uint64_t p,
             std::uint64_t max_mem,
             std::size_t out_len);
Bytes HKDF_SHA256(const Bytes& ikm,
                  const Bytes& salt,
                  const Bytes& info,
                  std::size_t out_len);

Bytes DeriveAes256KeyFromPassword_PBKDF2_SHA256(std::string_view password,
                                                const Bytes& salt,
                                                int iterations = 100000);
Bytes DeriveAes256KeyFromPassword_Scrypt(std::string_view password,
                                         const Bytes& salt,
                                         std::uint64_t N = 1u << 15,
                                         std::uint64_t r = 8,
                                         std::uint64_t p = 1,
                                         std::uint64_t max_mem = 64ull * 1024ull * 1024ull);
PasswordAesKeyInfo DeriveAes256KeyAndIvFromPassword_PBKDF2_SHA256(std::string_view password,
                                                                  const Bytes& salt,
                                                                  int iterations = 100000,
                                                                  std::size_t iv_len = 12);
CipherPack PasswordEncrypt_AES256_GCM_PBKDF2(std::string_view password,
                                             const Bytes& plaintext,
                                             const Bytes& salt,
                                             int iterations = 100000,
                                             const Bytes& aad = {},
                                             const Bytes& iv = {});
Bytes PasswordDecrypt_AES256_GCM_PBKDF2(std::string_view password,
                                        const CipherPack& pack,
                                        const Bytes& salt,
                                        int iterations = 100000,
                                        const Bytes& aad = {});

Bytes AES_128_CBC_Encrypt(const Bytes& plaintext, const Bytes& key, const Bytes& iv);
Bytes AES_128_CBC_Decrypt(const Bytes& ciphertext, const Bytes& key, const Bytes& iv);
Bytes AES_256_CBC_Encrypt(const Bytes& plaintext, const Bytes& key, const Bytes& iv);
Bytes AES_256_CBC_Decrypt(const Bytes& ciphertext, const Bytes& key, const Bytes& iv);
Bytes AES_128_CTR_Encrypt(const Bytes& plaintext, const Bytes& key, const Bytes& iv);
Bytes AES_128_CTR_Decrypt(const Bytes& ciphertext, const Bytes& key, const Bytes& iv);
Bytes AES_256_CTR_Encrypt(const Bytes& plaintext, const Bytes& key, const Bytes& iv);
Bytes AES_256_CTR_Decrypt(const Bytes& ciphertext, const Bytes& key, const Bytes& iv);
CipherPack AES_128_GCM_Encrypt(const Bytes& plaintext, const Bytes& key, const Bytes& iv, const Bytes& aad = {});
Bytes AES_128_GCM_Decrypt(const CipherPack& pack, const Bytes& key, const Bytes& aad = {});
CipherPack AES_256_GCM_Encrypt(const Bytes& plaintext, const Bytes& key, const Bytes& iv, const Bytes& aad = {});
Bytes AES_256_GCM_Decrypt(const CipherPack& pack, const Bytes& key, const Bytes& aad = {});
CipherPack ChaCha20_Poly1305_Encrypt(const Bytes& plaintext, const Bytes& key, const Bytes& iv, const Bytes& aad = {});
Bytes ChaCha20_Poly1305_Decrypt(const CipherPack& pack, const Bytes& key, const Bytes& aad = {});
#if !defined(OPENSSL_NO_SM4)
Bytes SM4_CBC_Encrypt(const Bytes& plaintext, const Bytes& key, const Bytes& iv);
Bytes SM4_CBC_Decrypt(const Bytes& ciphertext, const Bytes& key, const Bytes& iv);
Bytes SM4_CTR_Encrypt(const Bytes& plaintext, const Bytes& key, const Bytes& iv);
Bytes SM4_CTR_Decrypt(const Bytes& ciphertext, const Bytes& key, const Bytes& iv);
#endif

PemKeyPair GenerateRSAKeyPair(int bits = 2048);
PemKeyPair GenerateECKeyPair_P256();
PemKeyPair GenerateEd25519KeyPair();
std::string PublicKeyPemFromPrivateKeyPem(const std::string& private_key_pem);
Bytes RSA_Encrypt_OAEP_SHA256(const std::string& public_key_pem, const Bytes& plaintext);
Bytes RSA_Decrypt_OAEP_SHA256(const std::string& private_key_pem, const Bytes& ciphertext);
Bytes RSA_Sign_PSS_SHA256(const std::string& private_key_pem, const Bytes& data);
bool RSA_Verify_PSS_SHA256(const std::string& public_key_pem, const Bytes& data, const Bytes& sig);
Bytes RSA_Sign_PKCS1v15_SHA256(const std::string& private_key_pem, const Bytes& data);
bool RSA_Verify_PKCS1v15_SHA256(const std::string& public_key_pem, const Bytes& data, const Bytes& sig);
Bytes ECDSA_P256_Sign_SHA256(const std::string& private_key_pem, const Bytes& data);
bool ECDSA_P256_Verify_SHA256(const std::string& public_key_pem, const Bytes& data, const Bytes& sig);
Bytes Ed25519_Sign(const std::string& private_key_pem, const Bytes& data);
bool Ed25519_Verify(const std::string& public_key_pem, const Bytes& data, const Bytes& sig);
#if !defined(OPENSSL_NO_SM2)
Bytes SM2_Sign_SM3(const std::string& private_key_pem,
                   const Bytes& data,
                   std::string_view user_id = "1234567812345678");
bool SM2_Verify_SM3(const std::string& public_key_pem,
                    const Bytes& data,
                    const Bytes& sig,
                    std::string_view user_id = "1234567812345678");
#endif

class FileCrypto {
public:
    static FileEncryptResult EncryptFile_AES256_GCM(const std::string& input_path,
                                                    const std::string& output_path,
                                                    const Bytes& key,
                                                    const Bytes& iv,
                                                    const Bytes& aad = {},
                                                    std::size_t chunk_size = 1024 * 1024);

    static void DecryptFile_AES256_GCM(const std::string& input_path,
                                       const std::string& output_path,
                                       const Bytes& key,
                                       const Bytes& iv,
                                       const Bytes& tag,
                                       const Bytes& aad = {},
                                       std::size_t chunk_size = 1024 * 1024);

    static PasswordFileEncryptResult EncryptFileWithPassword_AES256_GCM_PBKDF2(
        const std::string& input_path,
        const std::string& output_path,
        std::string_view password,
        const Bytes& salt = {},
        int iterations = 100000,
        const Bytes& aad = {},
        const Bytes& iv = {},
        std::size_t chunk_size = 1024 * 1024);

    static void DecryptFileWithPassword_AES256_GCM_PBKDF2(const std::string& input_path,
                                                          const std::string& output_path,
                                                          std::string_view password,
                                                          const Bytes& salt,
                                                          int iterations,
                                                          const Bytes& iv,
                                                          const Bytes& tag,
                                                          const Bytes& aad = {},
                                                          std::size_t chunk_size = 1024 * 1024);

    static void EncryptFileToContainerWithPassword_AES256_GCM_PBKDF2(
        const std::string& input_path,
        const std::string& container_output_path,
        std::string_view password,
        const Bytes& salt = {},
        int iterations = 100000,
        std::size_t chunk_size = 1024 * 1024);

    static void DecryptFileFromContainerWithPassword_AES256_GCM_PBKDF2(
        const std::string& container_input_path,
        const std::string& output_path,
        std::string_view password,
        std::size_t chunk_size = 1024 * 1024);
};

} // namespace ytpp::sys_core::encryption
