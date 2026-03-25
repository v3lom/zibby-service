#include "core/crypto.h"

#include <boost/filesystem.hpp>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

namespace fs = boost::filesystem;

namespace zibby::core {

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

} // namespace zibby::core
