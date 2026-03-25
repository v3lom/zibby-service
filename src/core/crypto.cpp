#include "core/crypto.h"

#include <boost/filesystem.hpp>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>

#include <array>
#include <vector>

namespace fs = boost::filesystem;

namespace zibby::core {

namespace {

std::array<unsigned char, 32> deriveKey(const std::string& secret) {
    std::array<unsigned char, 32> key{};
    SHA256(reinterpret_cast<const unsigned char*>(secret.data()), secret.size(), key.data());
    return key;
}

std::string encodeBase64(const std::vector<unsigned char>& binary) {
    if (binary.empty()) {
        return {};
    }

    std::string out;
    out.resize(4 * ((binary.size() + 2) / 3));
    const int len = EVP_EncodeBlock(
        reinterpret_cast<unsigned char*>(out.data()),
        binary.data(),
        static_cast<int>(binary.size()));
    if (len <= 0) {
        return {};
    }
    out.resize(static_cast<std::size_t>(len));
    return out;
}

std::vector<unsigned char> decodeBase64(const std::string& text) {
    if (text.empty()) {
        return {};
    }

    std::vector<unsigned char> out((text.size() * 3) / 4 + 1, 0);
    const int len = EVP_DecodeBlock(out.data(), reinterpret_cast<const unsigned char*>(text.data()), static_cast<int>(text.size()));
    if (len < 0) {
        return {};
    }

    std::size_t padding = 0;
    if (!text.empty() && text.back() == '=') {
        padding++;
        if (text.size() > 1 && text[text.size() - 2] == '=') {
            padding++;
        }
    }

    out.resize(static_cast<std::size_t>(len) - padding);
    return out;
}

} // namespace

bool Crypto::ensureUserKeys(const std::string& dataDir) const {
    fs::create_directories(dataDir);

    const fs::path privateKeyPath = fs::path(dataDir) / "private.pem";
    const fs::path publicKeyPath = fs::path(dataDir) / "public.pem";

    if (fs::exists(privateKeyPath) && fs::exists(publicKeyPath)) {
        return true;
    }

    EVP_PKEY* pkey = EVP_PKEY_new();
    RSA* rsa = RSA_new();
    BIGNUM* exponent = BN_new();
    if (pkey == nullptr || rsa == nullptr || exponent == nullptr) {
        EVP_PKEY_free(pkey);
        RSA_free(rsa);
        BN_free(exponent);
        return false;
    }

    BN_set_word(exponent, RSA_F4);
    if (RSA_generate_key_ex(rsa, 2048, exponent, nullptr) != 1) {
        EVP_PKEY_free(pkey);
        RSA_free(rsa);
        BN_free(exponent);
        return false;
    }

    EVP_PKEY_assign_RSA(pkey, rsa);
    rsa = nullptr;

    FILE* privateFile = fopen(privateKeyPath.string().c_str(), "wb");
    FILE* publicFile = fopen(publicKeyPath.string().c_str(), "wb");
    if (privateFile == nullptr || publicFile == nullptr) {
        if (privateFile != nullptr) {
            fclose(privateFile);
        }
        if (publicFile != nullptr) {
            fclose(publicFile);
        }
        EVP_PKEY_free(pkey);
        BN_free(exponent);
        return false;
    }

    const bool privateOk = PEM_write_PrivateKey(privateFile, pkey, nullptr, nullptr, 0, nullptr, nullptr) == 1;
    const bool publicOk = PEM_write_PUBKEY(publicFile, pkey) == 1;

    fclose(privateFile);
    fclose(publicFile);

    EVP_PKEY_free(pkey);
    BN_free(exponent);

    return privateOk && publicOk;
}

std::string Crypto::encryptForStorage(const std::string& plaintext, const std::string& secret) const {
    if (plaintext.empty() || secret.empty()) {
        return {};
    }

    const auto key = deriveKey(secret);
    std::array<unsigned char, 16> iv{};
    if (RAND_bytes(iv.data(), static_cast<int>(iv.size())) != 1) {
        return {};
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr) {
        return {};
    }

    std::vector<unsigned char> encrypted(plaintext.size() + EVP_MAX_BLOCK_LENGTH);
    int outLen = 0;
    int totalLen = 0;

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key.data(), iv.data()) != 1
        || EVP_EncryptUpdate(
               ctx,
               encrypted.data(),
               &outLen,
               reinterpret_cast<const unsigned char*>(plaintext.data()),
               static_cast<int>(plaintext.size()))
            != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    totalLen += outLen;
    if (EVP_EncryptFinal_ex(ctx, encrypted.data() + totalLen, &outLen) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    totalLen += outLen;
    encrypted.resize(static_cast<std::size_t>(totalLen));
    EVP_CIPHER_CTX_free(ctx);

    std::vector<unsigned char> payload;
    payload.reserve(iv.size() + encrypted.size());
    payload.insert(payload.end(), iv.begin(), iv.end());
    payload.insert(payload.end(), encrypted.begin(), encrypted.end());
    return encodeBase64(payload);
}

std::string Crypto::decryptFromStorage(const std::string& ciphertextBase64, const std::string& secret) const {
    if (ciphertextBase64.empty() || secret.empty()) {
        return {};
    }

    auto payload = decodeBase64(ciphertextBase64);
    if (payload.size() <= 16) {
        return {};
    }

    const auto key = deriveKey(secret);
    const unsigned char* iv = payload.data();
    const unsigned char* encrypted = payload.data() + 16;
    const int encryptedSize = static_cast<int>(payload.size() - 16);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr) {
        return {};
    }

    std::vector<unsigned char> decrypted(static_cast<std::size_t>(encryptedSize) + EVP_MAX_BLOCK_LENGTH);
    int outLen = 0;
    int totalLen = 0;

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key.data(), iv) != 1
        || EVP_DecryptUpdate(ctx, decrypted.data(), &outLen, encrypted, encryptedSize) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    totalLen += outLen;
    if (EVP_DecryptFinal_ex(ctx, decrypted.data() + totalLen, &outLen) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    totalLen += outLen;
    EVP_CIPHER_CTX_free(ctx);

    return std::string(reinterpret_cast<const char*>(decrypted.data()), static_cast<std::size_t>(totalLen));
}

} // namespace zibby::core
