#pragma once

#include "chat_history.hpp"
#include "crypto/crypto.hpp"

#include <future>
#include <shared_mutex>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace Chat
{
    /**
     * @brief Interface for chat persistence strategies
     */
    class IChatPersistence 
    {
    public:
        virtual ~IChatPersistence() = default;
        virtual std::future<bool> saveChat(const ChatHistory& chat) = 0;
        virtual std::future<bool> deleteChat(const std::string& chatName) = 0;
        virtual std::future<std::vector<ChatHistory>> loadAllChats() = 0;
    };

    /**
     * @brief File-based chat persistence implementation using AES-GCM encryption
     */
    class FileChatPersistence : public IChatPersistence 
    {
    public:
        explicit FileChatPersistence(std::string basePath, std::array<uint8_t, 32> key)
            : m_basePath(std::move(basePath)), m_key(key) 
        {
			// Create base path if it doesn't exist
			if (!std::filesystem::exists(m_basePath))
			{
				std::filesystem::create_directory(m_basePath);
			}
        }

        std::future<bool> saveChat(const ChatHistory& chat) override 
        {
            return std::async(std::launch::async, [this, chat]() {
                std::unique_lock<std::shared_mutex> lock(m_ioMutex);
                return saveEncryptedChat(chat);
                });
        }

        std::future<bool> deleteChat(const std::string& chatName) override 
        {
            return std::async(std::launch::async, [this, chatName]() {
                std::unique_lock<std::shared_mutex> lock(m_ioMutex);
                try 
                {
                    std::filesystem::remove(getChatPath(chatName));
                    return true;
                }
                catch (...) 
                {
                    return false;
                }
                });
        }

        std::future<std::vector<ChatHistory>> loadAllChats() override 
        {
            return std::async(std::launch::async, [this]() {
                std::shared_lock<std::shared_mutex> lock(m_ioMutex);
                return loadEncryptedChats();
                });
        }

    private:
        const std::string m_basePath;
        const std::array<uint8_t, 32> m_key;
        mutable std::shared_mutex m_ioMutex;

        auto getChatPath(const std::string& chatName) const -> std::string 
        {
            return (std::filesystem::path(m_basePath) / (chatName + ".chat")).string();
        }

        bool saveEncryptedChat(const ChatHistory& chat) 
        {
            try {
                // Use the existing to_json serialization
                nlohmann::json chatJson;
                to_json(chatJson, chat);

                // Convert JSON to binary
                std::string jsonStr = chatJson.dump();
                std::vector<uint8_t> plaintext(jsonStr.begin(), jsonStr.end());

                // Encrypt the data
                auto encrypted = Crypto::encrypt(plaintext, m_key);

                // Save to file
				std::string chatPath = getChatPath(chat.name);
                std::ofstream file(chatPath, std::ios::binary);
                if (!file) {
                    return false;
                }

                file.write(reinterpret_cast<const char*>(encrypted.data()),
                    encrypted.size());

                return true;
            }
            catch (const std::exception& e) {
                // TODO: Log error details here
                return false;
            }
        }

        std::vector<ChatHistory> loadEncryptedChats() 
        {
            std::vector<ChatHistory> chats;

            try {
                for (const auto& entry : std::filesystem::directory_iterator(m_basePath)) {
                    if (entry.path().extension() == ".chat") {
                        // Read encrypted file
                        std::ifstream file(entry.path(), std::ios::binary);
                        if (!file) continue;

                        std::vector<uint8_t> encrypted(
                            (std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>()
                        );

                        // Decrypt the data
                        auto plaintext = Crypto::decrypt(encrypted, m_key);

                        // Parse JSON using the existing from_json serialization
                        std::string jsonStr(plaintext.begin(), plaintext.end());
                        auto chatJson = nlohmann::json::parse(jsonStr);

                        ChatHistory chat;
                        from_json(chatJson, chat);
                        chats.push_back(std::move(chat));
                    }
                }
            }
            catch (const std::exception& e) {
                // TODO: Log error details here
            }

            return chats;
        }
    };

} // namespace Chat