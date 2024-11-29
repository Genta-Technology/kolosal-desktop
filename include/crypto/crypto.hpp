#pragma once

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <vector>
#include <array>
#include <string>

// TODO: use password-based key derivation function (PBKDF2) to generate key from password
//       to be more secure.

class Crypto
{
public:
    static constexpr size_t IV_SIZE = 12;
    static constexpr size_t TAG_SIZE = 16;
    static constexpr size_t KEY_SIZE = 32;

    static std::array<uint8_t, KEY_SIZE> generateKey()
    {
        // Get the unique identifier for the device
        std::string deviceId = getUniqueDeviceIdentifier();

        // Hash the unique identifier to generate a key
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, deviceId.c_str(), deviceId.size());
        SHA256_Final(hash, &sha256);

        // Copy the first KEY_SIZE bytes into the key array
        std::array<uint8_t, KEY_SIZE> key;
        std::copy(hash, hash + KEY_SIZE, key.begin());

        return key;
    }

    static std::string getUniqueDeviceIdentifier()
    {
#ifdef _WIN32
		// Windows-specific code to get the GUID of the machine
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ | KEY_WOW64_64KEY, &hKey) != ERROR_SUCCESS)
        {
            throw std::runtime_error("Failed to open registry key");
        }

        char value[256];
        DWORD size = sizeof(value);
        if (RegQueryValueExA(hKey, "MachineGuid", nullptr, nullptr, (LPBYTE)value, &size) != ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            throw std::runtime_error("Failed to read MachineGuid");
        }
        RegCloseKey(hKey);

        return std::string(value, size - 1); // Exclude the null terminator
#else
        // Linux/Unix-specific code to get the MAC address
        struct ifaddrs* ifaddr, * ifa;
        if (getifaddrs(&ifaddr) == -1)
        {
            throw std::runtime_error("Failed to get network interfaces");
        }

        std::string macAddress;
        for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
        {
            if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_PACKET)
            {
                struct sockaddr_ll* s = (struct sockaddr_ll*)ifa->ifa_addr;
                if (s->sll_halen)
                {
                    for (int i = 0; i < s->sll_halen; ++i)
                    {
                        macAddress += static_cast<char>(s->sll_addr[i]);
                    }
                    break;
                }
            }
        }

        freeifaddrs(ifaddr);
        return macAddress;
#endif
    }

    static std::vector<uint8_t> encrypt(
        const std::vector<uint8_t>& plaintext,
        const std::array<uint8_t, KEY_SIZE>& key
    )
    {
        std::vector<uint8_t> iv(IV_SIZE);
        if (RAND_bytes(iv.data(), IV_SIZE) != 1)
        {
            throw std::runtime_error("Failed to generate IV");
        }

        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx)
        {
            throw std::runtime_error("Failed to create cipher context");
        }

        std::vector<uint8_t> encrypted;
        try
        {
            if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, key.data(), iv.data()) != 1)
            {
                throw std::runtime_error("Failed to initialize encryption");
            }

            std::vector<uint8_t> ciphertext(plaintext.size() + EVP_MAX_BLOCK_LENGTH);
            int len = 0, ciphertext_len = 0;

            if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len,
                plaintext.data(), plaintext.size()) != 1)
            {
                throw std::runtime_error("Failed to encrypt data");
            }
            ciphertext_len = len;

            if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1)
            {
                throw std::runtime_error("Failed to finalize encryption");
            }
            ciphertext_len += len;

            std::vector<uint8_t> tag(TAG_SIZE);
            if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_SIZE, tag.data()) != 1)
            {
                throw std::runtime_error("Failed to get tag");
            }

            // Format: IV || Ciphertext || Tag
            encrypted.reserve(IV_SIZE + ciphertext_len + TAG_SIZE);
            encrypted.insert(encrypted.end(), iv.begin(), iv.end());
            encrypted.insert(encrypted.end(), ciphertext.begin(), ciphertext.begin() + ciphertext_len);
            encrypted.insert(encrypted.end(), tag.begin(), tag.end());
        }
        catch (...)
        {
            EVP_CIPHER_CTX_free(ctx);
            throw;
        }

        EVP_CIPHER_CTX_free(ctx);
        return encrypted;
    }

    static std::vector<uint8_t> decrypt(
        const std::vector<uint8_t>& encrypted,
        const std::array<uint8_t, KEY_SIZE>& key
    )
    {
        if (encrypted.size() < IV_SIZE + TAG_SIZE)
        {
            throw std::runtime_error("Invalid encrypted data size");
        }

        std::vector<uint8_t> iv(encrypted.begin(), encrypted.begin() + IV_SIZE);
        std::vector<uint8_t> tag(encrypted.end() - TAG_SIZE, encrypted.end());
        std::vector<uint8_t> ciphertext(encrypted.begin() + IV_SIZE,
            encrypted.end() - TAG_SIZE);

        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx)
        {
            throw std::runtime_error("Failed to create cipher context");
        }

        std::vector<uint8_t> decrypted;
        try
        {
            if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, key.data(), iv.data()) != 1)
            {
                throw std::runtime_error("Failed to initialize decryption");
            }

            std::vector<uint8_t> plaintext(ciphertext.size());
            int len = 0, plaintext_len = 0;

            if (EVP_DecryptUpdate(ctx, plaintext.data(), &len,
                ciphertext.data(), ciphertext.size()) != 1)
            {
                throw std::runtime_error("Failed to decrypt data");
            }
            plaintext_len = len;

            if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, TAG_SIZE, tag.data()) != 1)
            {
                throw std::runtime_error("Failed to set tag");
            }

            if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1)
            {
                throw std::runtime_error("Failed to verify tag or finalize decryption");
            }
            plaintext_len += len;

            decrypted = std::vector<uint8_t>(plaintext.begin(), plaintext.begin() + plaintext_len);
        }
        catch (...)
        {
            EVP_CIPHER_CTX_free(ctx);
            throw;
        }

        EVP_CIPHER_CTX_free(ctx);
        return decrypted;
    }
};