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
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

namespace ytpp::sys_core::encryption {

OpenSslException::OpenSslException(_In_ const std::string& msg) : std::runtime_error(msg) {}

namespace {

const unsigned char* AsUnsignedBytes(_In_ const void* p) {
    return static_cast<const unsigned char*>(p);
}

unsigned char* AsUnsignedBytes(_Inout_ void* p) {
    return static_cast<unsigned char*>(p);
}

int HexValue(_In_ char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

void Ensure(_In_ bool ok, _In_ const std::string& msg) {
    if (!ok) {
        throw OpenSslException(msg);
    }
}

void EnsureKeyLength(_In_ const Bytes& key, _In_ std::size_t expected, _In_ const char* where) {
    if (key.size() != expected) {
        throw OpenSslException(std::string(where) + ": invalid key length, expected " + std::to_string(expected) +
                               " bytes");
    }
}

void EnsureIvLength(_In_ const Bytes& iv, _In_ std::size_t expected, _In_ const char* where) {
    if (iv.size() != expected) {
        throw OpenSslException(std::string(where) + ": invalid iv length, expected " + std::to_string(expected) +
                               " bytes");
    }
}

void EnsureNotEmptyPath(_In_ const std::string& path, _In_ const char* where) {
    if (path.empty()) {
        throw OpenSslException(std::string(where) + ": path must not be empty");
    }
}

std::uint64_t GetFileSize(_Inout_ std::ifstream& ifs) {
    auto current = ifs.tellg();
    ifs.seekg(0, std::ios::end);
    auto end = ifs.tellg();
    ifs.seekg(current, std::ios::beg);
    if (end < 0) {
        throw OpenSslException("GetFileSize: failed to query file size");
    }
    return static_cast<std::uint64_t>(end);
}

Bytes DigestImpl(_In_ const void* data, _In_ std::size_t len, _In_ const EVP_MD* md) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx)
        ThrowLastError("EVP_MD_CTX_new");

    Bytes out(static_cast<std::size_t>(EVP_MD_size(md)));
    unsigned int outputLength = 0;

    if (EVP_DigestInit_ex(ctx, md, nullptr) != 1 || (len > 0 && EVP_DigestUpdate(ctx, data, len) != 1) ||
        EVP_DigestFinal_ex(ctx, out.data(), &outputLength) != 1) {
        EVP_MD_CTX_free(ctx);
        ThrowLastError("Digest");
    }
    EVP_MD_CTX_free(ctx);
    out.resize(outputLength);
    return out;
}

Bytes DigestImpl(_In_ std::string_view s, _In_ const EVP_MD* md) {
    return DigestImpl(s.data(), s.size(), md);
}

Bytes HmacImpl(_In_ const void* data, _In_ std::size_t len, _In_ const void* key, _In_ std::size_t keyLength,
               _In_ const EVP_MD* md) {
    unsigned int outputLength = EVP_MAX_MD_SIZE;
    Bytes out(outputLength);
    unsigned char* ret =
        HMAC(md, key, static_cast<int>(keyLength), AsUnsignedBytes(data), len, out.data(), &outputLength);
    if (!ret)
        ThrowLastError("HMAC");
    out.resize(outputLength);
    return out;
}

Bytes HmacImpl(_In_ std::string_view data, _In_ std::string_view key, _In_ const EVP_MD* md) {
    return HmacImpl(data.data(), data.size(), key.data(), key.size(), md);
}

Bytes CipherEncryptRaw(_In_ const EVP_CIPHER* cipher, _In_ const Bytes& plaintext, _In_ const Bytes& key,
                       _In_ const Bytes& iv, _In_ bool padding = true) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        ThrowLastError("EVP_CIPHER_CTX_new");

    if (EVP_EncryptInit_ex(ctx, cipher, nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("EVP_EncryptInit_ex(phase1)");
    }
    if (EVP_CIPHER_CTX_set_padding(ctx, padding ? 1 : 0) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("EVP_CIPHER_CTX_set_padding");
    }
    if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.empty() ? nullptr : key.data(),
                           iv.empty() ? nullptr : iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("EVP_EncryptInit_ex(phase2)");
    }

    Bytes out(plaintext.size() + static_cast<std::size_t>(EVP_CIPHER_block_size(cipher)));
    int outputLength1 = 0;
    int outputLength2 = 0;
    if ((!plaintext.empty() && EVP_EncryptUpdate(ctx, out.data(), &outputLength1, plaintext.data(),
                                                 static_cast<int>(plaintext.size())) != 1) ||
        EVP_EncryptFinal_ex(ctx, out.data() + outputLength1, &outputLength2) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("CipherEncryptRaw");
    }

    EVP_CIPHER_CTX_free(ctx);
    out.resize(static_cast<std::size_t>(outputLength1 + outputLength2));
    return out;
}

Bytes CipherDecryptRaw(_In_ const EVP_CIPHER* cipher, _In_ const Bytes& ciphertext, _In_ const Bytes& key,
                       _In_ const Bytes& iv, _In_ bool padding = true) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        ThrowLastError("EVP_CIPHER_CTX_new");

    if (EVP_DecryptInit_ex(ctx, cipher, nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("EVP_DecryptInit_ex(phase1)");
    }
    if (EVP_CIPHER_CTX_set_padding(ctx, padding ? 1 : 0) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("EVP_CIPHER_CTX_set_padding");
    }
    if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.empty() ? nullptr : key.data(),
                           iv.empty() ? nullptr : iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("EVP_DecryptInit_ex(phase2)");
    }

    Bytes out(ciphertext.size() + static_cast<std::size_t>(EVP_CIPHER_block_size(cipher)));
    int outputLength1 = 0;
    int outputLength2 = 0;
    if ((!ciphertext.empty() && EVP_DecryptUpdate(ctx, out.data(), &outputLength1, ciphertext.data(),
                                                  static_cast<int>(ciphertext.size())) != 1) ||
        EVP_DecryptFinal_ex(ctx, out.data() + outputLength1, &outputLength2) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("CipherDecryptRaw");
    }

    EVP_CIPHER_CTX_free(ctx);
    out.resize(static_cast<std::size_t>(outputLength1 + outputLength2));
    return out;
}

CipherPack AeadEncrypt(_In_ const EVP_CIPHER* cipher, _In_ const Bytes& plaintext, _In_ const Bytes& key,
                       _In_ const Bytes& iv, _In_ const Bytes& aad, _In_ int tagLength = 16) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        ThrowLastError("EVP_CIPHER_CTX_new");

    if (EVP_EncryptInit_ex(ctx, cipher, nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("AeadEncrypt/EVP_EncryptInit_ex(phase1)");
    }

    const int kDefaultIvLength = 12;
    if (static_cast<int>(iv.size()) != kDefaultIvLength) {
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, static_cast<int>(iv.size()), nullptr) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            ThrowLastError("AeadEncrypt/SET_IVLEN");
        }
    }

    if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1) {
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
    if (!plaintext.empty() && EVP_EncryptUpdate(ctx, pack.ciphertext.data(), &len, plaintext.data(),
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

    pack.tag.resize(static_cast<std::size_t>(tagLength));
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, tagLength, pack.tag.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("AeadEncrypt/GET_TAG");
    }

    EVP_CIPHER_CTX_free(ctx);
    return pack;
}

Bytes AeadDecrypt(_In_ const EVP_CIPHER* cipher, _In_ const Bytes& ciphertext, _In_ const Bytes& key,
                  _In_ const Bytes& iv, _In_ const Bytes& aad, _In_ const Bytes& tag) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        ThrowLastError("EVP_CIPHER_CTX_new");

    if (EVP_DecryptInit_ex(ctx, cipher, nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("AeadDecrypt/EVP_DecryptInit_ex(phase1)");
    }

    const int kDefaultIvLength = 12;
    if (static_cast<int>(iv.size()) != kDefaultIvLength) {
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, static_cast<int>(iv.size()), nullptr) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            ThrowLastError("AeadDecrypt/SET_IVLEN");
        }
    }

    if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("AeadDecrypt/EVP_DecryptInit_ex(phase2)");
    }

    int len = 0;
    if (!aad.empty() && EVP_DecryptUpdate(ctx, nullptr, &len, aad.data(), static_cast<int>(aad.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("AeadDecrypt/AAD");
    }

    Bytes plaintext(ciphertext.size());
    if (!ciphertext.empty() &&
        EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(), static_cast<int>(ciphertext.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("AeadDecrypt/data");
    }
    int total = len;

    Bytes mutableTag = tag;
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, static_cast<int>(mutableTag.size()), mutableTag.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("AeadDecrypt/SET_TAG");
    }

    int finalLength = 0;
    const int ret = EVP_DecryptFinal_ex(ctx, plaintext.data() + total, &finalLength);
    EVP_CIPHER_CTX_free(ctx);
    if (ret != 1) {
        throw OpenSslException("AeadDecrypt: authentication failed (tag mismatch or data corrupted)");
    }
    total += finalLength;
    plaintext.resize(static_cast<std::size_t>(total));
    return plaintext;
}

EVP_PKEY* LoadPublicKeyFromPem(_In_ const std::string& pem) {
    BIO* bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
    if (!bio)
        ThrowLastError("BIO_new_mem_buf(public key)");
    EVP_PKEY* pkey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!pkey)
        ThrowLastError("PEM_read_bio_PUBKEY");
    return pkey;
}

EVP_PKEY* LoadPrivateKeyFromPem(_In_ const std::string& pem, _In_ const char* password = nullptr) {
    BIO* bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
    if (!bio)
        ThrowLastError("BIO_new_mem_buf(private key)");
    EVP_PKEY* pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, const_cast<char*>(password));
    BIO_free(bio);
    if (!pkey)
        ThrowLastError("PEM_read_bio_PrivateKey");
    return pkey;
}

std::string PemFromPrivateKey(_Inout_ EVP_PKEY* pkey) {
    BIO* bio = BIO_new(BIO_s_mem());
    if (!bio)
        ThrowLastError("BIO_new(private PEM)");
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

std::string PemFromPublicKey(_Inout_ EVP_PKEY* pkey) {
    BIO* bio = BIO_new(BIO_s_mem());
    if (!bio)
        ThrowLastError("BIO_new(public PEM)");
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

Bytes PKeyEncrypt(_Inout_ EVP_PKEY* pkey, _In_ const Bytes& plaintext, _In_ int rsaPadding = RSA_PKCS1_OAEP_PADDING,
                  _In_ const EVP_MD* oaepDigest = EVP_sha256()) {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(pkey, nullptr);
    if (!ctx)
        ThrowLastError("EVP_PKEY_CTX_new(encrypt)");

    if (EVP_PKEY_encrypt_init(ctx) != 1) {
        EVP_PKEY_CTX_free(ctx);
        ThrowLastError("EVP_PKEY_encrypt_init");
    }
    if (EVP_PKEY_base_id(pkey) == EVP_PKEY_RSA) {
        if (EVP_PKEY_CTX_set_rsa_padding(ctx, rsaPadding) != 1) {
            EVP_PKEY_CTX_free(ctx);
            ThrowLastError("EVP_PKEY_CTX_set_rsa_padding");
        }
        if (rsaPadding == RSA_PKCS1_OAEP_PADDING) {
            if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx, oaepDigest) != 1 ||
                EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, oaepDigest) != 1) {
                EVP_PKEY_CTX_free(ctx);
                ThrowLastError("EVP_PKEY_CTX_set_rsa_oaep_md/mgf1");
            }
        }
    }

    std::size_t outputLength = 0;
    if (EVP_PKEY_encrypt(ctx, nullptr, &outputLength, plaintext.empty() ? nullptr : plaintext.data(),
                         plaintext.size()) != 1) {
        EVP_PKEY_CTX_free(ctx);
        ThrowLastError("EVP_PKEY_encrypt(size)");
    }

    Bytes out(outputLength);
    if (EVP_PKEY_encrypt(ctx, out.data(), &outputLength, plaintext.empty() ? nullptr : plaintext.data(),
                         plaintext.size()) != 1) {
        EVP_PKEY_CTX_free(ctx);
        ThrowLastError("EVP_PKEY_encrypt");
    }
    EVP_PKEY_CTX_free(ctx);
    out.resize(outputLength);
    return out;
}

Bytes PKeyDecrypt(_Inout_ EVP_PKEY* pkey, _In_ const Bytes& ciphertext, _In_ int rsaPadding = RSA_PKCS1_OAEP_PADDING,
                  _In_ const EVP_MD* oaepDigest = EVP_sha256()) {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(pkey, nullptr);
    if (!ctx)
        ThrowLastError("EVP_PKEY_CTX_new(decrypt)");

    if (EVP_PKEY_decrypt_init(ctx) != 1) {
        EVP_PKEY_CTX_free(ctx);
        ThrowLastError("EVP_PKEY_decrypt_init");
    }
    if (EVP_PKEY_base_id(pkey) == EVP_PKEY_RSA) {
        if (EVP_PKEY_CTX_set_rsa_padding(ctx, rsaPadding) != 1) {
            EVP_PKEY_CTX_free(ctx);
            ThrowLastError("EVP_PKEY_CTX_set_rsa_padding");
        }
        if (rsaPadding == RSA_PKCS1_OAEP_PADDING) {
            if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx, oaepDigest) != 1 ||
                EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, oaepDigest) != 1) {
                EVP_PKEY_CTX_free(ctx);
                ThrowLastError("EVP_PKEY_CTX_set_rsa_oaep_md/mgf1");
            }
        }
    }

    std::size_t outputLength = 0;
    if (EVP_PKEY_decrypt(ctx, nullptr, &outputLength, ciphertext.empty() ? nullptr : ciphertext.data(),
                         ciphertext.size()) != 1) {
        EVP_PKEY_CTX_free(ctx);
        ThrowLastError("EVP_PKEY_decrypt(size)");
    }

    Bytes out(outputLength);
    if (EVP_PKEY_decrypt(ctx, out.data(), &outputLength, ciphertext.empty() ? nullptr : ciphertext.data(),
                         ciphertext.size()) != 1) {
        EVP_PKEY_CTX_free(ctx);
        ThrowLastError("EVP_PKEY_decrypt");
    }
    EVP_PKEY_CTX_free(ctx);
    out.resize(outputLength);
    return out;
}

Bytes DigestSign(_Inout_ EVP_PKEY* pkey, _In_ const Bytes& data, _In_ const EVP_MD* md, _In_ int rsaPadding = 0,
                 _In_ int rsaPssSaltLength = -1) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx)
        ThrowLastError("EVP_MD_CTX_new(sign)");

    EVP_PKEY_CTX* pctx = nullptr;
    if (EVP_DigestSignInit(ctx, &pctx, md, nullptr, pkey) != 1) {
        EVP_MD_CTX_free(ctx);
        ThrowLastError("EVP_DigestSignInit");
    }

    if (EVP_PKEY_base_id(pkey) == EVP_PKEY_RSA && rsaPadding != 0) {
        if (EVP_PKEY_CTX_set_rsa_padding(pctx, rsaPadding) != 1) {
            EVP_MD_CTX_free(ctx);
            ThrowLastError("EVP_PKEY_CTX_set_rsa_padding(sign)");
        }
        if (rsaPadding == RSA_PKCS1_PSS_PADDING) {
            if (EVP_PKEY_CTX_set_rsa_mgf1_md(pctx, md) != 1 ||
                EVP_PKEY_CTX_set_rsa_pss_saltlen(pctx, rsaPssSaltLength) != 1) {
                EVP_MD_CTX_free(ctx);
                ThrowLastError("EVP_PKEY_CTX_set_rsa_pss params");
            }
        }
    }

    if ((!data.empty() && EVP_DigestSignUpdate(ctx, data.data(), data.size()) != 1)) {
        EVP_MD_CTX_free(ctx);
        ThrowLastError("EVP_DigestSignUpdate");
    }

    std::size_t signatureLength = 0;
    if (EVP_DigestSignFinal(ctx, nullptr, &signatureLength) != 1) {
        EVP_MD_CTX_free(ctx);
        ThrowLastError("EVP_DigestSignFinal(size)");
    }

    Bytes sig(signatureLength);
    if (EVP_DigestSignFinal(ctx, sig.data(), &signatureLength) != 1) {
        EVP_MD_CTX_free(ctx);
        ThrowLastError("EVP_DigestSignFinal");
    }
    EVP_MD_CTX_free(ctx);
    sig.resize(signatureLength);
    return sig;
}

bool DigestVerify(_Inout_ EVP_PKEY* pkey, _In_ const Bytes& data, _In_ const Bytes& sig, _In_ const EVP_MD* md,
                  _In_ int rsaPadding = 0, _In_ int rsaPssSaltLength = -1) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx)
        ThrowLastError("EVP_MD_CTX_new(verify)");

    EVP_PKEY_CTX* pctx = nullptr;
    if (EVP_DigestVerifyInit(ctx, &pctx, md, nullptr, pkey) != 1) {
        EVP_MD_CTX_free(ctx);
        ThrowLastError("EVP_DigestVerifyInit");
    }

    if (EVP_PKEY_base_id(pkey) == EVP_PKEY_RSA && rsaPadding != 0) {
        if (EVP_PKEY_CTX_set_rsa_padding(pctx, rsaPadding) != 1) {
            EVP_MD_CTX_free(ctx);
            ThrowLastError("EVP_PKEY_CTX_set_rsa_padding(verify)");
        }
        if (rsaPadding == RSA_PKCS1_PSS_PADDING) {
            if (EVP_PKEY_CTX_set_rsa_mgf1_md(pctx, md) != 1 ||
                EVP_PKEY_CTX_set_rsa_pss_saltlen(pctx, rsaPssSaltLength) != 1) {
                EVP_MD_CTX_free(ctx);
                ThrowLastError("EVP_PKEY_CTX_set_rsa_pss verify params");
            }
        }
    }

    if ((!data.empty() && EVP_DigestVerifyUpdate(ctx, data.data(), data.size()) != 1)) {
        EVP_MD_CTX_free(ctx);
        ThrowLastError("EVP_DigestVerifyUpdate");
    }

    int ret = EVP_DigestVerifyFinal(ctx, sig.empty() ? nullptr : const_cast<unsigned char*>(sig.data()), sig.size());
    EVP_MD_CTX_free(ctx);
    if (ret == 1)
        return true;
    if (ret == 0)
        return false;
    ThrowLastError("EVP_DigestVerifyFinal");
    return false;
}

#if !defined(OPENSSL_NO_SM2)
void PrepareSM2ForSignVerify(_Inout_ EVP_PKEY* pkey, _Inout_ EVP_MD_CTX* mctx, _Out_ EVP_PKEY_CTX** outputPkeyContext,
                             _In_ const void* id, _In_ std::size_t idLength) {
#if OPENSSL_VERSION_NUMBER < 0x30000000L
    if (EVP_PKEY_base_id(pkey) == EVP_PKEY_EC) {
        if (EVP_PKEY_set_alias_type(pkey, EVP_PKEY_SM2) != 1) {
            ThrowLastError("EVP_PKEY_set_alias_type(SM2)");
        }
    }
#endif
    EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new(pkey, nullptr);
    if (!pctx)
        ThrowLastError("EVP_PKEY_CTX_new(SM2)");
    if (EVP_PKEY_CTX_set1_id(pctx, const_cast<void*>(id), static_cast<int>(idLength)) != 1) {
        EVP_PKEY_CTX_free(pctx);
        ThrowLastError("EVP_PKEY_CTX_set1_id(SM2)");
    }
    EVP_MD_CTX_set_pkey_ctx(mctx, pctx);
    *outputPkeyContext = pctx;
}
#endif

FileEncryptResult EncryptFileAes256GcmCore(_In_ const std::string& inputPath, _In_ const std::string& outputPath,
                                           _In_ const Bytes& key, _In_ const Bytes& iv, _In_ const Bytes& aad,
                                           _In_ std::size_t chunkSize) {
    EnsureNotEmptyPath(inputPath, "EncryptFileAes256GcmCore");
    EnsureNotEmptyPath(outputPath, "EncryptFileAes256GcmCore");
    EnsureKeyLength(key, 32, "EncryptFileAes256GcmCore");
    Ensure(!iv.empty(), "EncryptFileAes256GcmCore: iv must not be empty");
    Ensure(chunkSize > 0, "EncryptFileAes256GcmCore: chunkSize must be > 0");

    std::ifstream ifs(inputPath, std::ios::binary);
    if (!ifs)
        throw OpenSslException("EncryptFileAes256GcmCore: cannot open input file: " + inputPath);
    std::ofstream ofs(outputPath, std::ios::binary | std::ios::trunc);
    if (!ofs)
        throw OpenSslException("EncryptFileAes256GcmCore: cannot open output file: " + outputPath);

    FileEncryptResult result;
    result.iv = iv;
    result.plainSize = GetFileSize(ifs);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        ThrowLastError("EVP_CIPHER_CTX_new(file encrypt)");

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

    Bytes inputBuffer(chunkSize);
    Bytes outputBuffer(chunkSize + 32);

    while (ifs) {
        ifs.read(reinterpret_cast<char*>(inputBuffer.data()), static_cast<std::streamsize>(inputBuffer.size()));
        std::streamsize got = ifs.gcount();
        if (got <= 0)
            break;

        if (EVP_EncryptUpdate(ctx, outputBuffer.data(), &len, inputBuffer.data(), static_cast<int>(got)) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            ThrowLastError("EncryptFileAes256GcmCore/EncryptUpdate");
        }
        if (len > 0) {
            ofs.write(reinterpret_cast<const char*>(outputBuffer.data()), len);
            if (!ofs) {
                EVP_CIPHER_CTX_free(ctx);
                throw OpenSslException("EncryptFileAes256GcmCore: failed to write output file");
            }
            result.cipherSize += static_cast<std::uint64_t>(len);
        }
    }

    if (EVP_EncryptFinal_ex(ctx, outputBuffer.data(), &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("EncryptFileAes256GcmCore/EncryptFinal");
    }
    if (len > 0) {
        ofs.write(reinterpret_cast<const char*>(outputBuffer.data()), len);
        if (!ofs) {
            EVP_CIPHER_CTX_free(ctx);
            throw OpenSslException("EncryptFileAes256GcmCore: failed to write final block");
        }
        result.cipherSize += static_cast<std::uint64_t>(len);
    }

    result.tag.resize(16);
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, static_cast<int>(result.tag.size()), result.tag.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("EncryptFileAes256GcmCore/GET_TAG");
    }

    EVP_CIPHER_CTX_free(ctx);
    return result;
}

void DecryptFileAes256GcmCore(_Inout_ std::ifstream& ifs, _In_ std::uint64_t cipherSize,
                              _In_ const std::string& outputPath, _In_ const Bytes& key, _In_ const Bytes& iv,
                              _In_ const Bytes& tag, _In_ const Bytes& aad, _In_ std::size_t chunkSize) {
    EnsureNotEmptyPath(outputPath, "DecryptFileAes256GcmCore");
    EnsureKeyLength(key, 32, "DecryptFileAes256GcmCore");
    Ensure(!iv.empty(), "DecryptFileAes256GcmCore: iv must not be empty");
    Ensure(!tag.empty(), "DecryptFileAes256GcmCore: tag must not be empty");
    Ensure(chunkSize > 0, "DecryptFileAes256GcmCore: chunkSize must be > 0");

    std::ofstream ofs(outputPath, std::ios::binary | std::ios::trunc);
    if (!ofs)
        throw OpenSslException("DecryptFileAes256GcmCore: cannot open output file: " + outputPath);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        ThrowLastError("EVP_CIPHER_CTX_new(file decrypt)");

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

    Bytes inputBuffer(chunkSize);
    Bytes outputBuffer(chunkSize + 32);
    std::uint64_t remaining = cipherSize;
    while (remaining > 0) {
        std::size_t want = static_cast<std::size_t>(std::min<std::uint64_t>(remaining, inputBuffer.size()));
        ifs.read(reinterpret_cast<char*>(inputBuffer.data()), static_cast<std::streamsize>(want));
        std::streamsize got = ifs.gcount();
        if (got <= 0) {
            EVP_CIPHER_CTX_free(ctx);
            throw OpenSslException("DecryptFileAes256GcmCore: unexpected end of encrypted file");
        }

        if (EVP_DecryptUpdate(ctx, outputBuffer.data(), &len, inputBuffer.data(), static_cast<int>(got)) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            ThrowLastError("DecryptFileAes256GcmCore/DecryptUpdate");
        }
        if (len > 0) {
            ofs.write(reinterpret_cast<const char*>(outputBuffer.data()), len);
            if (!ofs) {
                EVP_CIPHER_CTX_free(ctx);
                throw OpenSslException("DecryptFileAes256GcmCore: failed to write output file");
            }
        }
        remaining -= static_cast<std::uint64_t>(got);
    }

    Bytes mutableTag = tag;
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, static_cast<int>(mutableTag.size()), mutableTag.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("DecryptFileAes256GcmCore/SET_TAG");
    }

    if (EVP_DecryptFinal_ex(ctx, outputBuffer.data(), &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw OpenSslException(
            "DecryptFileAes256GcmCore: authentication failed (password/key/iv/tag mismatch or file corrupted)");
    }
    if (len > 0) {
        ofs.write(reinterpret_cast<const char*>(outputBuffer.data()), len);
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
    std::uint32_t kdfId;
    std::uint32_t cipherId;
    std::uint32_t iterations;
    std::uint32_t saltLength;
    std::uint32_t ivLength;
    std::uint32_t tagLength;
    std::uint64_t plainSize;
};

constexpr char kContainerMagic[8] = {'Y', 'T', 'P', 'P', 'E', 'N', 'C', '1'};
constexpr std::uint32_t kContainerVersion = 1;
constexpr std::uint32_t kContainerKdfPbkdf2Sha256 = 1;
constexpr std::uint32_t kContainerCipherAes256Gcm = 1;

void WriteContainerHeader(_Inout_ std::ofstream& ofs, _In_ const PasswordContainerHeaderV1& header) {
    ofs.write(reinterpret_cast<const char*>(&header), sizeof(header));
    if (!ofs)
        throw OpenSslException("WriteContainerHeader: failed");
}

PasswordContainerHeaderV1 ReadContainerHeader(_Inout_ std::ifstream& ifs) {
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
    if (header.kdfId != kContainerKdfPbkdf2Sha256 || header.cipherId != kContainerCipherAes256Gcm) {
        throw OpenSslException("ReadContainerHeader: unsupported container algorithm");
    }
    if (header.saltLength == 0 || header.ivLength == 0 || header.tagLength == 0) {
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
        if (!out.empty())
            out += " | ";
        out += buf;
    }
    return out.empty() ? "unknown OpenSSL error" : out;
}

[[noreturn]] void ThrowLastError(_In_ const std::string& where) {
    throw OpenSslException(where + ": " + GetOpenSslErrors());
}

Bytes ToBytes(_In_ std::string_view s) {
    return Bytes(s.begin(), s.end());
}

std::string ToString(_In_ const Bytes& data) {
    return std::string(reinterpret_cast<const char*>(data.data()), data.size());
}

std::string HexEncode(_In_ const Bytes& data, _In_ bool upper) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    if (upper)
        oss.setf(std::ios::uppercase);
    for (unsigned char c : data) {
        oss << std::setw(2) << static_cast<int>(c);
    }
    return oss.str();
}

Bytes HexDecode(_In_ std::string_view hex) {
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

std::string Base64Encode(_In_ const Bytes& data, _In_ bool noNewLine) {
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* mem = BIO_new(BIO_s_mem());
    if (!b64 || !mem) {
        if (b64)
            BIO_free(b64);
        if (mem)
            BIO_free(mem);
        ThrowLastError("Base64Encode/BIO_new");
    }
    if (noNewLine) {
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

Bytes Base64Decode(_In_ std::string_view base64, _In_ bool noNewLine) {
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* mem = BIO_new_mem_buf(base64.data(), static_cast<int>(base64.size()));
    if (!b64 || !mem) {
        if (b64)
            BIO_free(b64);
        if (mem)
            BIO_free(mem);
        ThrowLastError("Base64Decode/BIO_new");
    }
    if (noNewLine) {
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

std::string BytesToBase64(_In_ const Bytes& data) {
    return Base64Encode(data, true);
}

Bytes Base64ToBytes(_In_ std::string_view s) {
    return Base64Decode(s, true);
}

Bytes RandomBytes(_In_ std::size_t n) {
    Bytes out(n);
    if (n > 0 && RAND_bytes(out.data(), static_cast<int>(n)) != 1) {
        ThrowLastError("RAND_bytes");
    }
    return out;
}

std::string RandomHex(_In_ std::size_t n) {
    return HexEncode(RandomBytes(n));
}

Bytes Md5(_In_ std::string_view s) {
    return DigestImpl(s, EVP_md5());
}

Bytes Sha1(_In_ std::string_view s) {
    return DigestImpl(s, EVP_sha1());
}

Bytes Sha224(_In_ std::string_view s) {
    return DigestImpl(s, EVP_sha224());
}

Bytes Sha256(_In_ std::string_view s) {
    return DigestImpl(s, EVP_sha256());
}

Bytes Sha384(_In_ std::string_view s) {
    return DigestImpl(s, EVP_sha384());
}

Bytes Sha512(_In_ std::string_view s) {
    return DigestImpl(s, EVP_sha512());
}
#if !defined(OPENSSL_NO_SM3)
Bytes Sm3(_In_ std::string_view s) {
    return DigestImpl(s, EVP_sm3());
}
#endif

Bytes HmacMd5(_In_ std::string_view data, _In_ std::string_view key) {
    return HmacImpl(data, key, EVP_md5());
}

Bytes HmacSha1(_In_ std::string_view data, _In_ std::string_view key) {
    return HmacImpl(data, key, EVP_sha1());
}

Bytes HmacSha256(_In_ std::string_view data, _In_ std::string_view key) {
    return HmacImpl(data, key, EVP_sha256());
}

Bytes HmacSha512(_In_ std::string_view data, _In_ std::string_view key) {
    return HmacImpl(data, key, EVP_sha512());
}

Bytes Pbkdf2HmacSha256(_In_ std::string_view password, _In_ const Bytes& salt, _In_ int iterations,
                       _In_ std::size_t outputLength) {
    Bytes out(outputLength);
    if (PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()), salt.empty() ? nullptr : salt.data(),
                          static_cast<int>(salt.size()), iterations, EVP_sha256(), static_cast<int>(outputLength),
                          out.data()) != 1) {
        ThrowLastError("PKCS5_PBKDF2_HMAC");
    }
    return out;
}

Bytes Pbkdf2HmacSha512(_In_ std::string_view password, _In_ const Bytes& salt, _In_ int iterations,
                       _In_ std::size_t outputLength) {
    Bytes out(outputLength);
    if (PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()), salt.empty() ? nullptr : salt.data(),
                          static_cast<int>(salt.size()), iterations, EVP_sha512(), static_cast<int>(outputLength),
                          out.data()) != 1) {
        ThrowLastError("PKCS5_PBKDF2_HMAC(Sha512)");
    }
    return out;
}

Bytes Scrypt(_In_ std::string_view password, _In_ const Bytes& salt, _In_ std::uint64_t N, _In_ std::uint64_t r,
             _In_ std::uint64_t p, _In_ std::uint64_t maximumMemory, _In_ std::size_t outputLength) {
    Bytes out(outputLength);
    if (EVP_PBE_scrypt(password.data(), password.size(), salt.empty() ? nullptr : salt.data(), salt.size(), N, r, p,
                       maximumMemory, out.data(), outputLength) != 1) {
        ThrowLastError("EVP_PBE_scrypt");
    }
    return out;
}

Bytes HkdfSha256(_In_ const Bytes& ikm, _In_ const Bytes& salt, _In_ const Bytes& info, _In_ std::size_t outputLength) {
    EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);
    if (!pctx)
        ThrowLastError("EVP_PKEY_CTX_new_id(HKDF)");

    Bytes out(outputLength);
    if (EVP_PKEY_derive_init(pctx) != 1 || EVP_PKEY_CTX_set_hkdf_md(pctx, EVP_sha256()) != 1 ||
        EVP_PKEY_CTX_set1_hkdf_salt(pctx, salt.empty() ? nullptr : salt.data(), static_cast<int>(salt.size())) != 1 ||
        EVP_PKEY_CTX_set1_hkdf_key(pctx, ikm.empty() ? nullptr : ikm.data(), static_cast<int>(ikm.size())) != 1 ||
        (!info.empty() && EVP_PKEY_CTX_add1_hkdf_info(pctx, info.data(), static_cast<int>(info.size())) != 1)) {
        EVP_PKEY_CTX_free(pctx);
        ThrowLastError("HKDF setup");
    }

    std::size_t len = outputLength;
    if (EVP_PKEY_derive(pctx, out.data(), &len) != 1) {
        EVP_PKEY_CTX_free(pctx);
        ThrowLastError("EVP_PKEY_derive(HKDF)");
    }
    EVP_PKEY_CTX_free(pctx);
    out.resize(len);
    return out;
}

Bytes DeriveAes256KeyFromPasswordPbkdf2Sha256(_In_ std::string_view password, _In_ const Bytes& salt,
                                              _In_ int iterations) {
    return Pbkdf2HmacSha256(password, salt, iterations, 32);
}

Bytes DeriveAes256KeyFromPasswordScrypt(_In_ std::string_view password, _In_ const Bytes& salt, _In_ std::uint64_t N,
                                        _In_ std::uint64_t r, _In_ std::uint64_t p, _In_ std::uint64_t maximumMemory) {
    return Scrypt(password, salt, N, r, p, maximumMemory, 32);
}

PasswordAesKeyInfo DeriveAes256KeyAndIvFromPasswordPbkdf2Sha256(_In_ std::string_view password, _In_ const Bytes& salt,
                                                                _In_ int iterations, _In_ std::size_t ivLength) {
    PasswordAesKeyInfo info;
    info.salt = salt;
    Bytes derived = Pbkdf2HmacSha256(password, salt, iterations, 32 + ivLength);
    info.key.assign(derived.begin(), derived.begin() + 32);
    info.iv.assign(derived.begin() + 32, derived.end());
    return info;
}

CipherPack EncryptWithPasswordAes256GcmPbkdf2(_In_ std::string_view password, _In_ const Bytes& plaintext,
                                              _In_ const Bytes& salt, _In_ int iterations, _In_ const Bytes& aad,
                                              _In_ const Bytes& iv) {
    Bytes key = DeriveAes256KeyFromPasswordPbkdf2Sha256(password, salt, iterations);
    Bytes actualIv = iv.empty() ? RandomBytes(12) : iv;
    Ensure(!actualIv.empty(), "EncryptWithPasswordAes256GcmPbkdf2: iv must not be empty");
    return EncryptAes256Gcm(plaintext, key, actualIv, aad);
}

Bytes DecryptWithPasswordAes256GcmPbkdf2(_In_ std::string_view password, _In_ const CipherPack& pack,
                                         _In_ const Bytes& salt, _In_ int iterations, _In_ const Bytes& aad) {
    Bytes key = DeriveAes256KeyFromPasswordPbkdf2Sha256(password, salt, iterations);
    return DecryptAes256Gcm(pack, key, aad);
}

Bytes EncryptAes128Cbc(_In_ const Bytes& plaintext, _In_ const Bytes& key, _In_ const Bytes& iv) {
    EnsureKeyLength(key, 16, "EncryptAes128Cbc");
    EnsureIvLength(iv, 16, "EncryptAes128Cbc");
    return CipherEncryptRaw(EVP_aes_128_cbc(), plaintext, key, iv, true);
}

Bytes DecryptAes128Cbc(_In_ const Bytes& ciphertext, _In_ const Bytes& key, _In_ const Bytes& iv) {
    EnsureKeyLength(key, 16, "DecryptAes128Cbc");
    EnsureIvLength(iv, 16, "DecryptAes128Cbc");
    return CipherDecryptRaw(EVP_aes_128_cbc(), ciphertext, key, iv, true);
}

Bytes EncryptAes256Cbc(_In_ const Bytes& plaintext, _In_ const Bytes& key, _In_ const Bytes& iv) {
    EnsureKeyLength(key, 32, "EncryptAes256Cbc");
    EnsureIvLength(iv, 16, "EncryptAes256Cbc");
    return CipherEncryptRaw(EVP_aes_256_cbc(), plaintext, key, iv, true);
}

Bytes DecryptAes256Cbc(_In_ const Bytes& ciphertext, _In_ const Bytes& key, _In_ const Bytes& iv) {
    EnsureKeyLength(key, 32, "DecryptAes256Cbc");
    EnsureIvLength(iv, 16, "DecryptAes256Cbc");
    return CipherDecryptRaw(EVP_aes_256_cbc(), ciphertext, key, iv, true);
}

Bytes EncryptAes128Ctr(_In_ const Bytes& plaintext, _In_ const Bytes& key, _In_ const Bytes& iv) {
    EnsureKeyLength(key, 16, "EncryptAes128Ctr");
    EnsureIvLength(iv, 16, "EncryptAes128Ctr");
    return CipherEncryptRaw(EVP_aes_128_ctr(), plaintext, key, iv, false);
}

Bytes DecryptAes128Ctr(_In_ const Bytes& ciphertext, _In_ const Bytes& key, _In_ const Bytes& iv) {
    EnsureKeyLength(key, 16, "DecryptAes128Ctr");
    EnsureIvLength(iv, 16, "DecryptAes128Ctr");
    return CipherDecryptRaw(EVP_aes_128_ctr(), ciphertext, key, iv, false);
}

Bytes EncryptAes256Ctr(_In_ const Bytes& plaintext, _In_ const Bytes& key, _In_ const Bytes& iv) {
    EnsureKeyLength(key, 32, "EncryptAes256Ctr");
    EnsureIvLength(iv, 16, "EncryptAes256Ctr");
    return CipherEncryptRaw(EVP_aes_256_ctr(), plaintext, key, iv, false);
}

Bytes DecryptAes256Ctr(_In_ const Bytes& ciphertext, _In_ const Bytes& key, _In_ const Bytes& iv) {
    EnsureKeyLength(key, 32, "DecryptAes256Ctr");
    EnsureIvLength(iv, 16, "DecryptAes256Ctr");
    return CipherDecryptRaw(EVP_aes_256_ctr(), ciphertext, key, iv, false);
}

CipherPack EncryptAes128Gcm(_In_ const Bytes& plaintext, _In_ const Bytes& key, _In_ const Bytes& iv,
                            _In_ const Bytes& aad) {
    EnsureKeyLength(key, 16, "EncryptAes128Gcm");
    Ensure(!iv.empty(), "EncryptAes128Gcm: iv must not be empty");
    return AeadEncrypt(EVP_aes_128_gcm(), plaintext, key, iv, aad, 16);
}

Bytes DecryptAes128Gcm(_In_ const CipherPack& pack, _In_ const Bytes& key, _In_ const Bytes& aad) {
    EnsureKeyLength(key, 16, "DecryptAes128Gcm");
    Ensure(!pack.iv.empty(), "DecryptAes128Gcm: iv must not be empty");
    Ensure(!pack.tag.empty(), "DecryptAes128Gcm: tag must not be empty");
    return AeadDecrypt(EVP_aes_128_gcm(), pack.ciphertext, key, pack.iv, aad, pack.tag);
}

CipherPack EncryptAes256Gcm(_In_ const Bytes& plaintext, _In_ const Bytes& key, _In_ const Bytes& iv,
                            _In_ const Bytes& aad) {
    EnsureKeyLength(key, 32, "EncryptAes256Gcm");
    Ensure(!iv.empty(), "EncryptAes256Gcm: iv must not be empty");
    return AeadEncrypt(EVP_aes_256_gcm(), plaintext, key, iv, aad, 16);
}

Bytes DecryptAes256Gcm(_In_ const CipherPack& pack, _In_ const Bytes& key, _In_ const Bytes& aad) {
    EnsureKeyLength(key, 32, "DecryptAes256Gcm");
    Ensure(!pack.iv.empty(), "DecryptAes256Gcm: iv must not be empty");
    Ensure(!pack.tag.empty(), "DecryptAes256Gcm: tag must not be empty");
    return AeadDecrypt(EVP_aes_256_gcm(), pack.ciphertext, key, pack.iv, aad, pack.tag);
}

std::string EncryptAes(_In_ std::string text, _In_ std::string password) {
    // password -> 32�ֽ� AES-256 Key
    Bytes key = Sha256(password);

    // ÿ��������� 12 �ֽ� GCM IV
    Bytes iv = RandomBytes(12);

    // string -> Bytes
    Bytes plaintext(text.begin(), text.end());

    CipherPack pack = EncryptAes256Gcm(plaintext, key, iv);

    // ���ո�ʽ��
    // IV(12) + TAG(16) + Ciphertext
    Bytes result;

    result.reserve(pack.iv.size() + pack.tag.size() + pack.ciphertext.size());

    result.insert(result.end(), pack.iv.begin(), pack.iv.end());
    result.insert(result.end(), pack.tag.begin(), pack.tag.end());
    result.insert(result.end(), pack.ciphertext.begin(), pack.ciphertext.end());

    return Base64Encode(result);
}

std::string DecryptAes(_In_ std::string text, _In_ std::string password) {
    Bytes data = Base64Decode(text);

    constexpr std::size_t kIvSize = 12;
    constexpr std::size_t kTagSize = 16;

    if (data.size() < kIvSize + kTagSize)
        throw std::runtime_error("Invalid AES encrypted data");

    // password -> ��ͬ�� 32�ֽ� AES-256 Key
    Bytes key = Sha256(password);

    CipherPack pack;

    pack.iv = Bytes(data.begin(), data.begin() + kIvSize);

    pack.tag = Bytes(data.begin() + kIvSize, data.begin() + kIvSize + kTagSize);

    pack.ciphertext = Bytes(data.begin() + kIvSize + kTagSize, data.end());

    Bytes plaintext = DecryptAes256Gcm(pack, key);

    return std::string(plaintext.begin(), plaintext.end());
}

CipherPack EncryptChaCha20Poly1305(_In_ const Bytes& plaintext, _In_ const Bytes& key, _In_ const Bytes& iv,
                                   _In_ const Bytes& aad) {
    EnsureKeyLength(key, 32, "EncryptChaCha20Poly1305");
    Ensure(!iv.empty(), "EncryptChaCha20Poly1305: iv must not be empty");
    return AeadEncrypt(EVP_chacha20_poly1305(), plaintext, key, iv, aad, 16);
}

Bytes DecryptChaCha20Poly1305(_In_ const CipherPack& pack, _In_ const Bytes& key, _In_ const Bytes& aad) {
    EnsureKeyLength(key, 32, "DecryptChaCha20Poly1305");
    Ensure(!pack.iv.empty(), "DecryptChaCha20Poly1305: iv must not be empty");
    Ensure(!pack.tag.empty(), "DecryptChaCha20Poly1305: tag must not be empty");
    return AeadDecrypt(EVP_chacha20_poly1305(), pack.ciphertext, key, pack.iv, aad, pack.tag);
}

#if !defined(OPENSSL_NO_SM4)
Bytes EncryptSm4Cbc(_In_ const Bytes& plaintext, _In_ const Bytes& key, _In_ const Bytes& iv) {
    EnsureKeyLength(key, 16, "EncryptSm4Cbc");
    EnsureIvLength(iv, 16, "EncryptSm4Cbc");
    return CipherEncryptRaw(EVP_sm4_cbc(), plaintext, key, iv, true);
}

Bytes DecryptSm4Cbc(_In_ const Bytes& ciphertext, _In_ const Bytes& key, _In_ const Bytes& iv) {
    EnsureKeyLength(key, 16, "DecryptSm4Cbc");
    EnsureIvLength(iv, 16, "DecryptSm4Cbc");
    return CipherDecryptRaw(EVP_sm4_cbc(), ciphertext, key, iv, true);
}

Bytes EncryptSm4Ctr(_In_ const Bytes& plaintext, _In_ const Bytes& key, _In_ const Bytes& iv) {
    EnsureKeyLength(key, 16, "EncryptSm4Ctr");
    EnsureIvLength(iv, 16, "EncryptSm4Ctr");
    return CipherEncryptRaw(EVP_sm4_ctr(), plaintext, key, iv, false);
}

Bytes DecryptSm4Ctr(_In_ const Bytes& ciphertext, _In_ const Bytes& key, _In_ const Bytes& iv) {
    EnsureKeyLength(key, 16, "DecryptSm4Ctr");
    EnsureIvLength(iv, 16, "DecryptSm4Ctr");
    return CipherDecryptRaw(EVP_sm4_ctr(), ciphertext, key, iv, false);
}
#endif

PemKeyPair GenerateRsaKeyPair(_In_ int bits) {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (!ctx)
        ThrowLastError("EVP_PKEY_CTX_new_id(RSA)");

    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_keygen_init(ctx) != 1 || EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, bits) != 1 ||
        EVP_PKEY_keygen(ctx, &pkey) != 1) {
        EVP_PKEY_CTX_free(ctx);
        ThrowLastError("GenerateRsaKeyPair");
    }
    EVP_PKEY_CTX_free(ctx);

    PemKeyPair kp{PemFromPublicKey(pkey), PemFromPrivateKey(pkey)};
    EVP_PKEY_free(pkey);
    return kp;
}

PemKeyPair GenerateEcP256KeyPair() {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
    if (!ctx)
        ThrowLastError("EVP_PKEY_CTX_new_id(EC)");

    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_paramgen_init(ctx) != 1 || EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, NID_X9_62_prime256v1) != 1) {
        EVP_PKEY_CTX_free(ctx);
        ThrowLastError("GenerateEcP256KeyPair/paramgen_init");
    }

    EVP_PKEY* params = nullptr;
    if (EVP_PKEY_paramgen(ctx, &params) != 1) {
        EVP_PKEY_CTX_free(ctx);
        ThrowLastError("GenerateEcP256KeyPair/paramgen");
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
        ThrowLastError("GenerateEcP256KeyPair/keygen");
    }
    EVP_PKEY_free(params);
    EVP_PKEY_CTX_free(ctx);

    PemKeyPair kp{PemFromPublicKey(pkey), PemFromPrivateKey(pkey)};
    EVP_PKEY_free(pkey);
    return kp;
}

PemKeyPair GenerateEd25519KeyPair() {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr);
    if (!ctx)
        ThrowLastError("EVP_PKEY_CTX_new_id(ED25519)");

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

std::string PublicKeyPemFromPrivateKeyPem(_In_ const std::string& privateKeyPem) {
    EVP_PKEY* pkey = LoadPrivateKeyFromPem(privateKeyPem);
    std::string pem = PemFromPublicKey(pkey);
    EVP_PKEY_free(pkey);
    return pem;
}

Bytes EncryptRsaOaepSha256(_In_ const std::string& publicKeyPem, _In_ const Bytes& plaintext) {
    EVP_PKEY* pkey = LoadPublicKeyFromPem(publicKeyPem);
    Bytes out = PKeyEncrypt(pkey, plaintext, RSA_PKCS1_OAEP_PADDING, EVP_sha256());
    EVP_PKEY_free(pkey);
    return out;
}

Bytes DecryptRsaOaepSha256(_In_ const std::string& privateKeyPem, _In_ const Bytes& ciphertext) {
    EVP_PKEY* pkey = LoadPrivateKeyFromPem(privateKeyPem);
    Bytes out = PKeyDecrypt(pkey, ciphertext, RSA_PKCS1_OAEP_PADDING, EVP_sha256());
    EVP_PKEY_free(pkey);
    return out;
}

Bytes SignRsaPssSha256(_In_ const std::string& privateKeyPem, _In_ const Bytes& data) {
    EVP_PKEY* pkey = LoadPrivateKeyFromPem(privateKeyPem);
    Bytes sig = DigestSign(pkey, data, EVP_sha256(), RSA_PKCS1_PSS_PADDING, -1);
    EVP_PKEY_free(pkey);
    return sig;
}

bool VerifyRsaPssSha256(_In_ const std::string& publicKeyPem, _In_ const Bytes& data, _In_ const Bytes& sig) {
    EVP_PKEY* pkey = LoadPublicKeyFromPem(publicKeyPem);
    bool ok = DigestVerify(pkey, data, sig, EVP_sha256(), RSA_PKCS1_PSS_PADDING, -1);
    EVP_PKEY_free(pkey);
    return ok;
}

Bytes SignRsaPkcs1V15Sha256(_In_ const std::string& privateKeyPem, _In_ const Bytes& data) {
    EVP_PKEY* pkey = LoadPrivateKeyFromPem(privateKeyPem);
    Bytes sig = DigestSign(pkey, data, EVP_sha256(), RSA_PKCS1_PADDING, -1);
    EVP_PKEY_free(pkey);
    return sig;
}

bool VerifyRsaPkcs1V15Sha256(_In_ const std::string& publicKeyPem, _In_ const Bytes& data, _In_ const Bytes& sig) {
    EVP_PKEY* pkey = LoadPublicKeyFromPem(publicKeyPem);
    bool ok = DigestVerify(pkey, data, sig, EVP_sha256(), RSA_PKCS1_PADDING, -1);
    EVP_PKEY_free(pkey);
    return ok;
}

Bytes SignEcdsaP256Sha256(_In_ const std::string& privateKeyPem, _In_ const Bytes& data) {
    EVP_PKEY* pkey = LoadPrivateKeyFromPem(privateKeyPem);
    Bytes sig = DigestSign(pkey, data, EVP_sha256());
    EVP_PKEY_free(pkey);
    return sig;
}

bool VerifyEcdsaP256Sha256(_In_ const std::string& publicKeyPem, _In_ const Bytes& data, _In_ const Bytes& sig) {
    EVP_PKEY* pkey = LoadPublicKeyFromPem(publicKeyPem);
    bool ok = DigestVerify(pkey, data, sig, EVP_sha256());
    EVP_PKEY_free(pkey);
    return ok;
}

Bytes SignEd25519(_In_ const std::string& privateKeyPem, _In_ const Bytes& data) {
    EVP_PKEY* pkey = LoadPrivateKeyFromPem(privateKeyPem);
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

    std::size_t signatureLength = 0;
    if (EVP_DigestSign(ctx, nullptr, &signatureLength, data.empty() ? nullptr : data.data(), data.size()) != 1) {
        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        ThrowLastError("EVP_DigestSign(size, Ed25519)");
    }
    Bytes sig(signatureLength);
    if (EVP_DigestSign(ctx, sig.data(), &signatureLength, data.empty() ? nullptr : data.data(), data.size()) != 1) {
        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        ThrowLastError("EVP_DigestSign(Ed25519)");
    }
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    sig.resize(signatureLength);
    return sig;
}

bool VerifyEd25519(_In_ const std::string& publicKeyPem, _In_ const Bytes& data, _In_ const Bytes& sig) {
    EVP_PKEY* pkey = LoadPublicKeyFromPem(publicKeyPem);
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
    int ret = EVP_DigestVerify(ctx, sig.empty() ? nullptr : sig.data(), sig.size(),
                               data.empty() ? nullptr : data.data(), data.size());
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    if (ret == 1)
        return true;
    if (ret == 0)
        return false;
    ThrowLastError("EVP_DigestVerify(Ed25519)");
    return false;
}

#if !defined(OPENSSL_NO_SM2)
Bytes SignSm2Sm3(_In_ const std::string& privateKeyPem, _In_ const Bytes& data, _In_ std::string_view userId) {
    EVP_PKEY* pkey = LoadPrivateKeyFromPem(privateKeyPem);
    EVP_MD_CTX* mctx = EVP_MD_CTX_new();
    if (!mctx) {
        EVP_PKEY_free(pkey);
        ThrowLastError("EVP_MD_CTX_new(SM2 sign)");
    }
    EVP_PKEY_CTX* pctx = nullptr;
    PrepareSM2ForSignVerify(pkey, mctx, &pctx, userId.data(), userId.size());

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
    std::size_t signatureLength = 0;
    if (EVP_DigestSignFinal(mctx, nullptr, &signatureLength) != 1) {
        EVP_PKEY_CTX_free(pctx);
        EVP_MD_CTX_free(mctx);
        EVP_PKEY_free(pkey);
        ThrowLastError("EVP_DigestSignFinal(size, SM2)");
    }
    Bytes sig(signatureLength);
    if (EVP_DigestSignFinal(mctx, sig.data(), &signatureLength) != 1) {
        EVP_PKEY_CTX_free(pctx);
        EVP_MD_CTX_free(mctx);
        EVP_PKEY_free(pkey);
        ThrowLastError("EVP_DigestSignFinal(SM2)");
    }
    sig.resize(signatureLength);
    EVP_PKEY_CTX_free(pctx);
    EVP_MD_CTX_free(mctx);
    EVP_PKEY_free(pkey);
    return sig;
}

bool VerifySm2Sm3(_In_ const std::string& publicKeyPem, _In_ const Bytes& data, _In_ const Bytes& sig,
                  _In_ std::string_view userId) {
    EVP_PKEY* pkey = LoadPublicKeyFromPem(publicKeyPem);
    EVP_MD_CTX* mctx = EVP_MD_CTX_new();
    if (!mctx) {
        EVP_PKEY_free(pkey);
        ThrowLastError("EVP_MD_CTX_new(SM2 verify)");
    }
    EVP_PKEY_CTX* pctx = nullptr;
    PrepareSM2ForSignVerify(pkey, mctx, &pctx, userId.data(), userId.size());

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
    int ret = EVP_DigestVerifyFinal(mctx, sig.empty() ? nullptr : const_cast<unsigned char*>(sig.data()), sig.size());
    EVP_PKEY_CTX_free(pctx);
    EVP_MD_CTX_free(mctx);
    EVP_PKEY_free(pkey);
    if (ret == 1)
        return true;
    if (ret == 0)
        return false;
    ThrowLastError("EVP_DigestVerifyFinal(SM2)");
    return false;
}
#endif

FileEncryptResult FileCrypto::EncryptFileAes256Gcm(_In_ const std::string& inputPath,
                                                   _In_ const std::string& outputPath, _In_ const Bytes& key,
                                                   _In_ const Bytes& iv, _In_ const Bytes& aad,
                                                   _In_ std::size_t chunkSize) {
    return EncryptFileAes256GcmCore(inputPath, outputPath, key, iv, aad, chunkSize);
}

void FileCrypto::DecryptFileAes256Gcm(_In_ const std::string& inputPath, _In_ const std::string& outputPath,
                                      _In_ const Bytes& key, _In_ const Bytes& iv, _In_ const Bytes& tag,
                                      _In_ const Bytes& aad, _In_ std::size_t chunkSize) {
    EnsureNotEmptyPath(inputPath, "FileCrypto::DecryptFileAes256Gcm");
    std::ifstream ifs(inputPath, std::ios::binary);
    if (!ifs)
        throw OpenSslException("FileCrypto::DecryptFileAes256Gcm: cannot open input file: " + inputPath);
    std::uint64_t fileSize = GetFileSize(ifs);
    ifs.seekg(0, std::ios::beg);
    DecryptFileAes256GcmCore(ifs, fileSize, outputPath, key, iv, tag, aad, chunkSize);
}

PasswordFileEncryptResult FileCrypto::EncryptFileWithPasswordAes256GcmPbkdf2(
    _In_ const std::string& inputPath, _In_ const std::string& outputPath, _In_ std::string_view password,
    _In_ const Bytes& salt, _In_ int iterations, _In_ const Bytes& aad, _In_ const Bytes& iv,
    _In_ std::size_t chunkSize) {
    PasswordFileEncryptResult result;
    result.salt = salt.empty() ? RandomBytes(16) : salt;
    result.iterations = iterations;
    Bytes key = DeriveAes256KeyFromPasswordPbkdf2Sha256(password, result.salt, iterations);
    Bytes actualIv = iv.empty() ? RandomBytes(12) : iv;
    FileEncryptResult base = EncryptFileAes256GcmCore(inputPath, outputPath, key, actualIv, aad, chunkSize);
    result.iv = std::move(base.iv);
    result.tag = std::move(base.tag);
    result.plainSize = base.plainSize;
    result.cipherSize = base.cipherSize;
    return result;
}

void FileCrypto::DecryptFileWithPasswordAes256GcmPbkdf2(_In_ const std::string& inputPath,
                                                        _In_ const std::string& outputPath,
                                                        _In_ std::string_view password, _In_ const Bytes& salt,
                                                        _In_ int iterations, _In_ const Bytes& iv,
                                                        _In_ const Bytes& tag, _In_ const Bytes& aad,
                                                        _In_ std::size_t chunkSize) {
    EnsureNotEmptyPath(inputPath, "FileCrypto::DecryptFileWithPasswordAes256GcmPbkdf2");
    Bytes key = DeriveAes256KeyFromPasswordPbkdf2Sha256(password, salt, iterations);
    std::ifstream ifs(inputPath, std::ios::binary);
    if (!ifs)
        throw OpenSslException("FileCrypto::DecryptFileWithPasswordAes256GcmPbkdf2: cannot open input file: " +
                               inputPath);
    std::uint64_t fileSize = GetFileSize(ifs);
    ifs.seekg(0, std::ios::beg);
    DecryptFileAes256GcmCore(ifs, fileSize, outputPath, key, iv, tag, aad, chunkSize);
}

void FileCrypto::EncryptFileToContainerWithPasswordAes256GcmPbkdf2(_In_ const std::string& inputPath,
                                                                   _In_ const std::string& containerOutputPath,
                                                                   _In_ std::string_view password,
                                                                   _In_ const Bytes& salt, _In_ int iterations,
                                                                   _In_ std::size_t chunkSize) {
    Bytes actualSalt = salt.empty() ? RandomBytes(16) : salt;
    Bytes iv = RandomBytes(12);
    Bytes key = DeriveAes256KeyFromPasswordPbkdf2Sha256(password, actualSalt, iterations);

    EnsureNotEmptyPath(inputPath, "FileCrypto::EncryptFileToContainerWithPasswordAes256GcmPbkdf2");
    EnsureNotEmptyPath(containerOutputPath, "FileCrypto::EncryptFileToContainerWithPasswordAes256GcmPbkdf2");

    std::ifstream ifs(inputPath, std::ios::binary);
    if (!ifs)
        throw OpenSslException(
            "FileCrypto::EncryptFileToContainerWithPasswordAes256GcmPbkdf2: cannot open input file: " + inputPath);
    std::uint64_t plainSize = GetFileSize(ifs);
    ifs.seekg(0, std::ios::beg);

    std::ofstream ofs(containerOutputPath, std::ios::binary | std::ios::trunc);
    if (!ofs)
        throw OpenSslException(
            "FileCrypto::EncryptFileToContainerWithPasswordAes256GcmPbkdf2: cannot open output file: " +
            containerOutputPath);

    PasswordContainerHeaderV1 header{};
    std::memcpy(header.magic, kContainerMagic, sizeof(kContainerMagic));
    header.version = kContainerVersion;
    header.kdfId = kContainerKdfPbkdf2Sha256;
    header.cipherId = kContainerCipherAes256Gcm;
    header.iterations = static_cast<std::uint32_t>(iterations);
    header.saltLength = static_cast<std::uint32_t>(actualSalt.size());
    header.ivLength = static_cast<std::uint32_t>(iv.size());
    header.tagLength = 16;
    header.plainSize = plainSize;
    WriteContainerHeader(ofs, header);
    ofs.write(reinterpret_cast<const char*>(actualSalt.data()), static_cast<std::streamsize>(actualSalt.size()));
    ofs.write(reinterpret_cast<const char*>(iv.data()), static_cast<std::streamsize>(iv.size()));
    if (!ofs)
        throw OpenSslException(
            "FileCrypto::EncryptFileToContainerWithPasswordAes256GcmPbkdf2: failed to write container header body");

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        ThrowLastError("EVP_CIPHER_CTX_new(container encrypt)");

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("Container encrypt/EVP_EncryptInit_ex(phase1)");
    }
    if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("Container encrypt/EVP_EncryptInit_ex(phase2)");
    }

    Bytes inputBuffer(chunkSize);
    Bytes outputBuffer(chunkSize + 32);
    int len = 0;
    while (ifs) {
        ifs.read(reinterpret_cast<char*>(inputBuffer.data()), static_cast<std::streamsize>(inputBuffer.size()));
        std::streamsize got = ifs.gcount();
        if (got <= 0)
            break;
        if (EVP_EncryptUpdate(ctx, outputBuffer.data(), &len, inputBuffer.data(), static_cast<int>(got)) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            ThrowLastError("Container encrypt/EncryptUpdate");
        }
        if (len > 0) {
            ofs.write(reinterpret_cast<const char*>(outputBuffer.data()), len);
            if (!ofs) {
                EVP_CIPHER_CTX_free(ctx);
                throw OpenSslException(
                    "FileCrypto::EncryptFileToContainerWithPasswordAes256GcmPbkdf2: failed to write ciphertext");
            }
        }
    }

    if (EVP_EncryptFinal_ex(ctx, outputBuffer.data(), &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        ThrowLastError("Container encrypt/EncryptFinal");
    }
    if (len > 0) {
        ofs.write(reinterpret_cast<const char*>(outputBuffer.data()), len);
        if (!ofs) {
            EVP_CIPHER_CTX_free(ctx);
            throw OpenSslException("FileCrypto::EncryptFileToContainerWithPasswordAes256GcmPbkdf2: failed to write "
                                   "final ciphertext block");
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
        throw OpenSslException("FileCrypto::EncryptFileToContainerWithPasswordAes256GcmPbkdf2: failed to write tag");
    }
}

void FileCrypto::DecryptFileFromContainerWithPasswordAes256GcmPbkdf2(_In_ const std::string& containerInputPath,
                                                                     _In_ const std::string& outputPath,
                                                                     _In_ std::string_view password,
                                                                     _In_ std::size_t chunkSize) {
    EnsureNotEmptyPath(containerInputPath, "FileCrypto::DecryptFileFromContainerWithPasswordAes256GcmPbkdf2");
    std::ifstream ifs(containerInputPath, std::ios::binary);
    if (!ifs)
        throw OpenSslException(
            "FileCrypto::DecryptFileFromContainerWithPasswordAes256GcmPbkdf2: cannot open input file: " +
            containerInputPath);

    PasswordContainerHeaderV1 header = ReadContainerHeader(ifs);
    Bytes salt(header.saltLength);
    Bytes iv(header.ivLength);
    Bytes tag(header.tagLength);

    ifs.read(reinterpret_cast<char*>(salt.data()), static_cast<std::streamsize>(salt.size()));
    ifs.read(reinterpret_cast<char*>(iv.data()), static_cast<std::streamsize>(iv.size()));
    if (!ifs) {
        throw OpenSslException(
            "FileCrypto::DecryptFileFromContainerWithPasswordAes256GcmPbkdf2: failed to read salt/iv");
    }

    std::uint64_t totalSize = GetFileSize(ifs);
    std::uint64_t metadataSize = sizeof(PasswordContainerHeaderV1) + salt.size() + iv.size() + tag.size();
    if (totalSize < metadataSize) {
        throw OpenSslException(
            "FileCrypto::DecryptFileFromContainerWithPasswordAes256GcmPbkdf2: invalid container size");
    }
    std::uint64_t cipherSize = totalSize - metadataSize;

    auto tagPosition = static_cast<std::streamoff>(totalSize - tag.size());
    ifs.seekg(tagPosition, std::ios::beg);
    ifs.read(reinterpret_cast<char*>(tag.data()), static_cast<std::streamsize>(tag.size()));
    if (!ifs) {
        throw OpenSslException("FileCrypto::DecryptFileFromContainerWithPasswordAes256GcmPbkdf2: failed to read tag");
    }

    Bytes key = DeriveAes256KeyFromPasswordPbkdf2Sha256(password, salt, static_cast<int>(header.iterations));

    auto cipherPosition = static_cast<std::streamoff>(sizeof(PasswordContainerHeaderV1) + salt.size() + iv.size());
    ifs.clear();
    ifs.seekg(cipherPosition, std::ios::beg);
    DecryptFileAes256GcmCore(ifs, cipherSize, outputPath, key, iv, tag, {}, chunkSize);
}

} // namespace ytpp::sys_core::encryption
