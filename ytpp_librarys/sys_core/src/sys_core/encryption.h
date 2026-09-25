#pragma once

#include <cstddef>
#include <cstdint>
#include <sal.h>
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
    std::string publicKeyPem;
    std::string privateKeyPem;
};

struct FileEncryptResult {
    Bytes iv;
    Bytes tag;
    std::uint64_t plainSize = 0;
    std::uint64_t cipherSize = 0;
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
    /// @brief 使用指定消息创建 OpenSSL 异常。
    /// @param msg 异常消息。
    explicit OpenSslException(_In_ const std::string& msg);
};

/// @brief 读取并合并当前线程的 OpenSSL 错误队列。
std::string GetOpenSslErrors();
/// @brief 读取 OpenSSL 错误队列并抛出异常；用于失败分支。
/// @param where 发生错误的操作位置。
[[noreturn]] void ThrowLastError(_In_ const std::string& where);

/// @brief 在文本与原始字节序列之间转换。
/// @param s 待处理文本。
Bytes ToBytes(_In_ std::string_view s);
/// @brief 在文本与原始字节序列之间转换。
/// @param data 输入字节数据。
std::string ToString(_In_ const Bytes& data);
/// @brief 将输入数据编码为 HexEncode 对应的文本形式。
/// @param data 输入字节数据。
/// @param upper 是否输出大写十六进制字符。
std::string HexEncode(_In_ const Bytes& data, _In_ bool upper = false);
/// @brief 解码 HexDecode 对应的文本并返回字节数据。
/// @param hex 十六进制文本。
Bytes HexDecode(_In_ std::string_view hex);
/// @brief 将输入数据编码为 Base64Encode 对应的文本形式。
/// @param data 输入字节数据。
/// @param noNewLine 是否禁止输出换行。
std::string Base64Encode(_In_ const Bytes& data, _In_ bool noNewLine = true);
/// @brief 解码 Base64Decode 对应的文本并返回字节数据。
/// @param base64 Base64 文本。
/// @param noNewLine 是否禁止输出换行。
Bytes Base64Decode(_In_ std::string_view base64, _In_ bool noNewLine = true);
/// @brief 将输入数据编码为 BytesToBase64 对应的文本形式。
/// @param data 输入字节数据。
std::string BytesToBase64(_In_ const Bytes& data);
/// @brief 解码 Base64ToBytes 对应的文本并返回字节数据。
/// @param s 待处理文本。
Bytes Base64ToBytes(_In_ std::string_view s);
/// @brief 执行 RandomBytes 操作。
/// @param n 需要生成的字节数。
Bytes RandomBytes(_In_ std::size_t n);
/// @brief 将输入数据编码为 RandomHex 对应的文本形式。
/// @param n 需要生成的字节数。
std::string RandomHex(_In_ std::size_t n);

/// @brief 计算 Md5 消息摘要或认证码。
/// @param s 待处理文本。
Bytes Md5(_In_ std::string_view s);
/// @brief 计算 Sha1 消息摘要或认证码。
/// @param s 待处理文本。
Bytes Sha1(_In_ std::string_view s);
/// @brief 计算 Sha224 消息摘要或认证码。
/// @param s 待处理文本。
Bytes Sha224(_In_ std::string_view s);
/// @brief 计算 Sha256 消息摘要或认证码。
/// @param s 待处理文本。
Bytes Sha256(_In_ std::string_view s);
/// @brief 计算 Sha384 消息摘要或认证码。
/// @param s 待处理文本。
Bytes Sha384(_In_ std::string_view s);
/// @brief 计算 Sha512 消息摘要或认证码。
/// @param s 待处理文本。
Bytes Sha512(_In_ std::string_view s);
#if !defined(OPENSSL_NO_SM3)
/// @brief 计算 Sm3 消息摘要或认证码。
/// @param s 待处理文本。
Bytes Sm3(_In_ std::string_view s);
#endif

/// @brief 计算 HmacMd5 消息摘要或认证码。
/// @param data 输入字节数据。
/// @param key 加密、认证或派生密钥。
Bytes HmacMd5(_In_ std::string_view data, _In_ std::string_view key);
/// @brief 计算 HmacSha1 消息摘要或认证码。
/// @param data 输入字节数据。
/// @param key 加密、认证或派生密钥。
Bytes HmacSha1(_In_ std::string_view data, _In_ std::string_view key);
/// @brief 计算 HmacSha256 消息摘要或认证码。
/// @param data 输入字节数据。
/// @param key 加密、认证或派生密钥。
Bytes HmacSha256(_In_ std::string_view data, _In_ std::string_view key);
/// @brief 计算 HmacSha512 消息摘要或认证码。
/// @param data 输入字节数据。
/// @param key 加密、认证或派生密钥。
Bytes HmacSha512(_In_ std::string_view data, _In_ std::string_view key);

/// @brief 执行 Pbkdf2HmacSha256 密钥派生操作。
/// @param password 用于密钥派生的密码。
/// @param salt 密钥派生盐值。
/// @param iterations 密钥派生迭代次数。
/// @param outputLength 输出字节数。
Bytes Pbkdf2HmacSha256(_In_ std::string_view password, _In_ const Bytes& salt, _In_ int iterations,
                       _In_ std::size_t outputLength);
/// @brief 执行 Pbkdf2HmacSha512 密钥派生操作。
/// @param password 用于密钥派生的密码。
/// @param salt 密钥派生盐值。
/// @param iterations 密钥派生迭代次数。
/// @param outputLength 输出字节数。
Bytes Pbkdf2HmacSha512(_In_ std::string_view password, _In_ const Bytes& salt, _In_ int iterations,
                       _In_ std::size_t outputLength);
/// @brief 执行 Scrypt 密钥派生操作。
/// @param password 用于密钥派生的密码。
/// @param salt 密钥派生盐值。
/// @param N Scrypt CPU/内存成本参数。
/// @param r Scrypt 块大小参数。
/// @param p Scrypt 并行参数。
/// @param maximumMemory 允许使用的最大内存字节数。
/// @param outputLength 输出字节数。
Bytes Scrypt(_In_ std::string_view password, _In_ const Bytes& salt, _In_ std::uint64_t N, _In_ std::uint64_t r,
             _In_ std::uint64_t p, _In_ std::uint64_t maximumMemory, _In_ std::size_t outputLength);
/// @brief 执行 HkdfSha256 密钥派生操作。
/// @param ikm 输入密钥材料。
/// @param salt 密钥派生盐值。
/// @param info HKDF 上下文信息。
/// @param outputLength 输出字节数。
Bytes HkdfSha256(_In_ const Bytes& ikm, _In_ const Bytes& salt, _In_ const Bytes& info, _In_ std::size_t outputLength);

/// @brief 执行 DeriveAes256KeyFromPasswordPbkdf2Sha256 密钥派生操作。
/// @param password 用于密钥派生的密码。
/// @param salt 密钥派生盐值。
/// @param iterations 密钥派生迭代次数。
Bytes DeriveAes256KeyFromPasswordPbkdf2Sha256(_In_ std::string_view password, _In_ const Bytes& salt,
                                              _In_ int iterations = 100000);
/// @brief 执行 DeriveAes256KeyFromPasswordScrypt 密钥派生操作。
/// @param password 用于密钥派生的密码。
/// @param salt 密钥派生盐值。
/// @param N Scrypt CPU/内存成本参数。
/// @param r Scrypt 块大小参数。
/// @param p Scrypt 并行参数。
/// @param maximumMemory 允许使用的最大内存字节数。
Bytes DeriveAes256KeyFromPasswordScrypt(_In_ std::string_view password, _In_ const Bytes& salt,
                                        _In_ std::uint64_t N = 1u << 15, _In_ std::uint64_t r = 8,
                                        _In_ std::uint64_t p = 1,
                                        _In_ std::uint64_t maximumMemory = 64ull * 1024ull * 1024ull);
/// @brief 执行 DeriveAes256KeyAndIvFromPasswordPbkdf2Sha256 密钥派生操作。
/// @param password 用于密钥派生的密码。
/// @param salt 密钥派生盐值。
/// @param iterations 密钥派生迭代次数。
/// @param ivLength 需要生成的初始化向量长度。
PasswordAesKeyInfo DeriveAes256KeyAndIvFromPasswordPbkdf2Sha256(_In_ std::string_view password, _In_ const Bytes& salt,
                                                                _In_ int iterations = 100000,
                                                                _In_ std::size_t ivLength = 12);
/// @brief 使用 EncryptWithPasswordAes256GcmPbkdf2 对应算法加密输入数据。
/// @param password 用于密钥派生的密码。
/// @param plaintext 待加密明文。
/// @param salt 密钥派生盐值。
/// @param iterations 密钥派生迭代次数。
/// @param aad 附加认证数据。
/// @param iv 初始化向量。
CipherPack EncryptWithPasswordAes256GcmPbkdf2(_In_ std::string_view password, _In_ const Bytes& plaintext,
                                              _In_ const Bytes& salt, _In_ int iterations = 100000,
                                              _In_ const Bytes& aad = {}, _In_ const Bytes& iv = {});
/// @brief 使用 DecryptWithPasswordAes256GcmPbkdf2 对应算法解密输入数据；认证失败时抛出异常。
/// @param password 用于密钥派生的密码。
/// @param pack 包含初始化向量、密文和认证标签的密文包。
/// @param salt 密钥派生盐值。
/// @param iterations 密钥派生迭代次数。
/// @param aad 附加认证数据。
Bytes DecryptWithPasswordAes256GcmPbkdf2(_In_ std::string_view password, _In_ const CipherPack& pack,
                                         _In_ const Bytes& salt, _In_ int iterations = 100000,
                                         _In_ const Bytes& aad = {});

/// @brief 使用 EncryptAes128Cbc 对应算法加密输入数据。
/// @param plaintext 待加密明文。
/// @param key 加密、认证或派生密钥。
/// @param iv 初始化向量。
Bytes EncryptAes128Cbc(_In_ const Bytes& plaintext, _In_ const Bytes& key, _In_ const Bytes& iv);
/// @brief 使用 DecryptAes128Cbc 对应算法解密输入数据；认证失败时抛出异常。
/// @param ciphertext 待解密密文。
/// @param key 加密、认证或派生密钥。
/// @param iv 初始化向量。
Bytes DecryptAes128Cbc(_In_ const Bytes& ciphertext, _In_ const Bytes& key, _In_ const Bytes& iv);
/// @brief 使用 EncryptAes256Cbc 对应算法加密输入数据。
/// @param plaintext 待加密明文。
/// @param key 加密、认证或派生密钥。
/// @param iv 初始化向量。
Bytes EncryptAes256Cbc(_In_ const Bytes& plaintext, _In_ const Bytes& key, _In_ const Bytes& iv);
/// @brief 使用 DecryptAes256Cbc 对应算法解密输入数据；认证失败时抛出异常。
/// @param ciphertext 待解密密文。
/// @param key 加密、认证或派生密钥。
/// @param iv 初始化向量。
Bytes DecryptAes256Cbc(_In_ const Bytes& ciphertext, _In_ const Bytes& key, _In_ const Bytes& iv);
/// @brief 使用 EncryptAes128Ctr 对应算法加密输入数据。
/// @param plaintext 待加密明文。
/// @param key 加密、认证或派生密钥。
/// @param iv 初始化向量。
Bytes EncryptAes128Ctr(_In_ const Bytes& plaintext, _In_ const Bytes& key, _In_ const Bytes& iv);
/// @brief 使用 DecryptAes128Ctr 对应算法解密输入数据；认证失败时抛出异常。
/// @param ciphertext 待解密密文。
/// @param key 加密、认证或派生密钥。
/// @param iv 初始化向量。
Bytes DecryptAes128Ctr(_In_ const Bytes& ciphertext, _In_ const Bytes& key, _In_ const Bytes& iv);
/// @brief 使用 EncryptAes256Ctr 对应算法加密输入数据。
/// @param plaintext 待加密明文。
/// @param key 加密、认证或派生密钥。
/// @param iv 初始化向量。
Bytes EncryptAes256Ctr(_In_ const Bytes& plaintext, _In_ const Bytes& key, _In_ const Bytes& iv);
/// @brief 使用 DecryptAes256Ctr 对应算法解密输入数据；认证失败时抛出异常。
/// @param ciphertext 待解密密文。
/// @param key 加密、认证或派生密钥。
/// @param iv 初始化向量。
Bytes DecryptAes256Ctr(_In_ const Bytes& ciphertext, _In_ const Bytes& key, _In_ const Bytes& iv);
/// @brief 使用 EncryptAes128Gcm 对应算法加密输入数据。
/// @param plaintext 待加密明文。
/// @param key 加密、认证或派生密钥。
/// @param iv 初始化向量。
/// @param aad 附加认证数据。
CipherPack EncryptAes128Gcm(_In_ const Bytes& plaintext, _In_ const Bytes& key, _In_ const Bytes& iv,
                            _In_ const Bytes& aad = {});
/// @brief 使用 DecryptAes128Gcm 对应算法解密输入数据；认证失败时抛出异常。
/// @param pack 包含初始化向量、密文和认证标签的密文包。
/// @param key 加密、认证或派生密钥。
/// @param aad 附加认证数据。
Bytes DecryptAes128Gcm(_In_ const CipherPack& pack, _In_ const Bytes& key, _In_ const Bytes& aad = {});
/// @brief 使用 EncryptAes256Gcm 对应算法加密输入数据。
/// @param plaintext 待加密明文。
/// @param key 加密、认证或派生密钥。
/// @param iv 初始化向量。
/// @param aad 附加认证数据。
CipherPack EncryptAes256Gcm(_In_ const Bytes& plaintext, _In_ const Bytes& key, _In_ const Bytes& iv,
                            _In_ const Bytes& aad = {});
/// @brief 使用 DecryptAes256Gcm 对应算法解密输入数据；认证失败时抛出异常。
/// @param pack 包含初始化向量、密文和认证标签的密文包。
/// @param key 加密、认证或派生密钥。
/// @param aad 附加认证数据。
Bytes DecryptAes256Gcm(_In_ const CipherPack& pack, _In_ const Bytes& key, _In_ const Bytes& aad = {});
/// @brief 使用 EncryptAes 对应算法加密输入数据。
/// @param text 待处理文本。
/// @param password 用于密钥派生的密码。
std::string EncryptAes(_In_ std::string text, _In_ std::string password);
/// @brief 使用 DecryptAes 对应算法解密输入数据；认证失败时抛出异常。
/// @param text 待处理文本。
/// @param password 用于密钥派生的密码。
std::string DecryptAes(_In_ std::string text, _In_ std::string password);
/// @brief 使用 EncryptChaCha20Poly1305 对应算法加密输入数据。
/// @param plaintext 待加密明文。
/// @param key 加密、认证或派生密钥。
/// @param iv 初始化向量。
/// @param aad 附加认证数据。
CipherPack EncryptChaCha20Poly1305(_In_ const Bytes& plaintext, _In_ const Bytes& key, _In_ const Bytes& iv,
                                   _In_ const Bytes& aad = {});
/// @brief 使用 DecryptChaCha20Poly1305 对应算法解密输入数据；认证失败时抛出异常。
/// @param pack 包含初始化向量、密文和认证标签的密文包。
/// @param key 加密、认证或派生密钥。
/// @param aad 附加认证数据。
Bytes DecryptChaCha20Poly1305(_In_ const CipherPack& pack, _In_ const Bytes& key, _In_ const Bytes& aad = {});
#if !defined(OPENSSL_NO_SM4)
/// @brief 使用 EncryptSm4Cbc 对应算法加密输入数据。
/// @param plaintext 待加密明文。
/// @param key 加密、认证或派生密钥。
/// @param iv 初始化向量。
Bytes EncryptSm4Cbc(_In_ const Bytes& plaintext, _In_ const Bytes& key, _In_ const Bytes& iv);
/// @brief 使用 DecryptSm4Cbc 对应算法解密输入数据；认证失败时抛出异常。
/// @param ciphertext 待解密密文。
/// @param key 加密、认证或派生密钥。
/// @param iv 初始化向量。
Bytes DecryptSm4Cbc(_In_ const Bytes& ciphertext, _In_ const Bytes& key, _In_ const Bytes& iv);
/// @brief 使用 EncryptSm4Ctr 对应算法加密输入数据。
/// @param plaintext 待加密明文。
/// @param key 加密、认证或派生密钥。
/// @param iv 初始化向量。
Bytes EncryptSm4Ctr(_In_ const Bytes& plaintext, _In_ const Bytes& key, _In_ const Bytes& iv);
/// @brief 使用 DecryptSm4Ctr 对应算法解密输入数据；认证失败时抛出异常。
/// @param ciphertext 待解密密文。
/// @param key 加密、认证或派生密钥。
/// @param iv 初始化向量。
Bytes DecryptSm4Ctr(_In_ const Bytes& ciphertext, _In_ const Bytes& key, _In_ const Bytes& iv);
#endif

/// @brief 生成 GenerateRsaKeyPair 对应的密钥或随机数据。
/// @param bits RSA 密钥位数。
PemKeyPair GenerateRsaKeyPair(_In_ int bits = 2048);
/// @brief 生成 GenerateEcP256KeyPair 对应的密钥或随机数据。
PemKeyPair GenerateEcP256KeyPair();
/// @brief 生成 GenerateEd25519KeyPair 对应的密钥或随机数据。
PemKeyPair GenerateEd25519KeyPair();
/// @brief 从 PEM 私钥提取对应的 PEM 公钥。
/// @param privateKeyPem PEM 格式私钥。
std::string PublicKeyPemFromPrivateKeyPem(_In_ const std::string& privateKeyPem);
/// @brief 使用 EncryptRsaOaepSha256 对应算法加密输入数据。
/// @param publicKeyPem PEM 格式公钥。
/// @param plaintext 待加密明文。
Bytes EncryptRsaOaepSha256(_In_ const std::string& publicKeyPem, _In_ const Bytes& plaintext);
/// @brief 使用 DecryptRsaOaepSha256 对应算法解密输入数据；认证失败时抛出异常。
/// @param privateKeyPem PEM 格式私钥。
/// @param ciphertext 待解密密文。
Bytes DecryptRsaOaepSha256(_In_ const std::string& privateKeyPem, _In_ const Bytes& ciphertext);
/// @brief 使用 SignRsaPssSha256 对应算法生成数字签名。
/// @param privateKeyPem PEM 格式私钥。
/// @param data 输入字节数据。
Bytes SignRsaPssSha256(_In_ const std::string& privateKeyPem, _In_ const Bytes& data);
/// @brief 验证 VerifyRsaPssSha256 对应的数字签名，成功返回 true。
/// @param publicKeyPem PEM 格式公钥。
/// @param data 输入字节数据。
/// @param sig 待验证签名。
bool VerifyRsaPssSha256(_In_ const std::string& publicKeyPem, _In_ const Bytes& data, _In_ const Bytes& sig);
/// @brief 使用 SignRsaPkcs1V15Sha256 对应算法生成数字签名。
/// @param privateKeyPem PEM 格式私钥。
/// @param data 输入字节数据。
Bytes SignRsaPkcs1V15Sha256(_In_ const std::string& privateKeyPem, _In_ const Bytes& data);
/// @brief 验证 VerifyRsaPkcs1V15Sha256 对应的数字签名，成功返回 true。
/// @param publicKeyPem PEM 格式公钥。
/// @param data 输入字节数据。
/// @param sig 待验证签名。
bool VerifyRsaPkcs1V15Sha256(_In_ const std::string& publicKeyPem, _In_ const Bytes& data, _In_ const Bytes& sig);
/// @brief 使用 SignEcdsaP256Sha256 对应算法生成数字签名。
/// @param privateKeyPem PEM 格式私钥。
/// @param data 输入字节数据。
Bytes SignEcdsaP256Sha256(_In_ const std::string& privateKeyPem, _In_ const Bytes& data);
/// @brief 验证 VerifyEcdsaP256Sha256 对应的数字签名，成功返回 true。
/// @param publicKeyPem PEM 格式公钥。
/// @param data 输入字节数据。
/// @param sig 待验证签名。
bool VerifyEcdsaP256Sha256(_In_ const std::string& publicKeyPem, _In_ const Bytes& data, _In_ const Bytes& sig);
/// @brief 使用 SignEd25519 对应算法生成数字签名。
/// @param privateKeyPem PEM 格式私钥。
/// @param data 输入字节数据。
Bytes SignEd25519(_In_ const std::string& privateKeyPem, _In_ const Bytes& data);
/// @brief 验证 VerifyEd25519 对应的数字签名，成功返回 true。
/// @param publicKeyPem PEM 格式公钥。
/// @param data 输入字节数据。
/// @param sig 待验证签名。
bool VerifyEd25519(_In_ const std::string& publicKeyPem, _In_ const Bytes& data, _In_ const Bytes& sig);
#if !defined(OPENSSL_NO_SM2)
/// @brief 使用 SignSm2Sm3 对应算法生成数字签名。
/// @param privateKeyPem PEM 格式私钥。
/// @param data 输入字节数据。
/// @param userId SM2 用户标识。
Bytes SignSm2Sm3(_In_ const std::string& privateKeyPem, _In_ const Bytes& data,
                 _In_ std::string_view userId = "1234567812345678");
/// @brief 验证 VerifySm2Sm3 对应的数字签名，成功返回 true。
/// @param publicKeyPem PEM 格式公钥。
/// @param data 输入字节数据。
/// @param sig 待验证签名。
/// @param userId SM2 用户标识。
bool VerifySm2Sm3(_In_ const std::string& publicKeyPem, _In_ const Bytes& data, _In_ const Bytes& sig,
                  _In_ std::string_view userId = "1234567812345678");
#endif

class FileCrypto {
  public:
    /// @brief 使用 EncryptFileAes256Gcm 对应算法加密输入数据。
    /// @param inputPath 输入文件路径。
    /// @param outputPath 输出文件路径。
    /// @param key 加密、认证或派生密钥。
    /// @param iv 初始化向量。
    /// @param aad 附加认证数据。
    /// @param chunkSize 流式处理块大小，单位为字节。
    static FileEncryptResult EncryptFileAes256Gcm(_In_ const std::string& inputPath, _In_ const std::string& outputPath,
                                                  _In_ const Bytes& key, _In_ const Bytes& iv,
                                                  _In_ const Bytes& aad = {}, _In_ std::size_t chunkSize = 1024 * 1024);

    /// @brief 使用 DecryptFileAes256Gcm 对应算法解密输入数据；认证失败时抛出异常。
    /// @param inputPath 输入文件路径。
    /// @param outputPath 输出文件路径。
    /// @param key 加密、认证或派生密钥。
    /// @param iv 初始化向量。
    /// @param tag GCM 认证标签。
    /// @param aad 附加认证数据。
    /// @param chunkSize 流式处理块大小，单位为字节。
    static void DecryptFileAes256Gcm(_In_ const std::string& inputPath, _In_ const std::string& outputPath,
                                     _In_ const Bytes& key, _In_ const Bytes& iv, _In_ const Bytes& tag,
                                     _In_ const Bytes& aad = {}, _In_ std::size_t chunkSize = 1024 * 1024);

    /// @brief 使用 EncryptFileWithPasswordAes256GcmPbkdf2 对应算法加密输入数据。
    /// @param inputPath 输入文件路径。
    /// @param outputPath 输出文件路径。
    /// @param password 用于密钥派生的密码。
    /// @param salt 密钥派生盐值。
    /// @param iterations 密钥派生迭代次数。
    /// @param aad 附加认证数据。
    /// @param iv 初始化向量。
    /// @param chunkSize 流式处理块大小，单位为字节。
    static PasswordFileEncryptResult EncryptFileWithPasswordAes256GcmPbkdf2(
        _In_ const std::string& inputPath, _In_ const std::string& outputPath, _In_ std::string_view password,
        _In_ const Bytes& salt = {}, _In_ int iterations = 100000, _In_ const Bytes& aad = {},
        _In_ const Bytes& iv = {}, _In_ std::size_t chunkSize = 1024 * 1024);

    /// @brief 使用 DecryptFileWithPasswordAes256GcmPbkdf2 对应算法解密输入数据；认证失败时抛出异常。
    /// @param inputPath 输入文件路径。
    /// @param outputPath 输出文件路径。
    /// @param password 用于密钥派生的密码。
    /// @param salt 密钥派生盐值。
    /// @param iterations 密钥派生迭代次数。
    /// @param iv 初始化向量。
    /// @param tag GCM 认证标签。
    /// @param aad 附加认证数据。
    /// @param chunkSize 流式处理块大小，单位为字节。
    static void DecryptFileWithPasswordAes256GcmPbkdf2(_In_ const std::string& inputPath,
                                                       _In_ const std::string& outputPath,
                                                       _In_ std::string_view password, _In_ const Bytes& salt,
                                                       _In_ int iterations, _In_ const Bytes& iv, _In_ const Bytes& tag,
                                                       _In_ const Bytes& aad = {},
                                                       _In_ std::size_t chunkSize = 1024 * 1024);

    /// @brief 使用 EncryptFileToContainerWithPasswordAes256GcmPbkdf2 对应算法加密输入数据。
    /// @param inputPath 输入文件路径。
    /// @param containerOutputPath 加密容器输出路径。
    /// @param password 用于密钥派生的密码。
    /// @param salt 密钥派生盐值。
    /// @param iterations 密钥派生迭代次数。
    /// @param chunkSize 流式处理块大小，单位为字节。
    static void EncryptFileToContainerWithPasswordAes256GcmPbkdf2(
        _In_ const std::string& inputPath, _In_ const std::string& containerOutputPath, _In_ std::string_view password,
        _In_ const Bytes& salt = {}, _In_ int iterations = 100000, _In_ std::size_t chunkSize = 1024 * 1024);

    /// @brief 使用 DecryptFileFromContainerWithPasswordAes256GcmPbkdf2 对应算法解密输入数据；认证失败时抛出异常。
    /// @param containerInputPath 加密容器输入路径。
    /// @param outputPath 输出文件路径。
    /// @param password 用于密钥派生的密码。
    /// @param chunkSize 流式处理块大小，单位为字节。
    static void DecryptFileFromContainerWithPasswordAes256GcmPbkdf2(_In_ const std::string& containerInputPath,
                                                                    _In_ const std::string& outputPath,
                                                                    _In_ std::string_view password,
                                                                    _In_ std::size_t chunkSize = 1024 * 1024);
};

} // namespace ytpp::sys_core::encryption
