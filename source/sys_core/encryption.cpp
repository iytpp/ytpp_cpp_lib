#include "sys_core/encryption.h"

#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/core_names.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/kdf.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

namespace ytpp::sys_core::encryption {

OpenSslException::OpenSslException(const std::string& msg) : std::runtime_error(msg) {}

namespace {

const unsigned char* U8(const void* p) {
    return static_cast<const unsigned char*>(p);
}

unsigned char* U8(void* p) {
    return static_cast<unsigned char*>(p);
}

int HexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

void Ensure(bool ok, const std::string& msg) {
    if (!ok) {
        throw OpenSslException(msg);
    }
}

void EnsureKeyLength(const Bytes& key, std::size_t expected, const char* where) {
    if (key.size() != expected) {
        throw OpenSslException(std::string(where) + ": invalid key length, expected " + std::to_string(expected) + " bytes");
    }
}

void EnsureIvLength(const Bytes& iv, std::size_t expected, const char* where) {
    if (iv.size() != expected) {
        throw OpenSslException(std::string(where) + ": invalid iv length, expected " + std::to_string(expected) + " bytes");
    }
}

void EnsureNotEmptyPath(const std::string& path, const char* where) {
    if (path.empty()) {
        throw OpenSslException(std::string(where) + ": path must not be empty");
    }
}

std::uint64_t GetFileSize(std::ifstream& ifs) {
    auto current = ifs.tellg();
    ifs.seekg(0, std::ios::end);
    auto end = ifs.tellg();
    ifs.seekg(current, std::ios::beg);
    if (end < 0) {
        throw OpenSslException("GetFileSize: failed to query file size");
    }
    return static_cast<std::uint64_t>(end);
}

Bytes DigestImpl(const void* data, std::size_t len, const EVP_MD* md) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) ThrowLastError("EVP_MD_CTX_new");

    Bytes out(static_cast<std::size_t>(EVP_MD_size(md)));
    unsigned int out_len = 0;

    if (EVP_DigestInit_ex(ctx, md, nullptr) != 1 ||
        (len > 0 && EVP_DigestUpdate(ctx, data, len) != 1) ||
        EVP_DigestFinal_ex(ctx, out.data(), &out_len) != 1) {
        EVP_MD_CTX_free(ctx);
        ThrowLastError("Digest");
    }
    EVP_MD_CTX_free(ctx);
    out.resize(out_len);
    return out;
}

Bytes DigestImpl(std::string_view s, const EVP_MD* md) {
    return DigestImpl(s.data(), s.size(), md);
}

Bytes HmacImpl(const void* data, std::size_t len,
               const void* key, std::size_t key_len,
               const EVP_MD* md) {
    unsigned int out_len = EVP_MAX_MD_SIZE;
    Bytes out(out_len);
    unsigned char* ret = HMAC(md,
                              key,
                              static_cast<int>(key_len),
                              U8(data),
                              len,
                              out.data(),
                              &out_len);
    if (!ret) ThrowLastError("HMAC");
    out.resize(out_len);
    return out;
}

Bytes HmacImpl(std::string_view data, std::string_view key, const EVP_MD* md) {
    return HmacImpl(data.data(), data.size(), key.data(), key.size(), md);
}

Bytes CipherEncryptRaw(const EVP_CIPHER* cipher,
                       const Bytes& plaintext,
                       const Bytes& key,
                       const Bytes& iv,
                       bool padding = true) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) ThrowLastError("EVP_CIPHER_CTX_new");

    if (EVP_EncryptInit_ex(ctx, cipher, nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("EVP_EncryptInit_ex(phase1)");
    }
    if (EVP_CIPHER_CTX_set_padding(ctx, padding ? 1 : 0) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("EVP_CIPHER_CTX_set_padding");
    }
    if (EVP_EncryptInit_ex(ctx,
                           nullptr,
                           nullptr,
                           key.empty() ? nullptr : key.data(),
                           iv.empty() ? nullptr : iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("EVP_EncryptInit_ex(phase2)");
    }

    Bytes out(plaintext.size() + static_cast<std::size_t>(EVP_CIPHER_block_size(cipher)));
    int out_len1 = 0;
    int out_len2 = 0;
    if ((!plaintext.empty() && EVP_EncryptUpdate(ctx,
                                                 out.data(),
                                                 &out_len1,
                                                 plaintext.data(),
                                                 static_cast<int>(plaintext.size())) != 1) ||
        EVP_EncryptFinal_ex(ctx, out.data() + out_len1, &out_len2) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("CipherEncryptRaw");
    }

    EVP_CIPHER_CTX_free(ctx);
    out.resize(static_cast<std::size_t>(out_len1 + out_len2));
    return out;
}

Bytes CipherDecryptRaw(const EVP_CIPHER* cipher,
                       const Bytes& ciphertext,
                       const Bytes& key,
                       const Bytes& iv,
                       bool padding = true) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) ThrowLastError("EVP_CIPHER_CTX_new");

    if (EVP_DecryptInit_ex(ctx, cipher, nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("EVP_DecryptInit_ex(phase1)");
    }
    if (EVP_CIPHER_CTX_set_padding(ctx, padding ? 1 : 0) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("EVP_CIPHER_CTX_set_padding");
    }
    if (EVP_DecryptInit_ex(ctx,
                           nullptr,
                           nullptr,
                           key.empty() ? nullptr : key.data(),
                           iv.empty() ? nullptr : iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("EVP_DecryptInit_ex(phase2)");
    }

    Bytes out(ciphertext.size() + static_cast<std::size_t>(EVP_CIPHER_block_size(cipher)));
    int out_len1 = 0;
    int out_len2 = 0;
    if ((!ciphertext.empty() && EVP_DecryptUpdate(ctx,
                                                  out.data(),
                                                  &out_len1,
                                                  ciphertext.data(),
                                                  static_cast<int>(ciphertext.size())) != 1) ||
        EVP_DecryptFinal_ex(ctx, out.data() + out_len1, &out_len2) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("CipherDecryptRaw");
    }

    EVP_CIPHER_CTX_free(ctx);
    out.resize(static_cast<std::size_t>(out_len1 + out_len2));
    return out;
}

CipherPack AeadEncrypt(const EVP_CIPHER* cipher,
                       const Bytes& plaintext,
                       const Bytes& key,
                       const Bytes& iv,
                       const Bytes& aad,
                       int tag_len = 16) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) ThrowLastError("EVP_CIPHER_CTX_new");

    if (EVP_EncryptInit_ex(ctx, cipher, nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("AeadEncrypt/EVP_EncryptInit_ex(phase1)");
    }

    const int default_iv_len = 12;
    if (static_cast<int>(iv.size()) != default_iv_len) {
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, static_cast<int>(iv.size()), nullptr) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            ThrowLastError("AeadEncrypt/SET_IVLEN");
        }
    }

    if (EVP_EncryptInit_ex(ctx,
                           nullptr,
                           nullptr,
                           key.data(),
                           iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("AeadEncrypt/EVP_EncryptInit_ex(phase2)");
    }

    int len = 0;
    if (!aad.empty() && EVP_EncryptUpdate(ctx, nullptr, &len, aad.data(), static_cast<int>(aad.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("AeadEncrypt/AAD");
    }

    CipherPack pack;
    pack.iv = iv;
    pack.ciphertext.resize(plaintext.size());
    if (!plaintext.empty() && EVP_EncryptUpdate(ctx,
                                                pack.ciphertext.data(),
                                                &len,
                                                plaintext.data(),
                                                static_cast<int>(plaintext.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("AeadEncrypt/data");
    }
    int total = len;

    if (EVP_EncryptFinal_ex(ctx, pack.ciphertext.data() + total, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("AeadEncrypt/final");
    }
    total += len;
    pack.ciphertext.resize(static_cast<std::size_t>(total));

    pack.tag.resize(static_cast<std::size_t>(tag_len));
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, tag_len, pack.tag.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("AeadEncrypt/GET_TAG");
    }

    EVP_CIPHER_CTX_free(ctx);
    return pack;
}

Bytes AeadDecrypt(const EVP_CIPHER* cipher,
                  const Bytes& ciphertext,
                  const Bytes& key,
                  const Bytes& iv,
                  const Bytes& aad,
                  const Bytes& tag) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) ThrowLastError("EVP_CIPHER_CTX_new");

    if (EVP_DecryptInit_ex(ctx, cipher, nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("AeadDecrypt/EVP_DecryptInit_ex(phase1)");
    }

    const int default_iv_len = 12;
    if (static_cast<int>(iv.size()) != default_iv_len) {
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, static_cast<int>(iv.size()), nullptr) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            ThrowLastError("AeadDecrypt/SET_IVLEN");
        }
    }

    if (EVP_DecryptInit_ex(ctx,
                           nullptr,
                           nullptr,
                           key.data(),
                           iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("AeadDecrypt/EVP_DecryptInit_ex(phase2)");
    }

    int len = 0;
    if (!aad.empty() && EVP_DecryptUpdate(ctx, nullptr, &len, aad.data(), static_cast<int>(aad.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("AeadDecrypt/AAD");
    }

    Bytes plaintext(ciphertext.size());
    if (!ciphertext.empty() && EVP_DecryptUpdate(ctx,
                                                 plaintext.data(),
                                                 &len,
                                                 ciphertext.data(),
                                                 static_cast<int>(ciphertext.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("AeadDecrypt/data");
    }
    int total = len;

    Bytes mutable_tag = tag;
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, static_cast<int>(mutable_tag.size()), mutable_tag.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("AeadDecrypt/SET_TAG");
    }

    int final_len = 0;
    const int ret = EVP_DecryptFinal_ex(ctx, plaintext.data() + total, &final_len);
    EVP_CIPHER_CTX_free(ctx);
    if (ret != 1) {
        throw OpenSslException("AeadDecrypt: authentication failed (tag mismatch or data corrupted)");
    }
    total += final_len;
    plaintext.resize(static_cast<std::size_t>(total));
    return plaintext;
}

EVP_PKEY* LoadPublicKeyFromPem(const std::string& pem) {
    BIO* bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
    if (!bio) ThrowLastError("BIO_new_mem_buf(public key)");
    EVP_PKEY* pkey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!pkey) ThrowLastError("PEM_read_bio_PUBKEY");
    return pkey;
}

EVP_PKEY* LoadPrivateKeyFromPem(const std::string& pem, const char* password = nullptr) {
    BIO* bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
    if (!bio) ThrowLastError("BIO_new_mem_buf(private key)");
    EVP_PKEY* pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, const_cast<char*>(password));
    BIO_free(bio);
    if (!pkey) ThrowLastError("PEM_read_bio_PrivateKey");
    return pkey;
}

std::string PemFromPrivateKey(EVP_PKEY* pkey) {
    BIO* bio = BIO_new(BIO_s_mem());
    if (!bio) ThrowLastError("BIO_new(private PEM)");
    if (PEM_write_bio_PrivateKey(bio, pkey, nullptr, nullptr, 0, nullptr, nullptr) != 1) {
        BIO_free(bio);
        ThrowLastError("PEM_write_bio_PrivateKey");
    }
    BUF_MEM* ptr = nullptr;
    BIO_get_mem_ptr(bio, &ptr);
    std::string pem(ptr && ptr->data ? ptr->data : "", ptr ? ptr->length : 0);
    BIO_free(bio);
    return pem;
}

std::string PemFromPublicKey(EVP_PKEY* pkey) {
    BIO* bio = BIO_new(BIO_s_mem());
    if (!bio) ThrowLastError("BIO_new(public PEM)");
    if (PEM_write_bio_PUBKEY(bio, pkey) != 1) {
        BIO_free(bio);
        ThrowLastError("PEM_write_bio_PUBKEY");
    }
    BUF_MEM* ptr = nullptr;
    BIO_get_mem_ptr(bio, &ptr);
    std::string pem(ptr && ptr->data ? ptr->data : "", ptr ? ptr->length : 0);
    BIO_free(bio);
    return pem;
}

Bytes PKeyEncrypt(EVP_PKEY* pkey, const Bytes& plaintext,
                  int rsa_padding = RSA_PKCS1_OAEP_PADDING,
                  const EVP_MD* oaep_md = EVP_sha256()) {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(pkey, nullptr);
    if (!ctx) ThrowLastError("EVP_PKEY_CTX_new(encrypt)");

    if (EVP_PKEY_encrypt_init(ctx) != 1) {
        EVP_PKEY_CTX_free(ctx);
        ThrowLastError("EVP_PKEY_encrypt_init");
    }
    if (EVP_PKEY_base_id(pkey) == EVP_PKEY_RSA) {
        if (EVP_PKEY_CTX_set_rsa_padding(ctx, rsa_padding) != 1) {
            EVP_PKEY_CTX_free(ctx);
            ThrowLastError("EVP_PKEY_CTX_set_rsa_padding");
        }
        if (rsa_padding == RSA_PKCS1_OAEP_PADDING) {
            if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx, oaep_md) != 1 ||
                EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, oaep_md) != 1) {
                EVP_PKEY_CTX_free(ctx);
                ThrowLastError("EVP_PKEY_CTX_set_rsa_oaep_md/mgf1");
            }
        }
    }

    std::size_t out_len = 0;
    if (EVP_PKEY_encrypt(ctx, nullptr, &out_len,
                         plaintext.empty() ? nullptr : plaintext.data(),
                         plaintext.size()) != 1) {
        EVP_PKEY_CTX_free(ctx);
        ThrowLastError("EVP_PKEY_encrypt(size)");
    }

    Bytes out(out_len);
    if (EVP_PKEY_encrypt(ctx, out.data(), &out_len,
                         plaintext.empty() ? nullptr : plaintext.data(),
                         plaintext.size()) != 1) {
        EVP_PKEY_CTX_free(ctx);
        ThrowLastError("EVP_PKEY_encrypt");
    }
    EVP_PKEY_CTX_free(ctx);
    out.resize(out_len);
    return out;
}

Bytes PKeyDecrypt(EVP_PKEY* pkey, const Bytes& ciphertext,
                  int rsa_padding = RSA_PKCS1_OAEP_PADDING,
                  const EVP_MD* oaep_md = EVP_sha256()) {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(pkey, nullptr);
    if (!ctx) ThrowLastError("EVP_PKEY_CTX_new(decrypt)");

    if (EVP_PKEY_decrypt_init(ctx) != 1) {
        EVP_PKEY_CTX_free(ctx);
        ThrowLastError("EVP_PKEY_decrypt_init");
    }
    if (EVP_PKEY_base_id(pkey) == EVP_PKEY_RSA) {
        if (EVP_PKEY_CTX_set_rsa_padding(ctx, rsa_padding) != 1) {
            EVP_PKEY_CTX_free(ctx);
            ThrowLastError("EVP_PKEY_CTX_set_rsa_padding");
        }
        if (rsa_padding == RSA_PKCS1_OAEP_PADDING) {
            if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx, oaep_md) != 1 ||
                EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, oaep_md) != 1) {
                EVP_PKEY_CTX_free(ctx);
                ThrowLastError("EVP_PKEY_CTX_set_rsa_oaep_md/mgf1");
            }
        }
    }

    std::size_t out_len = 0;
    if (EVP_PKEY_decrypt(ctx, nullptr, &out_len,
                         ciphertext.empty() ? nullptr : ciphertext.data(),
                         ciphertext.size()) != 1) {
        EVP_PKEY_CTX_free(ctx);
        ThrowLastError("EVP_PKEY_decrypt(size)");
    }

    Bytes out(out_len);
    if (EVP_PKEY_decrypt(ctx, out.data(), &out_len,
                         ciphertext.empty() ? nullptr : ciphertext.data(),
                         ciphertext.size()) != 1) {
        EVP_PKEY_CTX_free(ctx);
        ThrowLastError("EVP_PKEY_decrypt");
    }
    EVP_PKEY_CTX_free(ctx);
    out.resize(out_len);
    return out;
}

Bytes DigestSign(EVP_PKEY* pkey,
                 const Bytes& data,
                 const EVP_MD* md,
                 int rsa_padding = 0,
                 int rsa_pss_saltlen = -1) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) ThrowLastError("EVP_MD_CTX_new(sign)");

    EVP_PKEY_CTX* pctx = nullptr;
    if (EVP_DigestSignInit(ctx, &pctx, md, nullptr, pkey) != 1) {
        EVP_MD_CTX_free(ctx);
        ThrowLastError("EVP_DigestSignInit");
    }

    if (EVP_PKEY_base_id(pkey) == EVP_PKEY_RSA && rsa_padding != 0) {
        if (EVP_PKEY_CTX_set_rsa_padding(pctx, rsa_padding) != 1) {
            EVP_MD_CTX_free(ctx);
            ThrowLastError("EVP_PKEY_CTX_set_rsa_padding(sign)");
        }
        if (rsa_padding == RSA_PKCS1_PSS_PADDING) {
            if (EVP_PKEY_CTX_set_rsa_mgf1_md(pctx, md) != 1 ||
                EVP_PKEY_CTX_set_rsa_pss_saltlen(pctx, rsa_pss_saltlen) != 1) {
                EVP_MD_CTX_free(ctx);
                ThrowLastError("EVP_PKEY_CTX_set_rsa_pss params");
            }
        }
    }

    if ((!data.empty() && EVP_DigestSignUpdate(ctx, data.data(), data.size()) != 1)) {
        EVP_MD_CTX_free(ctx);
        ThrowLastError("EVP_DigestSignUpdate");
    }

    std::size_t sig_len = 0;
    if (EVP_DigestSignFinal(ctx, nullptr, &sig_len) != 1) {
        EVP_MD_CTX_free(ctx);
        ThrowLastError("EVP_DigestSignFinal(size)");
    }

    Bytes sig(sig_len);
    if (EVP_DigestSignFinal(ctx, sig.data(), &sig_len) != 1) {
        EVP_MD_CTX_free(ctx);
        ThrowLastError("EVP_DigestSignFinal");
    }
    EVP_MD_CTX_free(ctx);
    sig.resize(sig_len);
    return sig;
}

bool DigestVerify(EVP_PKEY* pkey,
                  const Bytes& data,
                  const Bytes& sig,
                  const EVP_MD* md,
                  int rsa_padding = 0,
                  int rsa_pss_saltlen = -1) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) ThrowLastError("EVP_MD_CTX_new(verify)");

    EVP_PKEY_CTX* pctx = nullptr;
    if (EVP_DigestVerifyInit(ctx, &pctx, md, nullptr, pkey) != 1) {
        EVP_MD_CTX_free(ctx);
        ThrowLastError("EVP_DigestVerifyInit");
    }

    if (EVP_PKEY_base_id(pkey) == EVP_PKEY_RSA && rsa_padding != 0) {
        if (EVP_PKEY_CTX_set_rsa_padding(pctx, rsa_padding) != 1) {
            EVP_MD_CTX_free(ctx);
            ThrowLastError("EVP_PKEY_CTX_set_rsa_padding(verify)");
        }
        if (rsa_padding == RSA_PKCS1_PSS_PADDING) {
            if (EVP_PKEY_CTX_set_rsa_mgf1_md(pctx, md) != 1 ||
                EVP_PKEY_CTX_set_rsa_pss_saltlen(pctx, rsa_pss_saltlen) != 1) {
                EVP_MD_CTX_free(ctx);
                ThrowLastError("EVP_PKEY_CTX_set_rsa_pss verify params");
            }
        }
    }

    if ((!data.empty() && EVP_DigestVerifyUpdate(ctx, data.data(), data.size()) != 1)) {
        EVP_MD_CTX_free(ctx);
        ThrowLastError("EVP_DigestVerifyUpdate");
    }

    int ret = EVP_DigestVerifyFinal(ctx,
                                    sig.empty() ? nullptr : const_cast<unsigned char*>(sig.data()),
                                    sig.size());
    EVP_MD_CTX_free(ctx);
    if (ret == 1) return true;
    if (ret == 0) return false;
    ThrowLastError("EVP_DigestVerifyFinal");
    return false;
}

#if !defined(OPENSSL_NO_SM2)
void PrepareSM2ForSignVerify(EVP_PKEY* pkey, EVP_MD_CTX* mctx, EVP_PKEY_CTX** out_pctx,
                             const void* id, std::size_t id_len) {
#if OPENSSL_VERSION_NUMBER < 0x30000000L
    if (EVP_PKEY_base_id(pkey) == EVP_PKEY_EC) {
        if (EVP_PKEY_set_alias_type(pkey, EVP_PKEY_SM2) != 1) {
            ThrowLastError("EVP_PKEY_set_alias_type(SM2)");
        }
    }
#endif
    EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new(pkey, nullptr);
    if (!pctx) ThrowLastError("EVP_PKEY_CTX_new(SM2)");
    if (EVP_PKEY_CTX_set1_id(pctx, const_cast<void*>(id), static_cast<int>(id_len)) != 1) {
        EVP_PKEY_CTX_free(pctx);
        ThrowLastError("EVP_PKEY_CTX_set1_id(SM2)");
    }
    EVP_MD_CTX_set_pkey_ctx(mctx, pctx);
    *out_pctx = pctx;
}
#endif

FileEncryptResult EncryptFileAes256GcmCore(const std::string& input_path,
                                           const std::string& output_path,
                                           const Bytes& key,
                                           const Bytes& iv,
                                           const Bytes& aad,
                                           std::size_t chunk_size) {
    EnsureNotEmptyPath(input_path, "EncryptFileAes256GcmCore");
    EnsureNotEmptyPath(output_path, "EncryptFileAes256GcmCore");
    EnsureKeyLength(key, 32, "EncryptFileAes256GcmCore");
    Ensure(!iv.empty(), "EncryptFileAes256GcmCore: iv must not be empty");
    Ensure(chunk_size > 0, "EncryptFileAes256GcmCore: chunk_size must be > 0");

    std::ifstream ifs(input_path, std::ios::binary);
    if (!ifs) throw OpenSslException("EncryptFileAes256GcmCore: cannot open input file: " + input_path);
    std::ofstream ofs(output_path, std::ios::binary | std::ios::trunc);
    if (!ofs) throw OpenSslException("EncryptFileAes256GcmCore: cannot open output file: " + output_path);

    FileEncryptResult result;
    result.iv = iv;
    result.plain_size = GetFileSize(ifs);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) ThrowLastError("EVP_CIPHER_CTX_new(file encrypt)");

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("EncryptFileAes256GcmCore/EVP_EncryptInit_ex(phase1)");
    }
    if (iv.size() != 12) {
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, static_cast<int>(iv.size()), nullptr) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            ThrowLastError("EncryptFileAes256GcmCore/SET_IVLEN");
        }
    }
    if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("EncryptFileAes256GcmCore/EVP_EncryptInit_ex(phase2)");
    }

    int len = 0;
    if (!aad.empty() && EVP_EncryptUpdate(ctx, nullptr, &len, aad.data(), static_cast<int>(aad.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("EncryptFileAes256GcmCore/AAD");
    }

    Bytes in_buf(chunk_size);
    Bytes out_buf(chunk_size + 32);

    while (ifs) {
        ifs.read(reinterpret_cast<char*>(in_buf.data()), static_cast<std::streamsize>(in_buf.size()));
        std::streamsize got = ifs.gcount();
        if (got <= 0) break;

        if (EVP_EncryptUpdate(ctx,
                              out_buf.data(),
                              &len,
                              in_buf.data(),
                              static_cast<int>(got)) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            ThrowLastError("EncryptFileAes256GcmCore/EncryptUpdate");
        }
        if (len > 0) {
            ofs.write(reinterpret_cast<const char*>(out_buf.data()), len);
            if (!ofs) {
                EVP_CIPHER_CTX_free(ctx);
                throw OpenSslException("EncryptFileAes256GcmCore: failed to write output file");
            }
            result.cipher_size += static_cast<std::uint64_t>(len);
        }
    }

    if (EVP_EncryptFinal_ex(ctx, out_buf.data(), &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("EncryptFileAes256GcmCore/EncryptFinal");
    }
    if (len > 0) {
        ofs.write(reinterpret_cast<const char*>(out_buf.data()), len);
        if (!ofs) {
            EVP_CIPHER_CTX_free(ctx);
            throw OpenSslException("EncryptFileAes256GcmCore: failed to write final block");
        }
        result.cipher_size += static_cast<std::uint64_t>(len);
    }

    result.tag.resize(16);
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, static_cast<int>(result.tag.size()), result.tag.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("EncryptFileAes256GcmCore/GET_TAG");
    }

    EVP_CIPHER_CTX_free(ctx);
    return result;
}

void DecryptFileAes256GcmCore(std::ifstream& ifs,
                              std::uint64_t cipher_size,
                              const std::string& output_path,
                              const Bytes& key,
                              const Bytes& iv,
                              const Bytes& tag,
                              const Bytes& aad,
                              std::size_t chunk_size) {
    EnsureNotEmptyPath(output_path, "DecryptFileAes256GcmCore");
    EnsureKeyLength(key, 32, "DecryptFileAes256GcmCore");
    Ensure(!iv.empty(), "DecryptFileAes256GcmCore: iv must not be empty");
    Ensure(!tag.empty(), "DecryptFileAes256GcmCore: tag must not be empty");
    Ensure(chunk_size > 0, "DecryptFileAes256GcmCore: chunk_size must be > 0");

    std::ofstream ofs(output_path, std::ios::binary | std::ios::trunc);
    if (!ofs) throw OpenSslException("DecryptFileAes256GcmCore: cannot open output file: " + output_path);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) ThrowLastError("EVP_CIPHER_CTX_new(file decrypt)");

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("DecryptFileAes256GcmCore/EVP_DecryptInit_ex(phase1)");
    }
    if (iv.size() != 12) {
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, static_cast<int>(iv.size()), nullptr) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            ThrowLastError("DecryptFileAes256GcmCore/SET_IVLEN");
        }
    }
    if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("DecryptFileAes256GcmCore/EVP_DecryptInit_ex(phase2)");
    }

    int len = 0;
    if (!aad.empty() && EVP_DecryptUpdate(ctx, nullptr, &len, aad.data(), static_cast<int>(aad.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("DecryptFileAes256GcmCore/AAD");
    }

    Bytes in_buf(chunk_size);
    Bytes out_buf(chunk_size + 32);
    std::uint64_t remaining = cipher_size;
    while (remaining > 0) {
        std::size_t want = static_cast<std::size_t>(std::min<std::uint64_t>(remaining, in_buf.size()));
        ifs.read(reinterpret_cast<char*>(in_buf.data()), static_cast<std::streamsize>(want));
        std::streamsize got = ifs.gcount();
        if (got <= 0) {
            EVP_CIPHER_CTX_free(ctx);
            throw OpenSslException("DecryptFileAes256GcmCore: unexpected end of encrypted file");
        }

        if (EVP_DecryptUpdate(ctx,
                              out_buf.data(),
                              &len,
                              in_buf.data(),
                              static_cast<int>(got)) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            ThrowLastError("DecryptFileAes256GcmCore/DecryptUpdate");
        }
        if (len > 0) {
            ofs.write(reinterpret_cast<const char*>(out_buf.data()), len);
            if (!ofs) {
                EVP_CIPHER_CTX_free(ctx);
                throw OpenSslException("DecryptFileAes256GcmCore: failed to write output file");
            }
        }
        remaining -= static_cast<std::uint64_t>(got);
    }

    Bytes mutable_tag = tag;
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, static_cast<int>(mutable_tag.size()), mutable_tag.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("DecryptFileAes256GcmCore/SET_TAG");
    }

    if (EVP_DecryptFinal_ex(ctx, out_buf.data(), &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw OpenSslException("DecryptFileAes256GcmCore: authentication failed (password/key/iv/tag mismatch or file corrupted)");
    }
    if (len > 0) {
        ofs.write(reinterpret_cast<const char*>(out_buf.data()), len);
        if (!ofs) {
            EVP_CIPHER_CTX_free(ctx);
            throw OpenSslException("DecryptFileAes256GcmCore: failed to write final block");
        }
    }

    EVP_CIPHER_CTX_free(ctx);
}

struct PasswordContainerHeaderV1 {
    char magic[8];
    std::uint32_t version;
    std::uint32_t kdf_id;
    std::uint32_t cipher_id;
    std::uint32_t iterations;
    std::uint32_t salt_len;
    std::uint32_t iv_len;
    std::uint32_t tag_len;
    std::uint64_t plain_size;
};

constexpr char kContainerMagic[8] = {'Y', 'T', 'P', 'P', 'E', 'N', 'C', '1'};
constexpr std::uint32_t kContainerVersion = 1;
constexpr std::uint32_t kContainerKdfPbkdf2Sha256 = 1;
constexpr std::uint32_t kContainerCipherAes256Gcm = 1;

void WriteContainerHeader(std::ofstream& ofs, const PasswordContainerHeaderV1& header) {
    ofs.write(reinterpret_cast<const char*>(&header), sizeof(header));
    if (!ofs) throw OpenSslException("WriteContainerHeader: failed");
}

PasswordContainerHeaderV1 ReadContainerHeader(std::ifstream& ifs) {
    PasswordContainerHeaderV1 header{};
    ifs.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (ifs.gcount() != static_cast<std::streamsize>(sizeof(header))) {
        throw OpenSslException("ReadContainerHeader: file is too short or invalid");
    }
    if (std::memcmp(header.magic, kContainerMagic, sizeof(kContainerMagic)) != 0) {
        throw OpenSslException("ReadContainerHeader: invalid container magic");
    }
    if (header.version != kContainerVersion) {
        throw OpenSslException("ReadContainerHeader: unsupported container version");
    }
    if (header.kdf_id != kContainerKdfPbkdf2Sha256 || header.cipher_id != kContainerCipherAes256Gcm) {
        throw OpenSslException("ReadContainerHeader: unsupported container algorithm");
    }
    if (header.salt_len == 0 || header.iv_len == 0 || header.tag_len == 0) {
        throw OpenSslException("ReadContainerHeader: invalid salt/iv/tag length in container");
    }
    return header;
}

} // namespace

std::string GetOpenSslErrors() {
    std::string out;
    unsigned long err = 0;
    char buf[256] = {0};
    while ((err = ERR_get_error()) != 0) {
        ERR_error_string_n(err, buf, sizeof(buf));
        if (!out.empty()) out += " | ";
        out += buf;
    }
    return out.empty() ? "unknown OpenSSL error" : out;
}

[[noreturn]] void ThrowLastError(const std::string& where) {
    throw OpenSslException(where + ": " + GetOpenSslErrors());
}

Bytes ToBytes(std::string_view s) {
    return Bytes(s.begin(), s.end());
}

std::string ToString(const Bytes& data) {
    return std::string(reinterpret_cast<const char*>(data.data()), data.size());
}

std::string HexEncode(const Bytes& data, bool upper) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    if (upper) oss.setf(std::ios::uppercase);
    for (unsigned char c : data) {
        oss << std::setw(2) << static_cast<int>(c);
    }
    return oss.str();
}

Bytes HexDecode(std::string_view hex) {
    std::string filtered;
    filtered.reserve(hex.size());
    for (char c : hex) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            filtered.push_back(c);
        }
    }
    if (filtered.size() % 2 != 0) {
        throw OpenSslException("HexDecode: hex string length must be even");
    }
    Bytes out(filtered.size() / 2);
    for (std::size_t i = 0; i < filtered.size(); i += 2) {
        int hi = HexValue(filtered[i]);
        int lo = HexValue(filtered[i + 1]);
        if (hi < 0 || lo < 0) {
            throw OpenSslException("HexDecode: invalid hex character");
        }
        out[i / 2] = static_cast<unsigned char>((hi << 4) | lo);
    }
    return out;
}

std::string Base64Encode(const Bytes& data, bool no_new_line) {
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* mem = BIO_new(BIO_s_mem());
    if (!b64 || !mem) {
        if (b64) BIO_free(b64);
        if (mem) BIO_free(mem);
        ThrowLastError("Base64Encode/BIO_new");
    }
    if (no_new_line) {
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    }
    BIO_push(b64, mem);

    if (BIO_write(b64, data.data(), static_cast<int>(data.size())) <= 0) {
        BIO_free_all(b64);
        ThrowLastError("Base64Encode/BIO_write");
    }
    if (BIO_flush(b64) != 1) {
        BIO_free_all(b64);
        ThrowLastError("Base64Encode/BIO_flush");
    }

    BUF_MEM* ptr = nullptr;
    BIO_get_mem_ptr(b64, &ptr);
    std::string out(ptr && ptr->data ? ptr->data : "", ptr ? ptr->length : 0);
    BIO_free_all(b64);
    return out;
}

Bytes Base64Decode(std::string_view base64, bool no_new_line) {
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* mem = BIO_new_mem_buf(base64.data(), static_cast<int>(base64.size()));
    if (!b64 || !mem) {
        if (b64) BIO_free(b64);
        if (mem) BIO_free(mem);
        ThrowLastError("Base64Decode/BIO_new");
    }
    if (no_new_line) {
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    }
    BIO_push(b64, mem);

    Bytes out((base64.size() * 3) / 4 + 4);
    int n = BIO_read(b64, out.data(), static_cast<int>(out.size()));
    BIO_free_all(b64);
    if (n < 0) {
        ThrowLastError("Base64Decode/BIO_read");
    }
    out.resize(static_cast<std::size_t>(n));
    return out;
}

std::string BytesToBase64(const Bytes& data) {
    return Base64Encode(data, true);
}

Bytes Base64ToBytes(std::string_view s) {
    return Base64Decode(s, true);
}

Bytes RandomBytes(std::size_t n) {
    Bytes out(n);
    if (n > 0 && RAND_bytes(out.data(), static_cast<int>(n)) != 1) {
        ThrowLastError("RAND_bytes");
    }
    return out;
}

std::string RandomHex(std::size_t n) {
    return HexEncode(RandomBytes(n));
}

Bytes MD5(std::string_view s) { return DigestImpl(s, EVP_md5()); }
Bytes SHA1(std::string_view s) { return DigestImpl(s, EVP_sha1()); }
Bytes SHA224(std::string_view s) { return DigestImpl(s, EVP_sha224()); }
Bytes SHA256(std::string_view s) { return DigestImpl(s, EVP_sha256()); }
Bytes SHA384(std::string_view s) { return DigestImpl(s, EVP_sha384()); }
Bytes SHA512(std::string_view s) { return DigestImpl(s, EVP_sha512()); }
#if !defined(OPENSSL_NO_SM3)
Bytes SM3(std::string_view s) { return DigestImpl(s, EVP_sm3()); }
#endif

Bytes HMAC_MD5(std::string_view data, std::string_view key) { return HmacImpl(data, key, EVP_md5()); }
Bytes HMAC_SHA1(std::string_view data, std::string_view key) { return HmacImpl(data, key, EVP_sha1()); }
Bytes HMAC_SHA256(std::string_view data, std::string_view key) { return HmacImpl(data, key, EVP_sha256()); }
Bytes HMAC_SHA512(std::string_view data, std::string_view key) { return HmacImpl(data, key, EVP_sha512()); }

Bytes PBKDF2_HMAC_SHA256(std::string_view password,
                         const Bytes& salt,
                         int iterations,
                         std::size_t out_len) {
    Bytes out(out_len);
    if (PKCS5_PBKDF2_HMAC(password.data(),
                          static_cast<int>(password.size()),
                          salt.empty() ? nullptr : salt.data(),
                          static_cast<int>(salt.size()),
                          iterations,
                          EVP_sha256(),
                          static_cast<int>(out_len),
                          out.data()) != 1) {
        ThrowLastError("PKCS5_PBKDF2_HMAC");
    }
    return out;
}

Bytes PBKDF2_HMAC_SHA512(std::string_view password,
                         const Bytes& salt,
                         int iterations,
                         std::size_t out_len) {
    Bytes out(out_len);
    if (PKCS5_PBKDF2_HMAC(password.data(),
                          static_cast<int>(password.size()),
                          salt.empty() ? nullptr : salt.data(),
                          static_cast<int>(salt.size()),
                          iterations,
                          EVP_sha512(),
                          static_cast<int>(out_len),
                          out.data()) != 1) {
        ThrowLastError("PKCS5_PBKDF2_HMAC(SHA512)");
    }
    return out;
}

Bytes Scrypt(std::string_view password,
             const Bytes& salt,
             std::uint64_t N,
             std::uint64_t r,
             std::uint64_t p,
             std::uint64_t max_mem,
             std::size_t out_len) {
    Bytes out(out_len);
    if (EVP_PBE_scrypt(password.data(),
                       password.size(),
                       salt.empty() ? nullptr : salt.data(),
                       salt.size(),
                       N, r, p,
                       max_mem,
                       out.data(),
                       out_len) != 1) {
        ThrowLastError("EVP_PBE_scrypt");
    }
    return out;
}

Bytes HKDF_SHA256(const Bytes& ikm,
                  const Bytes& salt,
                  const Bytes& info,
                  std::size_t out_len) {
    EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);
    if (!pctx) ThrowLastError("EVP_PKEY_CTX_new_id(HKDF)");

    Bytes out(out_len);
    if (EVP_PKEY_derive_init(pctx) != 1 ||
        EVP_PKEY_CTX_set_hkdf_md(pctx, EVP_sha256()) != 1 ||
        EVP_PKEY_CTX_set1_hkdf_salt(pctx,
                                    salt.empty() ? nullptr : salt.data(),
                                    static_cast<int>(salt.size())) != 1 ||
        EVP_PKEY_CTX_set1_hkdf_key(pctx,
                                   ikm.empty() ? nullptr : ikm.data(),
                                   static_cast<int>(ikm.size())) != 1 ||
        (!info.empty() && EVP_PKEY_CTX_add1_hkdf_info(pctx,
                                                      info.data(),
                                                      static_cast<int>(info.size())) != 1)) {
        EVP_PKEY_CTX_free(pctx);
        ThrowLastError("HKDF setup");
    }

    std::size_t len = out_len;
    if (EVP_PKEY_derive(pctx, out.data(), &len) != 1) {
        EVP_PKEY_CTX_free(pctx);
        ThrowLastError("EVP_PKEY_derive(HKDF)");
    }
    EVP_PKEY_CTX_free(pctx);
    out.resize(len);
    return out;
}

Bytes DeriveAes256KeyFromPassword_PBKDF2_SHA256(std::string_view password,
                                                const Bytes& salt,
                                                int iterations) {
    return PBKDF2_HMAC_SHA256(password, salt, iterations, 32);
}

Bytes DeriveAes256KeyFromPassword_Scrypt(std::string_view password,
                                         const Bytes& salt,
                                         std::uint64_t N,
                                         std::uint64_t r,
                                         std::uint64_t p,
                                         std::uint64_t max_mem) {
    return Scrypt(password, salt, N, r, p, max_mem, 32);
}

PasswordAesKeyInfo DeriveAes256KeyAndIvFromPassword_PBKDF2_SHA256(std::string_view password,
                                                                  const Bytes& salt,
                                                                  int iterations,
                                                                  std::size_t iv_len) {
    PasswordAesKeyInfo info;
    info.salt = salt;
    Bytes derived = PBKDF2_HMAC_SHA256(password, salt, iterations, 32 + iv_len);
    info.key.assign(derived.begin(), derived.begin() + 32);
    info.iv.assign(derived.begin() + 32, derived.end());
    return info;
}

CipherPack PasswordEncrypt_AES256_GCM_PBKDF2(std::string_view password,
                                             const Bytes& plaintext,
                                             const Bytes& salt,
                                             int iterations,
                                             const Bytes& aad,
                                             const Bytes& iv) {
    Bytes key = DeriveAes256KeyFromPassword_PBKDF2_SHA256(password, salt, iterations);
    Bytes actual_iv = iv.empty() ? RandomBytes(12) : iv;
    Ensure(!actual_iv.empty(), "PasswordEncrypt_AES256_GCM_PBKDF2: iv must not be empty");
    return AES_256_GCM_Encrypt(plaintext, key, actual_iv, aad);
}

Bytes PasswordDecrypt_AES256_GCM_PBKDF2(std::string_view password,
                                        const CipherPack& pack,
                                        const Bytes& salt,
                                        int iterations,
                                        const Bytes& aad) {
    Bytes key = DeriveAes256KeyFromPassword_PBKDF2_SHA256(password, salt, iterations);
    return AES_256_GCM_Decrypt(pack, key, aad);
}

Bytes AES_128_CBC_Encrypt(const Bytes& plaintext, const Bytes& key, const Bytes& iv) {
    EnsureKeyLength(key, 16, "AES_128_CBC_Encrypt");
    EnsureIvLength(iv, 16, "AES_128_CBC_Encrypt");
    return CipherEncryptRaw(EVP_aes_128_cbc(), plaintext, key, iv, true);
}

Bytes AES_128_CBC_Decrypt(const Bytes& ciphertext, const Bytes& key, const Bytes& iv) {
    EnsureKeyLength(key, 16, "AES_128_CBC_Decrypt");
    EnsureIvLength(iv, 16, "AES_128_CBC_Decrypt");
    return CipherDecryptRaw(EVP_aes_128_cbc(), ciphertext, key, iv, true);
}

Bytes AES_256_CBC_Encrypt(const Bytes& plaintext, const Bytes& key, const Bytes& iv) {
    EnsureKeyLength(key, 32, "AES_256_CBC_Encrypt");
    EnsureIvLength(iv, 16, "AES_256_CBC_Encrypt");
    return CipherEncryptRaw(EVP_aes_256_cbc(), plaintext, key, iv, true);
}

Bytes AES_256_CBC_Decrypt(const Bytes& ciphertext, const Bytes& key, const Bytes& iv) {
    EnsureKeyLength(key, 32, "AES_256_CBC_Decrypt");
    EnsureIvLength(iv, 16, "AES_256_CBC_Decrypt");
    return CipherDecryptRaw(EVP_aes_256_cbc(), ciphertext, key, iv, true);
}

Bytes AES_128_CTR_Encrypt(const Bytes& plaintext, const Bytes& key, const Bytes& iv) {
    EnsureKeyLength(key, 16, "AES_128_CTR_Encrypt");
    EnsureIvLength(iv, 16, "AES_128_CTR_Encrypt");
    return CipherEncryptRaw(EVP_aes_128_ctr(), plaintext, key, iv, false);
}

Bytes AES_128_CTR_Decrypt(const Bytes& ciphertext, const Bytes& key, const Bytes& iv) {
    EnsureKeyLength(key, 16, "AES_128_CTR_Decrypt");
    EnsureIvLength(iv, 16, "AES_128_CTR_Decrypt");
    return CipherDecryptRaw(EVP_aes_128_ctr(), ciphertext, key, iv, false);
}

Bytes AES_256_CTR_Encrypt(const Bytes& plaintext, const Bytes& key, const Bytes& iv) {
    EnsureKeyLength(key, 32, "AES_256_CTR_Encrypt");
    EnsureIvLength(iv, 16, "AES_256_CTR_Encrypt");
    return CipherEncryptRaw(EVP_aes_256_ctr(), plaintext, key, iv, false);
}

Bytes AES_256_CTR_Decrypt(const Bytes& ciphertext, const Bytes& key, const Bytes& iv) {
    EnsureKeyLength(key, 32, "AES_256_CTR_Decrypt");
    EnsureIvLength(iv, 16, "AES_256_CTR_Decrypt");
    return CipherDecryptRaw(EVP_aes_256_ctr(), ciphertext, key, iv, false);
}

CipherPack AES_128_GCM_Encrypt(const Bytes& plaintext, const Bytes& key, const Bytes& iv, const Bytes& aad) {
    EnsureKeyLength(key, 16, "AES_128_GCM_Encrypt");
    Ensure(!iv.empty(), "AES_128_GCM_Encrypt: iv must not be empty");
    return AeadEncrypt(EVP_aes_128_gcm(), plaintext, key, iv, aad, 16);
}

Bytes AES_128_GCM_Decrypt(const CipherPack& pack, const Bytes& key, const Bytes& aad) {
    EnsureKeyLength(key, 16, "AES_128_GCM_Decrypt");
    Ensure(!pack.iv.empty(), "AES_128_GCM_Decrypt: iv must not be empty");
    Ensure(!pack.tag.empty(), "AES_128_GCM_Decrypt: tag must not be empty");
    return AeadDecrypt(EVP_aes_128_gcm(), pack.ciphertext, key, pack.iv, aad, pack.tag);
}

CipherPack AES_256_GCM_Encrypt(const Bytes& plaintext, const Bytes& key, const Bytes& iv, const Bytes& aad) {
    EnsureKeyLength(key, 32, "AES_256_GCM_Encrypt");
    Ensure(!iv.empty(), "AES_256_GCM_Encrypt: iv must not be empty");
    return AeadEncrypt(EVP_aes_256_gcm(), plaintext, key, iv, aad, 16);
}

Bytes AES_256_GCM_Decrypt(const CipherPack& pack, const Bytes& key, const Bytes& aad) {
    EnsureKeyLength(key, 32, "AES_256_GCM_Decrypt");
    Ensure(!pack.iv.empty(), "AES_256_GCM_Decrypt: iv must not be empty");
    Ensure(!pack.tag.empty(), "AES_256_GCM_Decrypt: tag must not be empty");
    return AeadDecrypt(EVP_aes_256_gcm(), pack.ciphertext, key, pack.iv, aad, pack.tag);
}

CipherPack ChaCha20_Poly1305_Encrypt(const Bytes& plaintext, const Bytes& key, const Bytes& iv, const Bytes& aad) {
    EnsureKeyLength(key, 32, "ChaCha20_Poly1305_Encrypt");
    Ensure(!iv.empty(), "ChaCha20_Poly1305_Encrypt: iv must not be empty");
    return AeadEncrypt(EVP_chacha20_poly1305(), plaintext, key, iv, aad, 16);
}

Bytes ChaCha20_Poly1305_Decrypt(const CipherPack& pack, const Bytes& key, const Bytes& aad) {
    EnsureKeyLength(key, 32, "ChaCha20_Poly1305_Decrypt");
    Ensure(!pack.iv.empty(), "ChaCha20_Poly1305_Decrypt: iv must not be empty");
    Ensure(!pack.tag.empty(), "ChaCha20_Poly1305_Decrypt: tag must not be empty");
    return AeadDecrypt(EVP_chacha20_poly1305(), pack.ciphertext, key, pack.iv, aad, pack.tag);
}

#if !defined(OPENSSL_NO_SM4)
Bytes SM4_CBC_Encrypt(const Bytes& plaintext, const Bytes& key, const Bytes& iv) {
    EnsureKeyLength(key, 16, "SM4_CBC_Encrypt");
    EnsureIvLength(iv, 16, "SM4_CBC_Encrypt");
    return CipherEncryptRaw(EVP_sm4_cbc(), plaintext, key, iv, true);
}

Bytes SM4_CBC_Decrypt(const Bytes& ciphertext, const Bytes& key, const Bytes& iv) {
    EnsureKeyLength(key, 16, "SM4_CBC_Decrypt");
    EnsureIvLength(iv, 16, "SM4_CBC_Decrypt");
    return CipherDecryptRaw(EVP_sm4_cbc(), ciphertext, key, iv, true);
}

Bytes SM4_CTR_Encrypt(const Bytes& plaintext, const Bytes& key, const Bytes& iv) {
    EnsureKeyLength(key, 16, "SM4_CTR_Encrypt");
    EnsureIvLength(iv, 16, "SM4_CTR_Encrypt");
    return CipherEncryptRaw(EVP_sm4_ctr(), plaintext, key, iv, false);
}

Bytes SM4_CTR_Decrypt(const Bytes& ciphertext, const Bytes& key, const Bytes& iv) {
    EnsureKeyLength(key, 16, "SM4_CTR_Decrypt");
    EnsureIvLength(iv, 16, "SM4_CTR_Decrypt");
    return CipherDecryptRaw(EVP_sm4_ctr(), ciphertext, key, iv, false);
}
#endif

PemKeyPair GenerateRSAKeyPair(int bits) {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (!ctx) ThrowLastError("EVP_PKEY_CTX_new_id(RSA)");

    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_keygen_init(ctx) != 1 ||
        EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, bits) != 1 ||
        EVP_PKEY_keygen(ctx, &pkey) != 1) {
        EVP_PKEY_CTX_free(ctx);
        ThrowLastError("GenerateRSAKeyPair");
    }
    EVP_PKEY_CTX_free(ctx);

    PemKeyPair kp{PemFromPublicKey(pkey), PemFromPrivateKey(pkey)};
    EVP_PKEY_free(pkey);
    return kp;
}

PemKeyPair GenerateECKeyPair_P256() {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
    if (!ctx) ThrowLastError("EVP_PKEY_CTX_new_id(EC)");

    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_paramgen_init(ctx) != 1 ||
        EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, NID_X9_62_prime256v1) != 1) {
        EVP_PKEY_CTX_free(ctx);
        ThrowLastError("GenerateECKeyPair_P256/paramgen_init");
    }

    EVP_PKEY* params = nullptr;
    if (EVP_PKEY_paramgen(ctx, &params) != 1) {
        EVP_PKEY_CTX_free(ctx);
        ThrowLastError("GenerateECKeyPair_P256/paramgen");
    }
    EVP_PKEY_CTX_free(ctx);

    ctx = EVP_PKEY_CTX_new(params, nullptr);
    if (!ctx) {
        EVP_PKEY_free(params);
        ThrowLastError("EVP_PKEY_CTX_new(params)");
    }
    if (EVP_PKEY_keygen_init(ctx) != 1 || EVP_PKEY_keygen(ctx, &pkey) != 1) {
        EVP_PKEY_free(params);
        EVP_PKEY_CTX_free(ctx);
        ThrowLastError("GenerateECKeyPair_P256/keygen");
    }
    EVP_PKEY_free(params);
    EVP_PKEY_CTX_free(ctx);

    PemKeyPair kp{PemFromPublicKey(pkey), PemFromPrivateKey(pkey)};
    EVP_PKEY_free(pkey);
    return kp;
}

PemKeyPair GenerateEd25519KeyPair() {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr);
    if (!ctx) ThrowLastError("EVP_PKEY_CTX_new_id(ED25519)");

    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_keygen_init(ctx) != 1 || EVP_PKEY_keygen(ctx, &pkey) != 1) {
        EVP_PKEY_CTX_free(ctx);
        ThrowLastError("GenerateEd25519KeyPair");
    }
    EVP_PKEY_CTX_free(ctx);

    PemKeyPair kp{PemFromPublicKey(pkey), PemFromPrivateKey(pkey)};
    EVP_PKEY_free(pkey);
    return kp;
}

std::string PublicKeyPemFromPrivateKeyPem(const std::string& private_key_pem) {
    EVP_PKEY* pkey = LoadPrivateKeyFromPem(private_key_pem);
    std::string pem = PemFromPublicKey(pkey);
    EVP_PKEY_free(pkey);
    return pem;
}

Bytes RSA_Encrypt_OAEP_SHA256(const std::string& public_key_pem, const Bytes& plaintext) {
    EVP_PKEY* pkey = LoadPublicKeyFromPem(public_key_pem);
    Bytes out = PKeyEncrypt(pkey, plaintext, RSA_PKCS1_OAEP_PADDING, EVP_sha256());
    EVP_PKEY_free(pkey);
    return out;
}

Bytes RSA_Decrypt_OAEP_SHA256(const std::string& private_key_pem, const Bytes& ciphertext) {
    EVP_PKEY* pkey = LoadPrivateKeyFromPem(private_key_pem);
    Bytes out = PKeyDecrypt(pkey, ciphertext, RSA_PKCS1_OAEP_PADDING, EVP_sha256());
    EVP_PKEY_free(pkey);
    return out;
}

Bytes RSA_Sign_PSS_SHA256(const std::string& private_key_pem, const Bytes& data) {
    EVP_PKEY* pkey = LoadPrivateKeyFromPem(private_key_pem);
    Bytes sig = DigestSign(pkey, data, EVP_sha256(), RSA_PKCS1_PSS_PADDING, -1);
    EVP_PKEY_free(pkey);
    return sig;
}

bool RSA_Verify_PSS_SHA256(const std::string& public_key_pem, const Bytes& data, const Bytes& sig) {
    EVP_PKEY* pkey = LoadPublicKeyFromPem(public_key_pem);
    bool ok = DigestVerify(pkey, data, sig, EVP_sha256(), RSA_PKCS1_PSS_PADDING, -1);
    EVP_PKEY_free(pkey);
    return ok;
}

Bytes RSA_Sign_PKCS1v15_SHA256(const std::string& private_key_pem, const Bytes& data) {
    EVP_PKEY* pkey = LoadPrivateKeyFromPem(private_key_pem);
    Bytes sig = DigestSign(pkey, data, EVP_sha256(), RSA_PKCS1_PADDING, -1);
    EVP_PKEY_free(pkey);
    return sig;
}

bool RSA_Verify_PKCS1v15_SHA256(const std::string& public_key_pem, const Bytes& data, const Bytes& sig) {
    EVP_PKEY* pkey = LoadPublicKeyFromPem(public_key_pem);
    bool ok = DigestVerify(pkey, data, sig, EVP_sha256(), RSA_PKCS1_PADDING, -1);
    EVP_PKEY_free(pkey);
    return ok;
}

Bytes ECDSA_P256_Sign_SHA256(const std::string& private_key_pem, const Bytes& data) {
    EVP_PKEY* pkey = LoadPrivateKeyFromPem(private_key_pem);
    Bytes sig = DigestSign(pkey, data, EVP_sha256());
    EVP_PKEY_free(pkey);
    return sig;
}

bool ECDSA_P256_Verify_SHA256(const std::string& public_key_pem, const Bytes& data, const Bytes& sig) {
    EVP_PKEY* pkey = LoadPublicKeyFromPem(public_key_pem);
    bool ok = DigestVerify(pkey, data, sig, EVP_sha256());
    EVP_PKEY_free(pkey);
    return ok;
}

Bytes Ed25519_Sign(const std::string& private_key_pem, const Bytes& data) {
    EVP_PKEY* pkey = LoadPrivateKeyFromPem(private_key_pem);
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        EVP_PKEY_free(pkey);
        ThrowLastError("EVP_MD_CTX_new(Ed25519 sign)");
    }

    if (EVP_DigestSignInit(ctx, nullptr, nullptr, nullptr, pkey) != 1) {
        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        ThrowLastError("EVP_DigestSignInit(Ed25519)");
    }

    std::size_t sig_len = 0;
    if (EVP_DigestSign(ctx, nullptr, &sig_len,
                       data.empty() ? nullptr : data.data(), data.size()) != 1) {
        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        ThrowLastError("EVP_DigestSign(size, Ed25519)");
    }
    Bytes sig(sig_len);
    if (EVP_DigestSign(ctx, sig.data(), &sig_len,
                       data.empty() ? nullptr : data.data(), data.size()) != 1) {
        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        ThrowLastError("EVP_DigestSign(Ed25519)");
    }
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    sig.resize(sig_len);
    return sig;
}

bool Ed25519_Verify(const std::string& public_key_pem, const Bytes& data, const Bytes& sig) {
    EVP_PKEY* pkey = LoadPublicKeyFromPem(public_key_pem);
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        EVP_PKEY_free(pkey);
        ThrowLastError("EVP_MD_CTX_new(Ed25519 verify)");
    }
    if (EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, pkey) != 1) {
        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        ThrowLastError("EVP_DigestVerifyInit(Ed25519)");
    }
    int ret = EVP_DigestVerify(ctx,
                               sig.empty() ? nullptr : sig.data(), sig.size(),
                               data.empty() ? nullptr : data.data(), data.size());
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    if (ret == 1) return true;
    if (ret == 0) return false;
    ThrowLastError("EVP_DigestVerify(Ed25519)");
    return false;
}

#if !defined(OPENSSL_NO_SM2)
Bytes SM2_Sign_SM3(const std::string& private_key_pem,
                   const Bytes& data,
                   std::string_view user_id) {
    EVP_PKEY* pkey = LoadPrivateKeyFromPem(private_key_pem);
    EVP_MD_CTX* mctx = EVP_MD_CTX_new();
    if (!mctx) {
        EVP_PKEY_free(pkey);
        ThrowLastError("EVP_MD_CTX_new(SM2 sign)");
    }
    EVP_PKEY_CTX* pctx = nullptr;
    PrepareSM2ForSignVerify(pkey, mctx, &pctx, user_id.data(), user_id.size());

    if (EVP_DigestSignInit(mctx, nullptr, EVP_sm3(), nullptr, pkey) != 1) {
        EVP_PKEY_CTX_free(pctx);
        EVP_MD_CTX_free(mctx);
        EVP_PKEY_free(pkey);
        ThrowLastError("EVP_DigestSignInit(SM2)");
    }
    if (!data.empty() && EVP_DigestSignUpdate(mctx, data.data(), data.size()) != 1) {
        EVP_PKEY_CTX_free(pctx);
        EVP_MD_CTX_free(mctx);
        EVP_PKEY_free(pkey);
        ThrowLastError("EVP_DigestSignUpdate(SM2)");
    }
    std::size_t sig_len = 0;
    if (EVP_DigestSignFinal(mctx, nullptr, &sig_len) != 1) {
        EVP_PKEY_CTX_free(pctx);
        EVP_MD_CTX_free(mctx);
        EVP_PKEY_free(pkey);
        ThrowLastError("EVP_DigestSignFinal(size, SM2)");
    }
    Bytes sig(sig_len);
    if (EVP_DigestSignFinal(mctx, sig.data(), &sig_len) != 1) {
        EVP_PKEY_CTX_free(pctx);
        EVP_MD_CTX_free(mctx);
        EVP_PKEY_free(pkey);
        ThrowLastError("EVP_DigestSignFinal(SM2)");
    }
    sig.resize(sig_len);
    EVP_PKEY_CTX_free(pctx);
    EVP_MD_CTX_free(mctx);
    EVP_PKEY_free(pkey);
    return sig;
}

bool SM2_Verify_SM3(const std::string& public_key_pem,
                    const Bytes& data,
                    const Bytes& sig,
                    std::string_view user_id) {
    EVP_PKEY* pkey = LoadPublicKeyFromPem(public_key_pem);
    EVP_MD_CTX* mctx = EVP_MD_CTX_new();
    if (!mctx) {
        EVP_PKEY_free(pkey);
        ThrowLastError("EVP_MD_CTX_new(SM2 verify)");
    }
    EVP_PKEY_CTX* pctx = nullptr;
    PrepareSM2ForSignVerify(pkey, mctx, &pctx, user_id.data(), user_id.size());

    if (EVP_DigestVerifyInit(mctx, nullptr, EVP_sm3(), nullptr, pkey) != 1) {
        EVP_PKEY_CTX_free(pctx);
        EVP_MD_CTX_free(mctx);
        EVP_PKEY_free(pkey);
        ThrowLastError("EVP_DigestVerifyInit(SM2)");
    }
    if (!data.empty() && EVP_DigestVerifyUpdate(mctx, data.data(), data.size()) != 1) {
        EVP_PKEY_CTX_free(pctx);
        EVP_MD_CTX_free(mctx);
        EVP_PKEY_free(pkey);
        ThrowLastError("EVP_DigestVerifyUpdate(SM2)");
    }
    int ret = EVP_DigestVerifyFinal(mctx,
                                    sig.empty() ? nullptr : const_cast<unsigned char*>(sig.data()),
                                    sig.size());
    EVP_PKEY_CTX_free(pctx);
    EVP_MD_CTX_free(mctx);
    EVP_PKEY_free(pkey);
    if (ret == 1) return true;
    if (ret == 0) return false;
    ThrowLastError("EVP_DigestVerifyFinal(SM2)");
    return false;
}
#endif

FileEncryptResult FileCrypto::EncryptFile_AES256_GCM(const std::string& input_path,
                                                     const std::string& output_path,
                                                     const Bytes& key,
                                                     const Bytes& iv,
                                                     const Bytes& aad,
                                                     std::size_t chunk_size) {
    return EncryptFileAes256GcmCore(input_path, output_path, key, iv, aad, chunk_size);
}

void FileCrypto::DecryptFile_AES256_GCM(const std::string& input_path,
                                        const std::string& output_path,
                                        const Bytes& key,
                                        const Bytes& iv,
                                        const Bytes& tag,
                                        const Bytes& aad,
                                        std::size_t chunk_size) {
    EnsureNotEmptyPath(input_path, "FileCrypto::DecryptFile_AES256_GCM");
    std::ifstream ifs(input_path, std::ios::binary);
    if (!ifs) throw OpenSslException("FileCrypto::DecryptFile_AES256_GCM: cannot open input file: " + input_path);
    std::uint64_t file_size = GetFileSize(ifs);
    ifs.seekg(0, std::ios::beg);
    DecryptFileAes256GcmCore(ifs, file_size, output_path, key, iv, tag, aad, chunk_size);
}

PasswordFileEncryptResult FileCrypto::EncryptFileWithPassword_AES256_GCM_PBKDF2(
    const std::string& input_path,
    const std::string& output_path,
    std::string_view password,
    const Bytes& salt,
    int iterations,
    const Bytes& aad,
    const Bytes& iv,
    std::size_t chunk_size) {
    PasswordFileEncryptResult result;
    result.salt = salt.empty() ? RandomBytes(16) : salt;
    result.iterations = iterations;
    Bytes key = DeriveAes256KeyFromPassword_PBKDF2_SHA256(password, result.salt, iterations);
    Bytes actual_iv = iv.empty() ? RandomBytes(12) : iv;
    FileEncryptResult base = EncryptFileAes256GcmCore(input_path, output_path, key, actual_iv, aad, chunk_size);
    result.iv = std::move(base.iv);
    result.tag = std::move(base.tag);
    result.plain_size = base.plain_size;
    result.cipher_size = base.cipher_size;
    return result;
}

void FileCrypto::DecryptFileWithPassword_AES256_GCM_PBKDF2(const std::string& input_path,
                                                           const std::string& output_path,
                                                           std::string_view password,
                                                           const Bytes& salt,
                                                           int iterations,
                                                           const Bytes& iv,
                                                           const Bytes& tag,
                                                           const Bytes& aad,
                                                           std::size_t chunk_size) {
    EnsureNotEmptyPath(input_path, "FileCrypto::DecryptFileWithPassword_AES256_GCM_PBKDF2");
    Bytes key = DeriveAes256KeyFromPassword_PBKDF2_SHA256(password, salt, iterations);
    std::ifstream ifs(input_path, std::ios::binary);
    if (!ifs) throw OpenSslException("FileCrypto::DecryptFileWithPassword_AES256_GCM_PBKDF2: cannot open input file: " + input_path);
    std::uint64_t file_size = GetFileSize(ifs);
    ifs.seekg(0, std::ios::beg);
    DecryptFileAes256GcmCore(ifs, file_size, output_path, key, iv, tag, aad, chunk_size);
}

void FileCrypto::EncryptFileToContainerWithPassword_AES256_GCM_PBKDF2(
    const std::string& input_path,
    const std::string& container_output_path,
    std::string_view password,
    const Bytes& salt,
    int iterations,
    std::size_t chunk_size) {
    Bytes actual_salt = salt.empty() ? RandomBytes(16) : salt;
    Bytes iv = RandomBytes(12);
    Bytes key = DeriveAes256KeyFromPassword_PBKDF2_SHA256(password, actual_salt, iterations);

    EnsureNotEmptyPath(input_path, "FileCrypto::EncryptFileToContainerWithPassword_AES256_GCM_PBKDF2");
    EnsureNotEmptyPath(container_output_path, "FileCrypto::EncryptFileToContainerWithPassword_AES256_GCM_PBKDF2");

    std::ifstream ifs(input_path, std::ios::binary);
    if (!ifs) throw OpenSslException("FileCrypto::EncryptFileToContainerWithPassword_AES256_GCM_PBKDF2: cannot open input file: " + input_path);
    std::uint64_t plain_size = GetFileSize(ifs);
    ifs.seekg(0, std::ios::beg);

    std::ofstream ofs(container_output_path, std::ios::binary | std::ios::trunc);
    if (!ofs) throw OpenSslException("FileCrypto::EncryptFileToContainerWithPassword_AES256_GCM_PBKDF2: cannot open output file: " + container_output_path);

    PasswordContainerHeaderV1 header{};
    std::memcpy(header.magic, kContainerMagic, sizeof(kContainerMagic));
    header.version = kContainerVersion;
    header.kdf_id = kContainerKdfPbkdf2Sha256;
    header.cipher_id = kContainerCipherAes256Gcm;
    header.iterations = static_cast<std::uint32_t>(iterations);
    header.salt_len = static_cast<std::uint32_t>(actual_salt.size());
    header.iv_len = static_cast<std::uint32_t>(iv.size());
    header.tag_len = 16;
    header.plain_size = plain_size;
    WriteContainerHeader(ofs, header);
    ofs.write(reinterpret_cast<const char*>(actual_salt.data()), static_cast<std::streamsize>(actual_salt.size()));
    ofs.write(reinterpret_cast<const char*>(iv.data()), static_cast<std::streamsize>(iv.size()));
    if (!ofs) throw OpenSslException("FileCrypto::EncryptFileToContainerWithPassword_AES256_GCM_PBKDF2: failed to write container header body");

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) ThrowLastError("EVP_CIPHER_CTX_new(container encrypt)");

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("Container encrypt/EVP_EncryptInit_ex(phase1)");
    }
    if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("Container encrypt/EVP_EncryptInit_ex(phase2)");
    }

    Bytes in_buf(chunk_size);
    Bytes out_buf(chunk_size + 32);
    int len = 0;
    while (ifs) {
        ifs.read(reinterpret_cast<char*>(in_buf.data()), static_cast<std::streamsize>(in_buf.size()));
        std::streamsize got = ifs.gcount();
        if (got <= 0) break;
        if (EVP_EncryptUpdate(ctx,
                              out_buf.data(),
                              &len,
                              in_buf.data(),
                              static_cast<int>(got)) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            ThrowLastError("Container encrypt/EncryptUpdate");
        }
        if (len > 0) {
            ofs.write(reinterpret_cast<const char*>(out_buf.data()), len);
            if (!ofs) {
                EVP_CIPHER_CTX_free(ctx);
                throw OpenSslException("FileCrypto::EncryptFileToContainerWithPassword_AES256_GCM_PBKDF2: failed to write ciphertext");
            }
        }
    }

    if (EVP_EncryptFinal_ex(ctx, out_buf.data(), &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("Container encrypt/EncryptFinal");
    }
    if (len > 0) {
        ofs.write(reinterpret_cast<const char*>(out_buf.data()), len);
        if (!ofs) {
            EVP_CIPHER_CTX_free(ctx);
            throw OpenSslException("FileCrypto::EncryptFileToContainerWithPassword_AES256_GCM_PBKDF2: failed to write final ciphertext block");
        }
    }

    Bytes tag(16);
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, static_cast<int>(tag.size()), tag.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("Container encrypt/GET_TAG");
    }
    EVP_CIPHER_CTX_free(ctx);

    ofs.write(reinterpret_cast<const char*>(tag.data()), static_cast<std::streamsize>(tag.size()));
    if (!ofs) {
        throw OpenSslException("FileCrypto::EncryptFileToContainerWithPassword_AES256_GCM_PBKDF2: failed to write tag");
    }
}

void FileCrypto::DecryptFileFromContainerWithPassword_AES256_GCM_PBKDF2(
    const std::string& container_input_path,
    const std::string& output_path,
    std::string_view password,
    std::size_t chunk_size) {
    EnsureNotEmptyPath(container_input_path, "FileCrypto::DecryptFileFromContainerWithPassword_AES256_GCM_PBKDF2");
    std::ifstream ifs(container_input_path, std::ios::binary);
    if (!ifs) throw OpenSslException("FileCrypto::DecryptFileFromContainerWithPassword_AES256_GCM_PBKDF2: cannot open input file: " + container_input_path);

    PasswordContainerHeaderV1 header = ReadContainerHeader(ifs);
    Bytes salt(header.salt_len);
    Bytes iv(header.iv_len);
    Bytes tag(header.tag_len);

    ifs.read(reinterpret_cast<char*>(salt.data()), static_cast<std::streamsize>(salt.size()));
    ifs.read(reinterpret_cast<char*>(iv.data()), static_cast<std::streamsize>(iv.size()));
    if (!ifs) {
        throw OpenSslException("FileCrypto::DecryptFileFromContainerWithPassword_AES256_GCM_PBKDF2: failed to read salt/iv");
    }

    std::uint64_t total_size = GetFileSize(ifs);
    std::uint64_t meta_size = sizeof(PasswordContainerHeaderV1) + salt.size() + iv.size() + tag.size();
    if (total_size < meta_size) {
        throw OpenSslException("FileCrypto::DecryptFileFromContainerWithPassword_AES256_GCM_PBKDF2: invalid container size");
    }
    std::uint64_t cipher_size = total_size - meta_size;

    auto tag_pos = static_cast<std::streamoff>(total_size - tag.size());
    ifs.seekg(tag_pos, std::ios::beg);
    ifs.read(reinterpret_cast<char*>(tag.data()), static_cast<std::streamsize>(tag.size()));
    if (!ifs) {
        throw OpenSslException("FileCrypto::DecryptFileFromContainerWithPassword_AES256_GCM_PBKDF2: failed to read tag");
    }

    Bytes key = DeriveAes256KeyFromPassword_PBKDF2_SHA256(password, salt, static_cast<int>(header.iterations));

    auto cipher_pos = static_cast<std::streamoff>(sizeof(PasswordContainerHeaderV1) + salt.size() + iv.size());
    ifs.clear();
    ifs.seekg(cipher_pos, std::ios::beg);
    DecryptFileAes256GcmCore(ifs, cipher_size, output_path, key, iv, tag, {}, chunk_size);
}

} // namespace ytpp::sys_core::encryption
