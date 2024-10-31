/*
 * Copyright (c) 2024, Rifky Bujana Bisri.  All rights reserved.
 *
 * This file is part of Genta Technology.
 *
 * Developed by Genta Technology Team.
 * This product includes software developed by the Genta Technology Team.
 *
 *     https://genta.tech
 *
 * See the COPYRIGHT file at the top-level directory of this distribution
 * for details of code ownership.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "kolosal.h"
#include <iostream>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <array>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <fstream>
#include <ctime>
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"

using json = json;

//-----------------------------------------------------------------------------
// [SECTION] Constants and Configurations
//-----------------------------------------------------------------------------

namespace Config
{
    // Using the same constants as defined in kolosal.h
    // Ensuring consistency between header and source files
    using namespace ::Config; // Importing all from Config namespace
}

//-----------------------------------------------------------------------------
// [SECTION] Global Variables
//-----------------------------------------------------------------------------

MarkdownFonts g_mdFonts;
IconFonts g_iconFonts;
std::unique_ptr<ChatManager> g_chatManager;
std::unique_ptr<PresetManager> g_presetManager;
// idk if this is the right way to do it
// but it's the only way I can think of
bool ModelPresetSidebar::State::g_showSaveAsDialog = false;
char ModelPresetSidebar::State::g_newPresetName[256] = "";

//-----------------------------------------------------------------------------
// [SECTION] ChatManager Class Implementations
//-----------------------------------------------------------------------------

/**
 * @brief Constructs a new ChatManager object.
 *
 * @param chatsDirectory The directory where chats are stored.
 */
ChatManager::ChatManager(const std::string &chatsDirectory)
    : chatsPath(chatsDirectory), currentChatIndex(0), hasInitialized(false)
{
    createChatsDirectoryIfNotExists();
    if (!loadChats())
    {
        std::cerr << "Failed to load chats" << std::endl;
    }
}

/**
 * @brief Creates the chats directory if it does not exist.
 */
void ChatManager::createChatsDirectoryIfNotExists()
{
    try
    {
        if (!std::filesystem::exists(chatsPath))
        {
            std::filesystem::create_directories(chatsPath);
        }
        // Test file write permissions
        std::string testPath = chatsPath + "/test.txt";
        std::ofstream test(testPath);
        if (!test.is_open())
        {
            std::cerr << "Cannot write to chats directory" << std::endl;
        }
        else
        {
            test.close();
            std::filesystem::remove(testPath);
        }
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        std::cerr << "Error with chats directory: " << e.what() << std::endl;
        throw;
    }
}

/**
 * @brief Initializes the default chat history.
 */
void ChatManager::initializeDefaultChatHistory()
{
    defaultChatHistory = ChatHistory(
        0,
        static_cast<int>(std::time(nullptr)),
        "untitled",
        {} // Empty messages vector
    );
}

/**
 * @brief Gets the default chat histories.
 *
 * @return std::vector<ChatHistory> A vector containing the default chat history.
 */
auto ChatManager::getDefaultChatHistories() const -> std::vector<ChatHistory>
{
    return {defaultChatHistory};
}

/**
 * @brief Loads the chats from disk.
 *
 * @return bool True if the chats are successfully loaded, false otherwise.
 */
auto ChatManager::loadChats() -> bool
{
    createChatsDirectoryIfNotExists();

    loadedChats.clear();
    originalChats.clear();

    try
    {
        bool foundChats = false;
        for (const auto &entry : std::filesystem::directory_iterator(chatsPath))
        {
            if (entry.path().extension() == ".chat")
            {
                std::ifstream file(entry.path(), std::ios::binary);
                if (file.is_open())
                {
                    try
                    {
                        std::string encryptedData((std::istreambuf_iterator<char>(file)),
                                                  std::istreambuf_iterator<char>());
                        std::string decryptedData = decryptData(encryptedData);
                        json j = json::parse(decryptedData);
                        ChatHistory chat = j.get<ChatHistory>();
                        loadedChats.push_back(chat);
                        originalChats.push_back(chat);
                        foundChats = true;
                    }
                    catch (const json::exception &e)
                    {
                        std::cerr << "Error parsing chat file " << entry.path()
                                  << ": " << e.what() << std::endl;
                    }
                }
            }
        }

        if (!foundChats && !hasInitialized)
        {
            initializeDefaultChatHistory();
            loadedChats = getDefaultChatHistories();
            originalChats = loadedChats;
            saveDefaultChatHistory();
        }

        // Sort chats by lastModified
        std::sort(loadedChats.begin(), loadedChats.end(),
                  [](const ChatHistory &a, const ChatHistory &b)
                  { return a.lastModified > b.lastModified; });
        std::sort(originalChats.begin(), originalChats.end(),
                  [](const ChatHistory &a, const ChatHistory &b)
                  { return a.lastModified > b.lastModified; });

        currentChatIndex = loadedChats.empty() ? -1 : 0;
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error loading chats: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Saves a chat to disk.
 *
 * @param chat The chat to save.
 * @param createNewFile True to create a new file, false to overwrite an existing file.
 * @return bool True if the chat is successfully saved, false otherwise.
 */
auto ChatManager::saveChat(const ChatHistory &chat, bool createNewFile) -> bool
{
    if (!isValidChatName(chat.name))
    {
        std::cerr << "Invalid chat name: " << chat.name << std::endl;
        return false;
    }

    try
    {
        ChatHistory newChat = chat;
        newChat.lastModified = static_cast<int>(std::time(nullptr)); // Set the current time

        if (createNewFile)
        {
            // Ensure the chat name is unique (handled in createNewChat)
        }
        else
        {
            // Update existing chat in loadedChats and originalChats
            for (size_t i = 0; i < loadedChats.size(); ++i)
            {
                if (loadedChats[i].name == newChat.name)
                {
                    loadedChats[i] = newChat;
                    originalChats[i] = newChat;
                    break;
                }
            }
        }

        json j = newChat;
        std::string jsonData = j.dump(4);
        std::string encryptedData = encryptData(jsonData);
        std::ofstream file(getChatFilePath(newChat.name), std::ios::binary);
        if (!file.is_open())
        {
            std::cerr << "Could not open file for writing: " << newChat.name << std::endl;
            return false;
        }

        file << encryptedData;

        // Sort chats by lastModified
        std::sort(loadedChats.begin(), loadedChats.end(),
                  [](const ChatHistory &a, const ChatHistory &b)
                  { return a.lastModified > b.lastModified; });
        std::sort(originalChats.begin(), originalChats.end(),
                  [](const ChatHistory &a, const ChatHistory &b)
                  { return a.lastModified > b.lastModified; });

        // Reassign IDs after sorting
        for (size_t i = 0; i < loadedChats.size(); ++i)
        {
            loadedChats[i].id = static_cast<int>(i + 1);
            originalChats[i].id = static_cast<int>(i + 1);
        }

        // After sorting, find the index of the saved chat
        auto it = std::find_if(loadedChats.begin(), loadedChats.end(),
                               [&newChat](const ChatHistory &c)
                               { return c.name == newChat.name; });

        if (it != loadedChats.end())
        {
            int index = static_cast<int>(std::distance(loadedChats.begin(), it));
            // Set current chat to the saved chat
            switchChat(index);
        }
        else
        {
            std::cerr << "Error: saved chat not found in loadedChats after sorting." << std::endl;
        }

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error saving chat: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Saves a chat to a custom path as JSON (unencrypted).
 *
 * @param chat The chat to save.
 * @param filePath The file path to save the chat.
 * @return bool True if the chat is successfully saved, false otherwise.
 */
auto ChatManager::saveChatToPath(const ChatHistory &chat, const std::string &filePath) -> bool
{
    if (!isValidChatName(chat.name))
    {
        std::cerr << "Invalid chat name: " << chat.name << std::endl;
        return false;
    }

    try
    {
        // Ensure the directory exists
        std::filesystem::path path(filePath);
        std::filesystem::create_directories(path.parent_path());

        json j = chat;
        std::ofstream file(filePath);
        if (!file.is_open())
        {
            std::cerr << "Could not open file for writing: " << filePath << std::endl;
            return false;
        }

        file << j.dump(4);

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error saving chat: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Deletes a chat from disk.
 *
 * @param chatName The name of the chat to delete.
 * @return bool True if the chat is successfully deleted, false otherwise.
 */
auto ChatManager::deleteChat(const std::string &chatName) -> bool
{
    try
    {
        std::string filePath = getChatFilePath(chatName);

        // Remove from vectors first
        auto it = std::find_if(loadedChats.begin(), loadedChats.end(),
                               [&chatName](const ChatHistory &c)
                               { return c.name == chatName; });

        if (it != loadedChats.end())
        {
            size_t index = std::distance(loadedChats.begin(), it);
            loadedChats.erase(it);
            originalChats.erase(originalChats.begin() + index);

            // Adjust current index if necessary
            if (currentChatIndex >= static_cast<int>(loadedChats.size()))
            {
                currentChatIndex = loadedChats.empty() ? -1 : loadedChats.size() - 1;
            }
            // Reassign incremental IDs
            for (size_t i = 0; i < loadedChats.size(); ++i)
            {
                loadedChats[i].id = static_cast<int>(i + 1);
                originalChats[i].id = static_cast<int>(i + 1);
            }
        }

        // Try to delete the file if it exists
        if (std::filesystem::exists(filePath))
        {
            if (!std::filesystem::remove(filePath))
            {
                std::cerr << "Failed to delete chat file: " << filePath << std::endl;
                return false;
            }
        }

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error deleting chat: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Creates a new chat, switches to it, and places it at the top of the chat list.
 */
void ChatManager::createNewChat()
{
    try
    {
        // Generate a unique base name for the new chat
        int chatNumber = static_cast<int>(loadedChats.size()) + 1;
        std::string baseName = "Untitled_" + std::to_string(chatNumber);
        std::string chatName = baseName;

        // Ensure the chat name is unique
        int counter = 1;
        while (std::any_of(loadedChats.begin(), loadedChats.end(),
                           [&chatName](const ChatHistory &chat)
                           { return chat.name == chatName; }))
        {
            chatName = baseName + "_" + std::to_string(counter++);
        }

        // Create a new ChatHistory object
        ChatHistory newChat;
        newChat.id = chatNumber;
        newChat.name = chatName;
        newChat.lastModified = static_cast<int>(std::time(nullptr));
        newChat.messages = {}; // Start with an empty message list

        // Add the new chat to the vectors
        loadedChats.push_back(newChat);
        originalChats.push_back(newChat);

        // Save the new chat (creates a new file)
        saveChat(newChat, true);

        // After saving, the chats are sorted by lastModified in saveChat()
        // We need to find the index of the new chat after sorting
        auto it = std::find_if(loadedChats.begin(), loadedChats.end(),
                               [&newChat](const ChatHistory &chat)
                               { return chat.name == newChat.name; });
        if (it != loadedChats.end())
        {
            int index = static_cast<int>(std::distance(loadedChats.begin(), it));
            // Set current chat to the new chat
            switchChat(index);
        }
        else
        {
            std::cerr << "Error: New chat not found in loadedChats after sorting." << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error creating new chat: " << e.what() << std::endl;
    }
}

/**
 * @brief Switches the current chat to the one at the specified index.
 *
 * @param newIndex The index of the chat to switch to.
 */
void ChatManager::switchChat(int newIndex)
{
    if (newIndex < 0 || newIndex >= static_cast<int>(loadedChats.size()))
    {
        return;
    }

    // Save current chat if there are unsaved changes
    if (hasUnsavedChanges())
    {
        saveChat(getCurrentChatHistory(), false);
    }

    currentChatIndex = newIndex;
}

/**
 * @brief Checks if the current chat has unsaved changes.
 *
 * @return bool True if the current chat has unsaved changes, false otherwise.
 */
auto ChatManager::hasUnsavedChanges() const -> bool
{
    if (currentChatIndex >= loadedChats.size() ||
        currentChatIndex >= originalChats.size())
    {
        return false;
    }

    const ChatHistory &current = loadedChats[currentChatIndex];
    const ChatHistory &original = originalChats[currentChatIndex];

    if (current.name != original.name ||
        current.lastModified != original.lastModified ||
        current.messages.size() != original.messages.size())
    {
        return true;
    }

    // Check if any messages are different
    for (size_t i = 0; i < current.messages.size(); ++i)
    {
        if (current.messages[i].content != original.messages[i].content ||
            current.messages[i].role != original.messages[i].role)
        {
            return true;
        }
    }

    return false;
}

/**
 * @brief Resets the current chat to its original state.
 */
void ChatManager::resetCurrentChat()
{
    if (currentChatIndex < originalChats.size())
    {
        loadedChats[currentChatIndex] = originalChats[currentChatIndex];
    }
}

/**
 * @brief Gets the path to the chat file with the specified name.
 *
 * @param chatName The name of the chat.
 * @return std::string The path to the chat file.
 */
auto ChatManager::getChatFilePath(const std::string &chatName) const -> std::string
{
    return (std::filesystem::path(chatsPath) / (chatName + ".chat")).string();
}

/**
 * @brief Checks if a chat name is valid.
 *
 * @param name The name of the chat.
 * @return bool True if the chat name is valid, false otherwise.
 */
auto ChatManager::isValidChatName(const std::string &name) const -> bool
{
    if (name.empty() || name.length() > 255)
    {
        return false;
    }

    // Check for invalid filesystem characters
    const std::string invalidChars = R"(<>:"/\|?*)";
    return name.find_first_of(invalidChars) == std::string::npos;
}

/**
 * @brief Saves the default chat history to disk.
 */
void ChatManager::saveDefaultChatHistory()
{
    // Only save default chat if no chats exist and we haven't initialized yet
    if (!hasInitialized)
    {
        auto defaults = getDefaultChatHistories();
        for (const auto &chat : defaults)
        {
            saveChat(chat, true);
        }
        hasInitialized = true; // Mark as initialized after saving the defaults
    }
}

/**
 * @brief Handles adding a user message to the current chat.
 *
 * @param messageContent The content of the user message.
 */
void ChatManager::handleUserMessage(const std::string &messageContent)
{
    if (currentChatIndex < 0 || currentChatIndex >= static_cast<int>(loadedChats.size()))
    {
        // If no chat is selected, create a new chat
        ChatHistory newChat;
        newChat.name = "Chat_" + std::to_string(loadedChats.size() + 1);
        newChat.id = static_cast<int>(loadedChats.size()) + 1;
        newChat.lastModified = static_cast<int>(std::time(nullptr));
        newChat.messages = {};
        loadedChats.push_back(newChat);
        originalChats.push_back(newChat);
        currentChatIndex = static_cast<int>(loadedChats.size()) - 1;
    }

    ChatHistory &currentChat = loadedChats[currentChatIndex];

    Message newMessage;
    newMessage.id = static_cast<int>(currentChat.messages.size()) + 1;
    newMessage.role = "user";
    newMessage.content = messageContent;
    newMessage.timestamp = std::chrono::system_clock::now();

    currentChat.messages.push_back(newMessage);

    // Auto-save the chat
    saveChat(currentChat, false);
}

/**
 * @brief Handles adding an assistant message to the current chat.
 *
 * @param messageContent The content of the assistant message.
 */
void ChatManager::handleAssistantMessage(const std::string &messageContent)
{
    if (currentChatIndex < 0 || currentChatIndex >= static_cast<int>(loadedChats.size()))
    {
        std::cerr << "No chat selected to add assistant message." << std::endl;
        return;
    }

    ChatHistory &currentChat = loadedChats[currentChatIndex];

    Message newMessage;
    newMessage.id = static_cast<int>(currentChat.messages.size()) + 1;
    newMessage.role = "assistant";
    newMessage.content = messageContent;
    newMessage.timestamp = std::chrono::system_clock::now();

    currentChat.messages.push_back(newMessage);

    // Auto-save the chat
    saveChat(currentChat, false);
}

/**
 * @brief Encrypts data using a simple XOR cipher with the encryption key.
 *
 * @param data The data to encrypt.
 * @return std::string The encrypted data.
 */
auto ChatManager::encryptData(const std::string &data) -> std::string
{
    std::string encryptedData = data;
    for (size_t i = 0; i < data.size(); ++i)
    {
        encryptedData[i] = data[i] ^ encryptionKey[i % encryptionKey.size()];
    }
    return encryptedData;
}

/**
 * @brief Decrypts data using a simple XOR cipher with the encryption key.
 *
 * @param data The data to decrypt.
 * @return std::string The decrypted data.
 */
auto ChatManager::decryptData(const std::string &data) -> std::string
{
    // XOR decryption is the same as encryption
    return encryptData(data);
}

/**
 * @brief Gets the current chat index.
 *
 * @return int The current chat index.
 */
auto ChatManager::getCurrentChatIndex() const -> int
{
    return currentChatIndex;
}

/**
 * @brief Gets the chat history at the specified index.
 *
 * @param index The index of the chat history to get.
 * @return ChatHistory The chat history at the specified index.
 */
auto ChatManager::getChatHistory(const int index) const -> ChatHistory
{
    if (index >= 0 && index < static_cast<int>(loadedChats.size()))
    {
        return loadedChats[index];
    }
    else
    {
        return ChatHistory();
    }
}

/**
 * @brief Gets the number of loaded chat histories.
 *
 * @return int The number of loaded chat histories.
 */
auto ChatManager::getChatHistoryCount() const -> int
{
    return static_cast<int>(loadedChats.size());
}

/**
 * @brief Gets the current chat history.
 *
 * @return ChatHistory The current chat history.
 */
auto ChatManager::getCurrentChatHistory() const -> ChatHistory
{
    if (currentChatIndex >= 0 && currentChatIndex < static_cast<int>(loadedChats.size()))
    {
        return loadedChats[currentChatIndex];
    }
    else
    {
        return ChatHistory();
    }
}

/**
 * @brief Sets the current chat history.
 *
 * @param chatHistory The chat history to set as current.
 */
void ChatManager::setCurrentChatHistory(const ChatHistory &chatHistory)
{
    if (currentChatIndex >= 0 && currentChatIndex < static_cast<int>(loadedChats.size()))
    {
        loadedChats[currentChatIndex] = chatHistory;
        // Save the chat after setting it
        saveChat(chatHistory, false);
    }
}

//-----------------------------------------------------------------------------
// [SECTION] PresetManager Class Implementations
//-----------------------------------------------------------------------------

/**
 * @brief Constructs a new PresetManager object.
 *
 * @param presetsDirectory The directory where presets are stored.
 */
PresetManager::PresetManager(const std::string &presetsDirectory)
    : presetsPath(presetsDirectory), currentPresetIndex(0), hasInitialized(false)
{
    createPresetsDirectoryIfNotExists();
    if (!loadPresets())
    {
        std::cerr << "Failed to load presets" << std::endl;
    }
}

/**
 * @brief Creates the presets directory if it does not exist.
 */
void PresetManager::createPresetsDirectoryIfNotExists()
{
    try
    {
        if (!std::filesystem::exists(presetsPath))
        {
            std::filesystem::create_directories(presetsPath);
        }
        // Test file write permissions
        std::string testPath = presetsPath + "/test.txt";
        std::ofstream test(testPath);
        if (!test.is_open())
        {
            std::cerr << "Cannot write to presets directory" << std::endl;
        }
        else
        {
            test.close();
            std::filesystem::remove(testPath);
        }
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        std::cerr << "Error with presets directory: " << e.what() << std::endl;
        throw;
    }
}

/**
 * @brief Initializes the default preset.
 */
void PresetManager::initializeDefaultPreset()
{
    defaultPreset = ModelPreset(
        0,
        static_cast<int>(std::time(nullptr)),
        "default",
        "You are a helpful assistant.",
        0.7f,
        0.9f,
        50.0f,
        42,
        0.0f,
        2048.0f);
}

/**
 * @brief Gets the default presets.
 *
 * @return std::vector<ModelPreset> A vector containing the default preset.
 */
auto PresetManager::getDefaultPresets() const -> std::vector<ModelPreset>
{
    return {defaultPreset};
}

/**
 * @brief Loads the presets from disk.
 *
 * @return bool True if the presets are successfully loaded, false otherwise.
 */
auto PresetManager::loadPresets() -> bool
{
    createPresetsDirectoryIfNotExists();

    loadedPresets.clear();
    originalPresets.clear();

    try
    {
        bool foundPresets = false;
        for (const auto &entry : std::filesystem::directory_iterator(presetsPath))
        {
            if (entry.path().extension() == ".json")
            {
                std::ifstream file(entry.path());
                if (file.is_open())
                {
                    try
                    {
                        json j;
                        file >> j;
                        ModelPreset preset = j.get<ModelPreset>();
                        loadedPresets.push_back(preset);
                        originalPresets.push_back(preset);
                        foundPresets = true;
                    }
                    catch (const json::exception &e)
                    {
                        std::cerr << "Error parsing preset file " << entry.path()
                                  << ": " << e.what() << std::endl;
                    }
                }
            }
        }

        if (!foundPresets && !hasInitialized)
        {
            loadedPresets = getDefaultPresets();
            originalPresets = loadedPresets;
            saveDefaultPresets();
        }

        // Sort presets by lastModified
        std::sort(loadedPresets.begin(), loadedPresets.end(),
                  [](const ModelPreset &a, const ModelPreset &b)
                  { return a.lastModified > b.lastModified; });
        std::sort(originalPresets.begin(), originalPresets.end(),
                  [](const ModelPreset &a, const ModelPreset &b)
                  { return a.lastModified > b.lastModified; });

        currentPresetIndex = loadedPresets.empty() ? -1 : 0;
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error loading presets: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Saves a preset to disk.
 *
 * @param preset The preset to save.
 * @param createNewFile True to create a new file, false to overwrite an existing file.
 * @return bool True if the preset is successfully saved, false otherwise.
 */
auto PresetManager::savePreset(const ModelPreset &preset, bool createNewFile) -> bool
{
    if (!isValidPresetName(preset.name))
    {
        std::cerr << "Invalid preset name: " << preset.name << std::endl;
        return false;
    }

    try
    {
        ModelPreset newPreset = preset;
        newPreset.lastModified = static_cast<int>(std::time(nullptr)); // Set the current time

        if (createNewFile)
        {
            // Find a unique name if necessary
            int counter = 1;
            std::string baseName = newPreset.name;
            while (std::filesystem::exists(getPresetFilePath(newPreset.name)))
            {
                newPreset.name = baseName + "_" + std::to_string(counter++);
            }
        }

        json j = newPreset;
        std::ofstream file(getPresetFilePath(newPreset.name));
        if (!file.is_open())
        {
            std::cerr << "Could not open file for writing: " << newPreset.name << std::endl;
            return false;
        }

        file << j.dump(4);

        // Update original preset state after successful save
        if (!createNewFile)
        {
            for (size_t i = 0; i < loadedPresets.size(); ++i)
            {
                if (loadedPresets[i].name == newPreset.name)
                {
                    loadedPresets[i] = newPreset;
                    originalPresets[i] = newPreset;
                    break;
                }
            }
        }
        else
        {
            // Add the new preset to the lists
            loadedPresets.push_back(newPreset);
            originalPresets.push_back(newPreset);
        }

        // Sort presets by lastModified
        std::sort(loadedPresets.begin(), loadedPresets.end(),
                  [](const ModelPreset &a, const ModelPreset &b)
                  { return a.lastModified > b.lastModified; });
        std::sort(originalPresets.begin(), originalPresets.end(),
                  [](const ModelPreset &a, const ModelPreset &b)
                  { return a.lastModified > b.lastModified; });

        // Set current preset to the saved preset
        switchPreset(static_cast<int>(loadedPresets.size()) - 1);

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error saving preset: " << e.what() << std::endl;
        return false;
    }
}

auto PresetManager::savePresetToPath(const ModelPreset &preset, const std::string &filePath) -> bool
{
    if (!isValidPresetName(preset.name))
    {
        std::cerr << "Invalid preset name: " << preset.name << std::endl;
        return false;
    }

    try
    {
        // Ensure the directory exists
        std::filesystem::path path(filePath);
        std::filesystem::create_directories(path.parent_path());

        json j = preset;
        std::ofstream file(filePath);
        if (!file.is_open())
        {
            std::cerr << "Could not open file for writing: " << filePath << std::endl;
            return false;
        }

        file << j.dump(4);

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error saving preset: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Deletes a preset from disk.
 *
 * @param presetName The name of the preset to delete.
 * @return bool True if the preset is successfully deleted, false otherwise.
 */
auto PresetManager::deletePreset(const std::string &presetName) -> bool
{
    try
    {
        std::string filePath = getPresetFilePath(presetName);

        // Remove from vectors first
        auto it = std::find_if(loadedPresets.begin(), loadedPresets.end(),
                               [&presetName](const ModelPreset &p)
                               { return p.name == presetName; });

        if (it != loadedPresets.end())
        {
            size_t index = std::distance(loadedPresets.begin(), it);
            loadedPresets.erase(it);
            originalPresets.erase(originalPresets.begin() + index);

            // Adjust current index if necessary
            if (currentPresetIndex >= static_cast<int>(loadedPresets.size()))
            {
                currentPresetIndex = loadedPresets.empty() ? -1 : loadedPresets.size() - 1;
            }
            // Reassign incremental IDs
            for (size_t i = 0; i < loadedPresets.size(); ++i)
            {
                loadedPresets[i].id = static_cast<int>(i + 1);
                originalPresets[i].id = static_cast<int>(i + 1);
            }
        }

        // Try to delete the file if it exists
        if (std::filesystem::exists(filePath))
        {
            if (!std::filesystem::remove(filePath))
            {
                std::cerr << "Failed to delete preset file: " << filePath << std::endl;
                return false;
            }
        }

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error deleting preset: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Switches the current preset to the one at the specified index.
 *
 * @param newIndex The index of the preset to switch to.
 */
void PresetManager::switchPreset(int newIndex)
{
    if (newIndex < 0 || newIndex >= static_cast<int>(loadedPresets.size()))
    {
        return;
    }

    // Reset current preset if there are unsaved changes
    if (hasUnsavedChanges())
    {
        resetCurrentPreset();
    }

    currentPresetIndex = newIndex;
}

/**
 * @brief Checks if the current preset has unsaved changes.
 *
 * @return bool True if the current preset has unsaved changes, false otherwise.
 */
auto PresetManager::hasUnsavedChanges() const -> bool
{
    if (currentPresetIndex >= loadedPresets.size() ||
        currentPresetIndex >= originalPresets.size())
    {
        return false;
    }

    const ModelPreset &current = loadedPresets[currentPresetIndex];
    const ModelPreset &original = originalPresets[currentPresetIndex];

    return current.name != original.name ||
           current.systemPrompt != original.systemPrompt ||
           current.temperature != original.temperature ||
           current.top_p != original.top_p ||
           current.top_k != original.top_k ||
           current.random_seed != original.random_seed ||
           current.min_length != original.min_length ||
           current.max_new_tokens != original.max_new_tokens;
}

/**
 * @brief Resets the current preset to its original state.
 */
void PresetManager::resetCurrentPreset()
{
    if (currentPresetIndex < originalPresets.size())
    {
        loadedPresets[currentPresetIndex] = originalPresets[currentPresetIndex];
    }
}

/**
 * @brief Gets the path to the preset file with the specified name.
 *
 * @param presetName The name of the preset.
 * @return std::string The path to the preset file.
 */
auto PresetManager::getPresetFilePath(const std::string &presetName) const -> std::string
{
    return (std::filesystem::path(presetsPath) / (presetName + ".json")).string();
}

/**
 * @brief Checks if a preset name is valid.
 *
 * @param name The name of the preset.
 * @return bool True if the preset name is valid, false otherwise.
 */
auto PresetManager::isValidPresetName(const std::string &name) const -> bool
{
    if (name.empty() || name.length() > 255)
    {
        return false;
    }

    // Check for invalid filesystem characters
    const std::string invalidChars = R"(<>:"/\|?*)";
    return name.find_first_of(invalidChars) == std::string::npos;
}

/**
 * @brief Saves the default presets to disk.
 */
void PresetManager::saveDefaultPresets()
{
    // Only save default presets if no presets exist and we haven't initialized yet
    if (!hasInitialized)
    {
        auto defaults = getDefaultPresets();
        for (const auto &preset : defaults)
        {
            savePreset(preset, true);
        }
        hasInitialized = true; // Mark as initialized after saving the defaults
    }
}

//-----------------------------------------------------------------------------
// [SECTION] MarkdownRenderer Function Implementations
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// [SECTION] GLFW and OpenGL Initialization Functions
//-----------------------------------------------------------------------------

/**
 * @brief Initializes the GLFW library.
 *
 * @return true if the GLFW library is successfully initialized, false otherwise.
 */
auto initializeGLFW() -> bool
{
    if (glfwInit() == GLFW_FALSE)
    {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }
    return true;
}

/**
 * @brief Creates a GLFW window with an OpenGL context.
 *
 * @return A pointer to the created GLFW window, or nullptr if creation fails.
 */
auto createWindow() -> GLFWwindow *
{
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    constexpr int WINDOW_WIDTH = 800;
    constexpr int WINDOW_HEIGHT = 600;
    GLFWwindow *window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Kolosal AI", nullptr, nullptr);
    if (window == nullptr)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return nullptr;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    return window;
}

/**
 * @brief Initializes the GLAD library to load OpenGL function pointers.
 *
 * @return true if GLAD is successfully initialized, false otherwise.
 */
auto initializeGLAD() -> bool
{
    if (gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0)
    {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return false;
    }
    return true;
}

//-----------------------------------------------------------------------------
// [SECTION] ImGui Setup and Main Loop
//-----------------------------------------------------------------------------

/**
 * @brief Loads a font from a file and adds it to the ImGui context.
 * @param imguiIO The ImGuiIO object containing the font data.
 * @param fontPath The path to the font file.
 * @param fallbackFont The fallback font to use if loading fails.
 * @param fontSize The size of the font.
 * @return The loaded font, or the fallback font if loading fails.
 * @note If loading fails, an error message is printed to the standard error stream.
 * @note The fallback font is used if the loaded font is nullptr.
 *
 * This function loads a font from a file and adds it to the ImGui context.
 */
auto LoadFont(ImGuiIO &imguiIO, const char *fontPath, ImFont *fallbackFont, float fontSize) -> ImFont *
{
    ImFont *font = imguiIO.Fonts->AddFontFromFileTTF(fontPath, fontSize);
    if (font == nullptr)
    {
        std::cerr << "Failed to load font: " << fontPath << std::endl;
        return fallbackFont;
    }
    return font;
}

/**
 * @brief Loads an icon font from a file and adds it to the ImGui context.
 * @param io The ImGuiIO object containing the font data.
 * @param iconFontPath The path to the icon font file.
 * @param fontSize The size of the font.
 * @return The loaded icon font, or the regular font if loading fails.
 * @note If loading fails, an error message is printed to the standard error stream.
 *
 * This function loads an icon font from a file and adds it to the ImGui context.
 */
auto LoadIconFont(ImGuiIO &io, const char *iconFontPath, float fontSize) -> ImFont *
{
    static const ImWchar icons_ranges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};
    ImFontConfig icons_config;
    icons_config.MergeMode = true;            // Enable merging
    icons_config.PixelSnapH = true;           // Enable pixel snapping
    icons_config.GlyphMinAdvanceX = fontSize; // Use fontSize as min advance x

    // First load the regular font if not already loaded
    if (!g_mdFonts.regular)
    {
        g_mdFonts.regular = LoadFont(io, IMGUI_FONT_PATH_INTER_REGULAR, io.Fonts->AddFontDefault(), fontSize);
    }

    // Load and merge icon font
    ImFont *iconFont = io.Fonts->AddFontFromFileTTF(iconFontPath, fontSize, &icons_config, icons_ranges);
    if (iconFont == nullptr)
    {
        std::cerr << "Failed to load icon font: " << iconFontPath << std::endl;
        return g_mdFonts.regular;
    }

    return iconFont;
}

/**
 * @brief Sets up the ImGui context and initializes the platform/renderer backends.
 *
 * @param window A pointer to the GLFW window.
 */
void setupImGui(GLFWwindow *window)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &imguiIO = ImGui::GetIO();

    // Set up the ImGui style
    g_mdFonts.regular = LoadFont(imguiIO, IMGUI_FONT_PATH_INTER_REGULAR, imguiIO.Fonts->AddFontDefault(), Config::Font::DEFAULT_FONT_SIZE);
    g_mdFonts.bold = LoadFont(imguiIO, IMGUI_FONT_PATH_INTER_BOLD, g_mdFonts.regular, Config::Font::DEFAULT_FONT_SIZE);
    g_mdFonts.italic = LoadFont(imguiIO, IMGUI_FONT_PATH_INTER_ITALIC, g_mdFonts.regular, Config::Font::DEFAULT_FONT_SIZE);
    g_mdFonts.boldItalic = LoadFont(imguiIO, IMGUI_FONT_PATH_INTER_BOLDITALIC, g_mdFonts.bold, Config::Font::DEFAULT_FONT_SIZE);
    g_mdFonts.code = LoadFont(imguiIO, IMGUI_FONT_PATH_FIRACODE_REGULAR, g_mdFonts.regular, Config::Font::DEFAULT_FONT_SIZE);

    // Load icon fonts
    g_iconFonts.regular = LoadIconFont(imguiIO, IMGUI_FONT_PATH_FA_REGULAR, Config::Icon::DEFAULT_FONT_SIZE);
    g_iconFonts.solid = LoadIconFont(imguiIO, IMGUI_FONT_PATH_FA_SOLID, Config::Icon::DEFAULT_FONT_SIZE);
    g_iconFonts.brands = LoadIconFont(imguiIO, IMGUI_FONT_PATH_FA_BRANDS, Config::Icon::DEFAULT_FONT_SIZE);

    imguiIO.FontDefault = g_mdFonts.regular;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

/**
 * @brief The main loop of the application, which handles rendering and event polling.
 *
 * @param window A pointer to the GLFW window.
 */
void mainLoop(GLFWwindow *window)
{
    float inputHeight = Config::INPUT_HEIGHT; // Set your desired input field height here

    // Initialize sidebar width with a default value from the configuration
    float chatHistorySidebarWidth = Config::ChatHistorySidebar::SIDEBAR_WIDTH;
    float modelPresetSidebarWidth = Config::ModelPresetSidebar::SIDEBAR_WIDTH;

    // Initialize the chat manager and preset manager
    g_chatManager = std::make_unique<ChatManager>(CHAT_HISTORY_DIRECTORY);
    g_presetManager = std::make_unique<PresetManager>(PRESETS_DIRECTORY);

    // setup NFD
    NFD_Init();

    while (glfwWindowShouldClose(window) == GLFW_FALSE)
    {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Render the sidebar first to capture any width changes
        ChatHistorySidebar::render(chatHistorySidebarWidth);
        ModelPresetSidebar::render(modelPresetSidebarWidth);

        // Render the main chat window, passing the current sidebar width
        ChatWindow::render(inputHeight, chatHistorySidebarWidth, modelPresetSidebarWidth);

        // Render the ImGui frame
        ImGui::Render();

        // Get the framebuffer size
        int display_w;
        int display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);

        // Set the viewport and clear the screen
        glViewport(0, 0, display_w, display_h);
        glClearColor(Config::BackgroundColor::R, Config::BackgroundColor::G, Config::BackgroundColor::B, Config::BackgroundColor::A);
        glClear(GL_COLOR_BUFFER_BIT);

        // Render ImGui draw data
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Swap buffers
        glfwSwapBuffers(window);
    }

    NFD_Quit();
}

/**
 * @brief Cleans up ImGui and GLFW resources before exiting the application.
 *
 * @param window A pointer to the GLFW window to be destroyed.
 */
void cleanup(GLFWwindow *window)
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
}

//-----------------------------------------------------------------------------
// [SECTION] Utility Functions
//-----------------------------------------------------------------------------

/**
 * @brief Converts RGBA color values to an ImVec4 color.
 *
 * @param r The red component of the color.
 * @param g The green component of the color.
 * @param b The blue component of the color.
 * @param a The alpha component of the color.
 * @return ImVec4 The converted ImVec4 color.
 */
auto RGBAToImVec4(float r, float g, float b, float a) -> ImVec4
{
    return ImVec4(r / 255, g / 255, b / 255, a / 255);
}

/**
 * @brief Converts a time_point to a string.
 *
 * @param tp The time_point to convert.
 * @return std::string The string representation of the time_point.
 */
auto timePointToString(const std::chrono::system_clock::time_point &tp) -> std::string
{
    std::time_t time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm = *std::localtime(&time);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

/**
 * @brief Converts a string to a time_point.
 *
 * @param str The string to convert.
 * @return std::chrono::system_clock::time_point The time_point representation of the string.
 */
auto stringToTimePoint(const std::string &str) -> std::chrono::system_clock::time_point
{
    std::istringstream iss(str);
    std::tm tm = {};
    iss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    std::time_t time = std::mktime(&tm);
    return std::chrono::system_clock::from_time_t(time);
}

//-----------------------------------------------------------------------------
// [SECTION] JSON Serialization and Deserialization
//-----------------------------------------------------------------------------

/**
 * @brief Serializes a Message object to JSON.
 *
 * @param j The JSON object to serialize to.
 * @param msg The Message object to serialize.
 */
void to_json(json &j, const Message &msg)
{
    j = json{
        {"id", msg.id},
        {"isLiked", msg.isLiked},
        {"isDisliked", msg.isDisliked},
        {"role", msg.role},
        {"content", msg.content},
        {"timestamp", timePointToString(msg.timestamp)}};
}

/**
 * @brief Deserializes a JSON object to a Message object.
 *
 * @param j The JSON object to deserialize.
 * @param msg The Message object to populate.
 */
void from_json(const json &j, Message &msg)
{
    msg.id = j.at("id").get<int>();
    msg.isLiked = j.at("isLiked").get<bool>();
    msg.isDisliked = j.at("isDisliked").get<bool>();
    msg.role = j.at("role").get<std::string>();
    msg.content = j.at("content").get<std::string>();
    std::string timestampStr = j.at("timestamp").get<std::string>();
    msg.timestamp = stringToTimePoint(timestampStr);
}

/**
 * @brief Serializes a ChatHistory object to JSON.
 *
 * @param j The JSON object to serialize to.
 * @param chatHistory The ChatHistory object to serialize.
 */
void to_json(json &j, const ChatHistory &chatHistory)
{
    j = json{
        {"id", chatHistory.id},
        {"lastModified", chatHistory.lastModified},
        {"name", chatHistory.name},
        {"messages", chatHistory.messages}};
}

/**
 * @brief Deserializes a JSON object to a ChatHistory object.
 *
 * @param j The JSON object to deserialize.
 * @param chatHistory The ChatHistory object to populate.
 */
void from_json(const json &j, ChatHistory &chatHistory)
{
    j.at("id").get_to(chatHistory.id);
    j.at("lastModified").get_to(chatHistory.lastModified);
    j.at("name").get_to(chatHistory.name);
    j.at("messages").get_to(chatHistory.messages);
}

/**
 * @brief Serializes a ModelPreset object to JSON.
 *
 * @param j The JSON object to serialize to.
 * @param p The ModelPreset object to serialize.
 */
void to_json(json &j, const ModelPreset &p)
{
    j = json{
        {"id", p.id},
        {"lastModified", p.lastModified},
        {"name", p.name},
        {"systemPrompt", p.systemPrompt},
        {"temperature", p.temperature},
        {"top_p", p.top_p},
        {"top_k", p.top_k},
        {"random_seed", p.random_seed},
        {"min_length", p.min_length},
        {"max_new_tokens", p.max_new_tokens}};
}

/**
 * @brief Deserializes a JSON object to a ModelPreset object.
 *
 * @param j The JSON object to deserialize.
 * @param p The ModelPreset object to populate.
 */
void from_json(const json &j, ModelPreset &p)
{
    j.at("id").get_to(p.id);
    j.at("lastModified").get_to(p.lastModified);
    j.at("name").get_to(p.name);
    j.at("systemPrompt").get_to(p.systemPrompt);
    j.at("temperature").get_to(p.temperature);
    j.at("top_p").get_to(p.top_p);
    j.at("top_k").get_to(p.top_k);
    j.at("random_seed").get_to(p.random_seed);
    j.at("min_length").get_to(p.min_length);
    j.at("max_new_tokens").get_to(p.max_new_tokens);
}

//-----------------------------------------------------------------------------
// [SECTION] Custom UI Functions
//-----------------------------------------------------------------------------

/**
 * @brief Renders a single button with the specified configuration.
 *
 * @param config The configuration for the button.
 */
void Widgets::Button::render(const ButtonConfig &config)
{
    // Handle button state and styles as before
    ButtonState currentState = config.state.value_or(ButtonState::NORMAL);

    switch (currentState)
    {
    case ButtonState::DISABLED:
        ImGui::PushStyleColor(ImGuiCol_Button, config.activeColor.value());
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, config.activeColor.value());
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, config.activeColor.value());

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5F);
        break;
    case ButtonState::ACTIVE:
        ImGui::PushStyleColor(ImGuiCol_Button, config.activeColor.value());
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, config.activeColor.value());
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, config.activeColor.value());

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 1.0F);
        break;
    default:
        ImGui::PushStyleColor(ImGuiCol_Button, config.backgroundColor.value());
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, config.hoverColor.value());
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, config.activeColor.value());

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 1.0F);
        break;
    }

    // Set the border radius (rounding) for the button
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, Config::Button::RADIUS);

    // Render the button with an empty label
    if (ImGui::Button(config.id.c_str(), config.size) &&
        config.onClick && currentState != ButtonState::DISABLED && currentState != ButtonState::ACTIVE)
    {
        config.onClick();
    }

    // Get the rectangle of the button
    ImVec2 buttonMin = ImGui::GetItemRectMin();
    ImVec2 buttonMax = ImGui::GetItemRectMax();

    // Prepare the label configuration
    LabelConfig labelConfig;
    labelConfig.id = config.id;
    labelConfig.label = config.label.value_or("");
    labelConfig.icon = config.icon.value_or("");
    labelConfig.size = config.size;
    labelConfig.isBold = false; // Set according to your needs
    labelConfig.iconSolid = config.iconSolid.value_or(false);
    labelConfig.gap = config.gap.value_or(5.0f);
    labelConfig.alignment = config.alignment.value_or(Alignment::CENTER);

    // Render the label inside the button's rectangle
    Widgets::Label::render(labelConfig, buttonMin, buttonMax);

    // Pop styles
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
}

/**
 * @brief Renders a group of buttons with the specified configurations.
 *
 * @param buttons The configurations for the buttons.
 * @param startX The X-coordinate to start rendering the buttons.
 * @param startY The Y-coordinate to start rendering the buttons.
 * @param spacing The spacing between buttons.
 */
void Widgets::Button::renderGroup(const std::vector<ButtonConfig> &buttons, float startX, float startY, float spacing)
{
    ImGui::SetCursorPosX(startX);
    ImGui::SetCursorPosY(startY);

    // Calculate total width of all buttons including spacing
    float totalWidth = 0.0f;
    for (const auto &button : buttons)
    {
        totalWidth += button.size.x;
    }
    totalWidth += spacing * (buttons.size() - 1);

    // Position each button and apply spacing
    float currentX = startX;
    for (size_t i = 0; i < buttons.size(); ++i)
    {
        // Set cursor position for each button
        ImGui::SetCursorPos(ImVec2(currentX, startY));

        // Render button
        render(buttons[i]);

        // Update position for next button
        currentX += buttons[i].size.x + spacing;
    }
}

/**
 * @brief Renders a label with the specified configuration.
 *
 * @param config The configuration for the label.
 */
void Widgets::Label::render(const LabelConfig &config)
{
    bool hasIcon = !config.icon.value().empty();

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + config.iconPaddingX.value());
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + config.iconPaddingY.value());

    if (hasIcon)
    {
        // Select font based on icon style
        if (config.iconSolid.value())
        {
            ImGui::PushFont(g_iconFonts.solid);
        }
        else
        {
            ImGui::PushFont(g_iconFonts.regular);
        }

        // Render icon
        ImGui::Text("%s", config.icon.value().c_str());
        ImGui::SameLine(0, (config.size.x / 4) + config.gap.value());

        ImGui::PopFont(); // Pop icon font
    }

    // Render label text with specified font weight
    if (config.isBold.value())
    {
        ImGui::PushFont(g_mdFonts.bold);
    }
    else
    {
        ImGui::PushFont(g_mdFonts.regular);
    }

    ImGui::Text("%s", config.label.c_str());

    ImGui::PopFont();
}

/**
 * @brief Sets the style for the input field.
 *
 * @param config The configuration for the input field.
 * @param rectMin The minimum position of the rectangle.
 * @param rectMax The maximum position of the rectangle.
 *
 * @see Config::LabelConfig
 */
void Widgets::Label::render(const LabelConfig &config, ImVec2 rectMin, ImVec2 rectMax)
{
    bool hasIcon = !config.icon.value().empty();
    bool hasLabel = !config.label.empty();

    // Compute the size of the rectangle
    ImVec2 rectSize = ImVec2(rectMax.x - rectMin.x, rectMax.y - rectMin.y);

    // Push a clipping rectangle to constrain rendering within the button
    ImGui::PushClipRect(rectMin, rectMax, true);

    // Calculate the size of the label text if there is a label
    ImVec2 labelSize(0, 0);
    if (hasLabel)
    {
        if (config.isBold.value())
        {
            ImGui::PushFont(g_mdFonts.bold);
        }
        else
        {
            ImGui::PushFont(g_mdFonts.regular);
        }
        labelSize = ImGui::CalcTextSize(config.label.c_str());
        ImGui::PopFont();
    }

    // Calculate the size of the icon if present
    ImVec2 iconSize(0, 0);
    if (hasIcon)
    {
        if (config.iconSolid.value())
        {
            ImGui::PushFont(g_iconFonts.solid);
        }
        else
        {
            ImGui::PushFont(g_iconFonts.regular);
        }
        iconSize = ImGui::CalcTextSize(config.icon.value().c_str());
        ImGui::PopFont();
    }

    // Calculate total content width and height
    float contentWidth = 0.0f;
    if (hasIcon && hasLabel)
    {
        contentWidth = iconSize.x + config.gap.value_or(0.0f) + labelSize.x;
    }
    else if (hasIcon)
    {
        contentWidth = iconSize.x;
    }
    else if (hasLabel)
    {
        contentWidth = labelSize.x;
    }
    float contentHeight = std::max(labelSize.y, iconSize.y);

    // Calculate vertical offset to center content
    float verticalOffset = rectMin.y + (rectSize.y - contentHeight) / 2.0f;

    // Calculate horizontal offset based on alignment
    float horizontalOffset = rectMin.x; // Default to left alignment
    Alignment alignment = config.alignment.value_or(Alignment::LEFT);
    switch (alignment)
    {
    case Alignment::CENTER:
        horizontalOffset = rectMin.x + (rectSize.x - contentWidth) / 2.0f;
        break;
    case Alignment::RIGHT:
        horizontalOffset = rectMin.x + (rectSize.x - contentWidth) - config.gap.value_or(5.0f);
        break;
    default:
        horizontalOffset = rectMin.x + config.gap.value_or(5.0f);
        break;
    }

    // Set the cursor position to the calculated offsets
    ImGui::SetCursorScreenPos(ImVec2(horizontalOffset, verticalOffset));

    // Now render the icon and/or label
    if (hasIcon)
    {
        if (config.iconSolid.value())
        {
            ImGui::PushFont(g_iconFonts.solid);
        }
        else
        {
            ImGui::PushFont(g_iconFonts.regular);
        }

        ImGui::TextUnformatted(config.icon.value().c_str());
        if (hasLabel)
        {
            ImGui::SameLine(0.0f, config.gap.value_or(0.0f));
        }

        ImGui::PopFont(); // Pop icon font
    }

    // Render label text with specified font weight, if it exists
    if (hasLabel)
    {
        if (config.isBold.value())
        {
            ImGui::PushFont(g_mdFonts.bold);
        }
        else
        {
            ImGui::PushFont(g_mdFonts.regular);
        }

        ImGui::TextUnformatted(config.label.c_str());
        ImGui::PopFont();
    }

    // Pop the clipping rectangle
    ImGui::PopClipRect();
}

/**
 * @brief Sets the style for the input field.
 *
 * @param frameRounding The rounding of the input field frame.
 * @param framePadding The padding of the input field frame.
 * @param bgColor The background color of the input field.
 */
void Widgets::InputField::setStyle(float frameRounding, const ImVec2 &framePadding, const ImVec4 &bgColor, const ImVec4 &hoverColor, const ImVec4 &activeColor)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, frameRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, framePadding);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, bgColor);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, hoverColor);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, activeColor);
}

/**
 * @brief Restores the default style for the input field.
 */
void Widgets::InputField::restoreStyle()
{
    ImGui::PopStyleColor(3); // Restore FrameBg
    ImGui::PopStyleVar(2);   // Restore frame rounding and padding
}

/**
 * @brief Handles the submission of input text.
 *
 * @param inputText The input text buffer.
 * @param focusInputField The flag to focus the input field.
 * @param processInput The function to process the input text.
 * @param clearInput The flag to clear the input text after submission.
 */
void Widgets::InputField::handleSubmission(char *inputText, bool &focusInputField, const std::function<void(const std::string &)> &processInput, bool clearInput)
{
    std::string inputStr(inputText);
    inputStr.erase(0, inputStr.find_first_not_of(" \n\r\t"));
    inputStr.erase(inputStr.find_last_not_of(" \n\r\t") + 1);

    if (!inputStr.empty())
    {
        processInput(inputStr);
        if (clearInput)
        {
            inputText[0] = '\0'; // Clear input after submission
        }
    }

    focusInputField = true;
}

/**
 * @brief Renders an input field with the specified configuration.
 *
 * @param label The label for the input field.
 * @param inputTextBuffer The buffer to store the input text.
 * @param inputSize The size of the input field.
 * @param placeholderText The placeholder text for the input field.
 * @param inputFlags The ImGui input text flags.
 * @param processInput The function to process the input text.
 * @param focusInputField The flag to focus the input field.
 */
void Widgets::InputField::renderMultiline(
    const char *label, char *inputTextBuffer, const ImVec2 &inputSize,
    const std::string &placeholderText, ImGuiInputTextFlags inputFlags,
    const std::function<void(const std::string &)> &processInput, bool &focusInputField)
{
    // Set style
    Widgets::InputField::setStyle(Config::InputField::FRAME_ROUNDING, ImVec2(Config::FRAME_PADDING_X, Config::FRAME_PADDING_Y),
                                  Config::InputField::INPUT_FIELD_BG_COLOR, Config::InputField::INPUT_FIELD_BG_COLOR, Config::InputField::INPUT_FIELD_BG_COLOR);

    // Set keyboard focus initially, then reset
    if (focusInputField)
    {
        ImGui::SetKeyboardFocusHere();
        focusInputField = false;
    }

    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + inputSize.x - 15);

    // Draw the input field
    if (ImGui::InputTextMultiline(label, inputTextBuffer, Config::InputField::TEXT_SIZE, inputSize, inputFlags) && processInput)
    {
        Widgets::InputField::handleSubmission(inputTextBuffer, focusInputField, processInput,
                                              (inputFlags & ImGuiInputTextFlags_CtrlEnterForNewLine) ||
                                                  (inputFlags & ImGuiInputTextFlags_ShiftEnterForNewLine));
    }

    ImGui::PopTextWrapPos();

    // Draw placeholder if input is empty
    if (strlen(inputTextBuffer) == 0)
    {
        // Allow overlapping rendering
        ImGui::SetItemAllowOverlap();

        // Get the current window's draw list
        ImDrawList *drawList = ImGui::GetWindowDrawList();

        // Get the input field's bounding box
        ImVec2 inputMin = ImGui::GetItemRectMin();

        // Calculate the position for the placeholder text
        ImVec2 placeholderPos = ImVec2(inputMin.x + Config::FRAME_PADDING_X, inputMin.y + Config::FRAME_PADDING_Y);

        // Set placeholder text color (light gray)
        ImU32 placeholderColor = ImGui::GetColorU32(ImVec4(0.7f, 0.7f, 0.7f, 1.0f));

        // Calculate the maximum width for the placeholder text
        float wrapWidth = inputSize.x - (2 * Config::FRAME_PADDING_X);

        // Render the placeholder text using AddText with wrapping
        drawList->AddText(
            ImGui::GetFont(),
            ImGui::GetFontSize(),
            placeholderPos,
            placeholderColor,
            placeholderText.c_str(),
            nullptr,
            wrapWidth);
    }

    // Restore original style
    Widgets::InputField::restoreStyle();
}

/**
 * @brief Renders an input field with the specified configuration.
 *
 * @param label The label for the input field.
 * @param inputTextBuffer The buffer to store the input text.
 * @param inputSize The size of the input field.
 * @param placeholderText The placeholder text for the input field.
 * @param inputFlags The ImGui input text flags.
 * @param processInput The function to process the input text.
 * @param focusInputField The flag to focus the input field.
 */
void Widgets::InputField::render(
    const char *label, char *inputTextBuffer, const ImVec2 &inputSize,
    const std::string &placeholderText, ImGuiInputTextFlags inputFlags,
    const std::function<void(const std::string &)> &processInput, bool &focusInputField)
{
    // Set style
    Widgets::InputField::setStyle(5.0F, ImVec2(Config::FRAME_PADDING_X, Config::FRAME_PADDING_Y),
                                  Config::InputField::INPUT_FIELD_BG_COLOR, Config::InputField::INPUT_FIELD_BG_COLOR, Config::InputField::INPUT_FIELD_BG_COLOR);

    // Set keyboard focus initially, then reset
    if (focusInputField)
    {
        ImGui::SetKeyboardFocusHere();
        focusInputField = false;
    }

    // Draw the single-line input field
    if (ImGui::InputText(label, inputTextBuffer, Config::InputField::TEXT_SIZE, inputFlags) && processInput)
    {
        Widgets::InputField::handleSubmission(inputTextBuffer, focusInputField, processInput, false);
    }

    // Draw placeholder if input is empty
    if (strlen(inputTextBuffer) == 0)
    {
        // Allow overlapping rendering
        ImGui::SetItemAllowOverlap();

        // Get the current window's draw list
        ImDrawList *drawList = ImGui::GetWindowDrawList();

        // Get the input field's bounding box
        ImVec2 inputMin = ImGui::GetItemRectMin();
        ImVec2 inputMax = ImGui::GetItemRectMax();

        // Calculate the position for the placeholder text
        ImVec2 placeholderPos = ImVec2(inputMin.x + Config::FRAME_PADDING_X, inputMin.y + (inputMax.y - inputMin.y) * 0.5f - ImGui::GetFontSize() * 0.5f);

        // Set placeholder text color (light gray)
        ImU32 placeholderColor = ImGui::GetColorU32(ImVec4(0.7f, 0.7f, 0.7f, 1.0f));

        // Render the placeholder text
        drawList->AddText(placeholderPos, placeholderColor, placeholderText.c_str());
    }

    // Restore original style
    Widgets::InputField::restoreStyle();
}

/**
 * @brief Renders a slider with the specified configuration.
 *
 * @param label The label for the slider.
 * @param value The value of the slider.
 * @param minValue The minimum value of the slider.
 * @param maxValue The maximum value of the slider.
 * @param sliderWidth The width of the slider.
 * @param format The format string for the slider value.
 * @param paddingX The horizontal padding for the slider.
 * @param inputWidth The width of the input field.
 */
void Widgets::Slider::render(const char *label, float &value, float minValue, float maxValue, const float sliderWidth, const char *format, const float paddingX, const float inputWidth)
{
    // Get the render label by stripping ## from the label and replacing _ with space
    std::string renderLabel = label;
    renderLabel.erase(std::remove(renderLabel.begin(), renderLabel.end(), '#'), renderLabel.end());
    std::replace(renderLabel.begin(), renderLabel.end(), '_', ' ');

    // Apply horizontal padding and render label
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + paddingX);
    Widgets::Label::render(LabelConfig{.id = label, .label = renderLabel, .size = ImVec2(0, 0), .isBold = false, .iconSolid = false});

    // Move the cursor to the right edge minus the input field width and padding
    ImGui::SameLine();

    // Apply custom styling for InputFloat
    ImGui::PushStyleColor(ImGuiCol_FrameBg, Config::Color::TRANSPARENT);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Config::Color::SECONDARY);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Config::Color::PRIMARY);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0F);

    // Get the current value as a string to measure its width
    char buffer[64];
    snprintf(buffer, sizeof(buffer), format, value);
    float textWidth = ImGui::CalcTextSize(buffer).x;

    // Adjust the input field width to match the text width, plus padding
    float adjustedInputWidth = textWidth + ImGui::GetStyle().FramePadding.x * 2.0f;

    // Calculate the position to align the input field's right edge with the desired right edge
    float rightEdge = sliderWidth + paddingX;
    float inputPositionX = rightEdge - adjustedInputWidth + 8;

    // Set the cursor position to the calculated position
    ImGui::SetCursorPosX(inputPositionX);

    // Render the input field with the adjusted width
    ImGui::PushItemWidth(adjustedInputWidth);
    if (ImGui::InputFloat((std::string(label) + "_input").c_str(), &value, 0.0f, 0.0f, format))
    {
        // Clamp the value within the specified range
        if (value < minValue)
            value = minValue;
        if (value > maxValue)
            value = maxValue;
    }
    ImGui::PopItemWidth();

    // Restore previous styling
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    // Move to the next line for the slider
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 10.0F);

    // Apply horizontal padding before rendering the slider
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + paddingX);

    // Apply custom styling for the slider
    ImGui::PushStyleColor(ImGuiCol_FrameBg, Config::Slider::TRACK_COLOR);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Config::Slider::TRACK_COLOR);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Config::Slider::TRACK_COLOR);
    ImGui::PushStyleColor(ImGuiStyleVar_SliderContrast, 1.0F);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, Config::Color::TRANSPARENT);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, Config::Slider::GRAB_COLOR);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, Config::Slider::GRAB_MIN_SIZE);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, Config::Slider::GRAB_RADIUS);
    ImGui::PushStyleVar(ImGuiStyleVar_SliderThickness, Config::Slider::TRACK_THICKNESS);

    // Render the slider below the label and input field
    ImGui::PushItemWidth(sliderWidth);
    if (ImGui::SliderFloat(label, &value, minValue, maxValue, format))
    {
        // Handle any additional logic when the slider value changes
    }
    ImGui::PopItemWidth();

    // Restore previous styling
    ImGui::PopStyleVar(3);   // FramePadding and GrabRounding
    ImGui::PopStyleColor(6); // Reset all custom colors
}

/**
 * @brief Renders an integer input field with the specified configuration.
 *
 * @param label The label for the input field.
 * @param value The value of the input field.
 * @param inputWidth The width of the input field.
 * @param paddingX The horizontal padding for the input field.
 */
void Widgets::IntInputField::render(const char *label, int &value, const float inputWidth, const float paddingX)
{
    // Get the render label by stripping ## from the label and replacing _ with space
    std::string renderLabel = label;
    renderLabel.erase(std::remove(renderLabel.begin(), renderLabel.end(), '#'), renderLabel.end());
    std::replace(renderLabel.begin(), renderLabel.end(), '_', ' ');

    // Apply horizontal padding and render label
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + paddingX);
    Widgets::Label::render(LabelConfig{.id = label, .label = renderLabel, .size = ImVec2(0, 0), .isBold = false, .iconSolid = false});

    ImGui::SetCursorPosY(ImGui::GetCursorPosY());
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + paddingX);

    // Apply custom styling for InputInt
    ImGui::PushStyleColor(ImGuiCol_FrameBg, Config::Color::SECONDARY);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Config::Color::SECONDARY);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Config::Color::PRIMARY);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0F);

    // Render input field
    ImGui::PushItemWidth(inputWidth);
    if (ImGui::InputInt(label, &value, 0, 0))
    {
        // Clamp the value within the specified range
        if (value < 0)
            value = 0;
    }
    ImGui::PopItemWidth();

    // Restore previous styling
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
}

/**
 * @brief Renders a combo box with the specified configuration.
 *
 * @param label The label for the combo box.
 * @param items The array of items to display in the combo box.
 * @param itemsCount The number of items in the array.
 * @param selectedItem The index of the selected item.
 * @param width The width of the combo box.
 * @return bool True if the selected item has changed, false otherwise.
 */
auto Widgets::ComboBox::render(const char *label, const char **items, int itemsCount, int &selectedItem, float width) -> bool
{
    // Push style variables for rounded corners
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, Config::ComboBox::FRAME_ROUNDING); // Round the frame corners
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, Config::ComboBox::POPUP_ROUNDING); // Round the popup corners

    // Push style colors
    ImGui::PushStyleColor(ImGuiCol_FrameBg, Config::ComboBox::COMBO_BG_COLOR);             // ComboBox background
    ImGui::PushStyleColor(ImGuiCol_Border, Config::ComboBox::COMBO_BORDER_COLOR);          // ComboBox border
    ImGui::PushStyleColor(ImGuiCol_Text, Config::ComboBox::TEXT_COLOR);                    // ComboBox text
    ImGui::PushStyleColor(ImGuiCol_Button, Config::ComboBox::COMBO_BG_COLOR);              // Button background
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Config::ComboBox::BUTTON_HOVERED_COLOR); // Button hovered
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Config::ComboBox::BUTTON_ACTIVE_COLOR);   // Button active
    ImGui::PushStyleColor(ImGuiCol_PopupBg, Config::ComboBox::POPUP_BG_COLOR);             // Popup background

    // Set the ComboBox width
    ImGui::SetNextItemWidth(width);

    // Render the ComboBox
    bool changed = false;
    if (ImGui::BeginCombo(label, items[selectedItem]))
    {
        for (int i = 0; i < itemsCount; ++i)
        {
            bool isSelected = (selectedItem == i);
            if (ImGui::Selectable(items[i], isSelected))
            {
                selectedItem = i;
                changed = true;
            }

            if (isSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    // Pop style colors and variables to revert to previous styles
    ImGui::PopStyleColor(7); // Number of colors pushed
    ImGui::PopStyleVar(2);   // Number of style vars pushed

    return changed; // Return true if the selected item has changed
}

//-----------------------------------------------------------------------------
// [SECTION] Chat Window Rendering Functions
//-----------------------------------------------------------------------------

/**
 * @brief Pushes an ID and sets the colors for the ImGui context based on the message type.
 *
 * @param msg The message object containing information about the message.
 * @param index The index used to set the ImGui ID.
 */
void ChatWindow::MessageBubble::pushIDAndColors(const Message msg, int index)
{
    ImGui::PushID(index);

    ImVec4 userColor = ImVec4(Config::UserColor::COMPONENT, Config::UserColor::COMPONENT, Config::UserColor::COMPONENT, 1.0F); // #2f2f2f for user
    ImVec4 transparentColor = ImVec4(0.0F, 0.0F, 0.0F, 0.0F);                                                                  // Transparent for bot

    ImVec4 bgColor = msg.role == "user" ? userColor : transparentColor;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, bgColor);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0F, 1.0F, 1.0F, 1.0F)); // White text
}

/**
 * @brief Calculates the dimensions for the message bubble.
 *
 * @param msg The message object containing information about the message.
 * @return A tuple containing:
 *         - windowWidth: The width of the ImGui window content region.
 *         - bubbleWidth: The width of the message bubble.
 *         - bubblePadding: The padding inside the message bubble.
 *         - paddingX: The horizontal padding to align the bubble.
 */
auto ChatWindow::MessageBubble::calculateDimensions(const Message msg, float windowWidth) -> std::tuple<float, float, float>
{
    float bubbleWidth = windowWidth * Config::Bubble::WIDTH_RATIO; // 75% width for both user and bot
    float bubblePadding = Config::Bubble::PADDING;                 // Padding inside the bubble
    float paddingX = msg.role == "user"
                         ? (windowWidth - bubbleWidth - Config::Bubble::RIGHT_PADDING)
                         : Config::Bubble::BOT_PADDING_X;

    return {bubbleWidth, bubblePadding, paddingX};
}

/**
 * @brief Renders the message content inside the bubble.
 *
 * @param msg The message object containing the content to be displayed.
 * @param bubbleWidth The width of the message bubble.
 * @param bubblePadding The padding inside the message bubble.
 */
void ChatWindow::MessageBubble::renderMessageContent(const Message msg, float bubbleWidth, float bubblePadding)
{
    ImGui::SetCursorPosX(bubblePadding);
    ImGui::SetCursorPosY(bubblePadding);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + bubbleWidth - (bubblePadding * 2));
    ImGui::TextWrapped("%s", msg.content.c_str());
    ImGui::PopTextWrapPos();
}

/**
 * @brief Renders the timestamp of a message in the UI.
 *
 * @param msg The message object containing the timestamp to render.
 * @param bubblePadding The padding to apply on the left side of the timestamp.
 */
void ChatWindow::MessageBubble::renderTimestamp(const Message msg, float bubblePadding)
{
    // Set timestamp color to a lighter gray
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7F, 0.7F, 0.7F, 1.0F)); // Light gray for timestamp

    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - ImGui::GetTextLineHeightWithSpacing() - (bubblePadding - Config::Timing::TIMESTAMP_OFFSET_Y)); // Align timestamp at the bottom
    ImGui::SetCursorPosX(bubblePadding);                                                                                                           // Align timestamp to the left
    ImGui::TextWrapped("%s", timePointToString(msg.timestamp).c_str());

    ImGui::PopStyleColor(); // Restore original text color
}

/**
 * @brief Renders interactive buttons for a message bubble.
 *
 * @param msg The message object containing the content and metadata.
 * @param index The index of the message in the message list.
 * @param bubbleWidth The width of the message bubble.
 * @param bubblePadding The padding inside the message bubble.
 */
void ChatWindow::MessageBubble::renderButtons(const Message msg, int index, float bubbleWidth, float bubblePadding)
{
    ImVec2 textSize = ImGui::CalcTextSize(msg.content.c_str(), nullptr, true, bubbleWidth - bubblePadding * 2);
    float buttonPosY = textSize.y + bubblePadding;

    if (msg.role == "user")
    {
        ButtonConfig copyButtonConfig{
            .id = "##copy" + std::to_string(index),
            .label = std::nullopt,
            .icon = ICON_FA_COPY,
            .size = ImVec2(Config::Button::WIDTH, 0),
            .onClick = [&msg]()
            {
                ImGui::SetClipboardText(msg.content.c_str());
                std::cout << "Copied message content to clipboard" << std::endl;
            }};

        std::vector<ButtonConfig> userButtons = {copyButtonConfig};

        Widgets::Button::renderGroup(
            userButtons,
            bubbleWidth - bubblePadding - Config::Button::WIDTH,
            buttonPosY);
    }
    else
    {
        ButtonConfig likeButtonConfig{
            .id = "##like" + std::to_string(index),
            .label = std::nullopt,
            .icon = ICON_FA_THUMBS_UP,
            .size = ImVec2(Config::Button::WIDTH, 0),
            .onClick = [index]()
            {
                std::cout << "Like button clicked for message " << index << std::endl;
            }};

        ButtonConfig dislikeButtonConfig{
            .id = "##dislike" + std::to_string(index),
            .label = std::nullopt,
            .icon = ICON_FA_THUMBS_DOWN,
            .size = ImVec2(Config::Button::WIDTH, 0),
            .onClick = [index]()
            {
                std::cout << "Dislike button clicked for message " << index << std::endl;
            }};

        std::vector<ButtonConfig> assistantButtons = {likeButtonConfig, dislikeButtonConfig};

        Widgets::Button::renderGroup(
            assistantButtons,
            bubbleWidth - bubblePadding - (2 * Config::Button::WIDTH + Config::Button::SPACING),
            buttonPosY);
    }
}

/**
 * @brief Renders a message bubble with associated UI elements.
 *
 * @param msg The message object to be rendered.
 * @param index The index of the message in the list.
 */
void ChatWindow::MessageBubble::renderMessage(const Message msg, int index, float contentWidth)
{
    ChatWindow::MessageBubble::pushIDAndColors(msg, index);

    float windowWidth = contentWidth; // Use contentWidth instead of ImGui::GetWindowContentRegionMax().x

    auto [bubbleWidth, bubblePadding, paddingX] = ChatWindow::MessageBubble::calculateDimensions(msg, windowWidth);

    ImVec2 textSize = ImGui::CalcTextSize(msg.content.c_str(), nullptr, true, bubbleWidth - bubblePadding * 2);
    float estimatedHeight = textSize.y + bubblePadding * 2 + ImGui::GetTextLineHeightWithSpacing(); // Add padding and spacing for buttons

    ImGui::SetCursorPosX(paddingX);

    if (msg.role == "user")
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, Config::InputField::CHILD_ROUNDING); // Adjust the rounding as desired
    }

    ImGui::BeginGroup();

    // Corrected BeginChild call
    ImGui::BeginChild(
        ("MessageCard" + std::to_string(index)).c_str(),
        ImVec2(bubbleWidth, estimatedHeight),
        false,                       // No border
        ImGuiWindowFlags_NoScrollbar // Supported flag
    );

    ChatWindow::MessageBubble::renderMessageContent(msg, bubbleWidth, bubblePadding);
    ChatWindow::MessageBubble::renderTimestamp(msg, bubblePadding);
    ChatWindow::MessageBubble::renderButtons(msg, index, bubbleWidth, bubblePadding);

    ImGui::EndChild();
    ImGui::EndGroup();

    if (msg.role == "user")
    {
        ImGui::PopStyleVar(); // Pop ChildRounding
    }

    ImGui::PopStyleColor(2); // Pop Text and ChildBg colors
    ImGui::PopID();
    ImGui::Spacing(); // Add spacing between messages
}

/**
 * @brief Renders the chat history with an auto-scroll feature.
 *
 * @param chatHistory The chat history object containing the messages to be displayed.
 */
void ChatWindow::renderChatHistory(const ChatHistory chatHistory, float contentWidth)
{
    static size_t lastMessageCount = 0;
    size_t currentMessageCount = chatHistory.messages.size();

    // Check if new messages have been added
    bool newMessageAdded = currentMessageCount > lastMessageCount;

    // Save the scroll position before rendering
    float scrollY = ImGui::GetScrollY();
    float scrollMaxY = ImGui::GetScrollMaxY();
    bool isAtBottom = (scrollMaxY <= 0.0F) || (scrollY >= scrollMaxY - 1.0F);

    // Render messages
    const std::vector<Message> &messages = chatHistory.messages;
    for (size_t i = 0; i < messages.size(); ++i)
    {
        ChatWindow::MessageBubble::renderMessage(messages[i], static_cast<int>(i), contentWidth);
    }

    // If the user was at the bottom and new messages were added, scroll to bottom
    if (newMessageAdded && isAtBottom)
    {
        ImGui::SetScrollHereY(1.0F);
    }

    // Update the last message count
    lastMessageCount = currentMessageCount;
}

/**
 * @brief Renders the chat window to cover the full width and height of the application window,
 *        minus the sidebar width.
 *
 * @param focusInputField A boolean reference to control the focus state of the input field.
 * @param inputHeight The height of the input field.
 * @param sidebarWidth The current width of the sidebar.
 */
void ChatWindow::render(float inputHeight, float leftSidebarWidth, float rightSidebarWidth)
{
    ImGuiIO &imguiIO = ImGui::GetIO();

    // Calculate the size of the chat window based on the sidebar width
    ImVec2 windowSize = ImVec2(imguiIO.DisplaySize.x - rightSidebarWidth - leftSidebarWidth, imguiIO.DisplaySize.y);

    // Set window to cover the remaining display area
    ImGui::SetNextWindowPos(ImVec2(leftSidebarWidth, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar |
                                   ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus;
    // Remove window border
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);

    ImGui::Begin("Chatbot", nullptr, windowFlags);

    // Calculate available width for content
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float contentWidth = (availableWidth < Config::CHAT_WINDOW_CONTENT_WIDTH) ? availableWidth : Config::CHAT_WINDOW_CONTENT_WIDTH;
    float paddingX = (availableWidth - contentWidth) / 2.0F;

    // Center the content horizontally
    if (paddingX > 0.0F)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + paddingX);
    }

    // Begin the main scrolling region for the chat history
    float availableHeight = ImGui::GetContentRegionAvail().y - inputHeight - Config::BOTTOM_MARGIN;
    ImGui::BeginChild("ChatHistoryRegion", ImVec2(contentWidth, availableHeight), false, ImGuiWindowFlags_NoScrollbar);

    // Render chat history
    ChatWindow::renderChatHistory(g_chatManager->getCurrentChatHistory(), contentWidth);

    ImGui::EndChild(); // End of ChatHistoryRegion

    // Add some spacing or separator if needed
    ImGui::Spacing();

    // Center the input field horizontally by calculating left padding
    float inputFieldPaddingX = (availableWidth - contentWidth) / 2.0F;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + inputFieldPaddingX);

    // Render the input field at the bottom, centered
    ChatWindow::renderInputField(inputHeight, contentWidth);

    ImGui::End(); // End of Chatbot window

    // Restore the window border size
    ImGui::PopStyleVar();
}

/**
 * @brief Renders the input field with text wrapping and no horizontal scrolling.
 *
 * @param focusInputField A reference to the focus input field flag.
 * @param inputHeight The height of the input field.
 */
void ChatWindow::renderInputField(float inputHeight, float inputWidth)
{
    static std::array<char, Config::InputField::TEXT_SIZE> inputTextBuffer = {0};
    static bool focusInputField = true;

    // Define the input size
    ImVec2 inputSize = ImVec2(inputWidth, inputHeight);

    // Define a lambda to process the submitted input
    auto processInput = [](const std::string &input)
    {
        g_chatManager->handleUserMessage(input);
        g_chatManager->handleAssistantMessage(input);
    };

    // Render the input field widget with a placeholder
    Widgets::InputField::renderMultiline("##chatinput", inputTextBuffer.data(), inputSize,
                                         "Type a message and press Enter to send (Ctrl+Enter or Shift+Enter for new line)",
                                         ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CtrlEnterForNewLine | ImGuiInputTextFlags_ShiftEnterForNewLine,
                                         processInput, focusInputField);
}

//-----------------------------------------------------------------------------
// [SECTION] Chat History Sidebar Rendering Functions
//-----------------------------------------------------------------------------

/**
 * @brief Renders the chat history sidebar with the specified width.
 *
 * @param sidebarWidth The width of the sidebar.
 */
void ChatHistorySidebar::render(float &sidebarWidth)
{
    ImGuiIO &io = ImGui::GetIO();
    const float sidebarHeight = io.DisplaySize.y;

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(sidebarWidth, sidebarHeight), ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(Config::ChatHistorySidebar::MIN_SIDEBAR_WIDTH, sidebarHeight),
        ImVec2(Config::ChatHistorySidebar::MAX_SIDEBAR_WIDTH, sidebarHeight));

    ImGuiWindowFlags sidebarFlags = ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoCollapse |
                                    ImGuiWindowFlags_NoTitleBar |
                                    ImGuiWindowFlags_NoBackground |
                                    ImGuiWindowFlags_NoScrollbar;

    ImGui::Begin("Chat History", nullptr, sidebarFlags);

    ImVec2 currentSize = ImGui::GetWindowSize();
    sidebarWidth = currentSize.x;

    ButtonConfig createNewChatButtonConfig{
        .id = "##createNewChat",
        .label = "New Chat",
        .icon = ICON_FA_COMMENT_MEDICAL,
        .size = ImVec2(sidebarWidth - 20, 0),
        .gap = 10.0F,
        .onClick = []()
        {
            g_chatManager->createNewChat();
        },
        .backgroundColor = RGBAToImVec4(26, 95, 180, 128),
        .hoverColor = RGBAToImVec4(53, 132, 228, 255),
        .activeColor = RGBAToImVec4(53, 132, 228, 255)};

    Widgets::Button::render(createNewChatButtonConfig);

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Spacing();

    Widgets::Label::render(
        LabelConfig{.id = "##chathistory",
                    .label = "Recents",
                    .size = ImVec2(Config::Icon::DEFAULT_FONT_SIZE, 0),
                    .iconPaddingX = 10.0F,
                    .isBold = true});

    ImGui::Spacing();

    for (size_t i = 0; i < g_chatManager->getChatHistoryCount(); ++i)
    {
        const ChatHistory chat = g_chatManager->getChatHistory(i);

        ButtonConfig chatButtonConfig{
            .id = "##chat" + std::to_string(i),
            .label = chat.name,
            .icon = ICON_FA_COMMENT,
            .size = ImVec2(sidebarWidth - 20, 0),
            .gap = 10.0F,
            .onClick = [i]()
            {
                g_chatManager->switchChat(i);
            },
            .state = (i == g_chatManager->getCurrentChatIndex()) ? ButtonState::ACTIVE : ButtonState::NORMAL,
            .alignment = Alignment::LEFT};

        Widgets::Button::render(chatButtonConfig);
    }

    ImGui::End();
}

//-----------------------------------------------------------------------------
// [SECTION] Model Settings Rendering Functions
//-----------------------------------------------------------------------------

/**
 * @brief Renders the model settings sidebar with the specified width.
 *
 * @param sidebarWidth The width of the sidebar.
 */
void ModelPresetSidebar::renderSamplingSettings(const float sidebarWidth)
{
    ImGui::Spacing();
    ImGui::Spacing();

    Widgets::Label::render({.id = "##systemprompt",
                            .label = "System Prompt",
                            .icon = ICON_FA_COG,
                            .size = ImVec2(Config::Icon::DEFAULT_FONT_SIZE, 0),
                            .isBold = true});

    ImGui::Spacing();
    ImGui::Spacing();

    // Get reference to current preset
    auto &currentPreset = g_presetManager->getCurrentPreset();

    // Ensure the string has enough capacity
    currentPreset.systemPrompt.reserve(Config::InputField::TEXT_SIZE);

    // System prompt input
    static bool focusSystemPrompt = true;
    ImVec2 inputSize = ImVec2(sidebarWidth - 20, 100);

    // Provide a processInput lambda to update the systemPrompt
    Widgets::InputField::renderMultiline(
        "##systemprompt",
        &currentPreset.systemPrompt[0], // Ensure mutable access
        inputSize,
        "Enter your system prompt here...",
        0,
        [&](const std::string &input)
        {
            currentPreset.systemPrompt = input; // Update the string with user input
        },
        focusSystemPrompt);

    ImGui::Spacing();
    ImGui::Spacing();

    Widgets::Label::render({.id = "##modelconfig",
                            .label = "Model Configuration",
                            .icon = ICON_FA_SLIDERS_H,
                            .size = ImVec2(Config::Icon::DEFAULT_FONT_SIZE, 0),
                            .isBold = true});

    ImGui::Spacing();
    ImGui::Spacing();

    // Sampling settings
    Widgets::Slider::render("##temperature", currentPreset.temperature, 0.0f, 1.0f, sidebarWidth - 30);
    Widgets::Slider::render("##top_p", currentPreset.top_p, 0.0f, 1.0f, sidebarWidth - 30);
    Widgets::Slider::render("##top_k", currentPreset.top_k, 0.0f, 100.0f, sidebarWidth - 30, "%.0f");
    Widgets::IntInputField::render("##random_seed", currentPreset.random_seed, sidebarWidth - 30);

    ImGui::Spacing();
    ImGui::Spacing();

    // Generation settings
    Widgets::Slider::render("##min_length", currentPreset.min_length, 0.0f, 4096.0f, sidebarWidth - 30, "%.0f");
    Widgets::Slider::render("##max_new_tokens", currentPreset.max_new_tokens, 0.0f, 4096.0f, sidebarWidth - 30, "%.0f");
}

/**
 * @brief Renders the "Save Preset As" dialog for saving a model preset under a new name.
 */
void ModelPresetSidebar::renderSaveAsDialog()
{
    if (ModelPresetSidebar::State::g_showSaveAsDialog)
    {
        ImGui::OpenPopup("Save Preset As");
        ModelPresetSidebar::State::g_showSaveAsDialog = false;
    }

    // Change the window title background color
    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.125F, 0.125F, 0.125F, 1.0F));       // Inactive state color
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.125F, 0.125F, 0.125F, 1.0F)); // Active state color

    // Apply rounded corners to the window
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);

    if (ImGui::BeginPopupModal("Save Preset As", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        static bool focusNewPresetName = true;
        // Set the new preset name to the current preset name by default
        if (strlen(ModelPresetSidebar::State::g_newPresetName) == 0)
        {
            strncpy(ModelPresetSidebar::State::g_newPresetName, g_presetManager->getCurrentPreset().name.c_str(), sizeof(ModelPresetSidebar::State::g_newPresetName));
            ModelPresetSidebar::State::g_newPresetName[sizeof(ModelPresetSidebar::State::g_newPresetName) - 1] = '\0'; // Ensure null-terminated
        }

        Widgets::InputField::render(
            "##newpresetname", ModelPresetSidebar::State::g_newPresetName, ImVec2(250, 0),
            "Enter new preset name...", 0, nullptr, focusNewPresetName);

        ImGui::Spacing();

        ButtonConfig confirmSave{
            .id = "##confirmSave",
            .label = "Save",
            .icon = std::nullopt,
            .size = ImVec2(122.5F, 0),
            .onClick = []()
            {
                if (strlen(ModelPresetSidebar::State::g_newPresetName) > 0)
                {
                    auto currentPreset = g_presetManager->getCurrentPreset();
                    currentPreset.name = ModelPresetSidebar::State::g_newPresetName;
                    if (g_presetManager->savePreset(currentPreset, true))
                    {
                        g_presetManager->loadPresets(); // Reload to include the new preset
                        ImGui::CloseCurrentPopup();
                        memset(ModelPresetSidebar::State::g_newPresetName, 0, sizeof(ModelPresetSidebar::State::g_newPresetName));
                    }
                }
            },
            .iconSolid = false,
            .backgroundColor = g_presetManager->hasUnsavedChanges() ? RGBAToImVec4(26, 95, 180, 255) : RGBAToImVec4(26, 95, 180, 128),
            .hoverColor = RGBAToImVec4(53, 132, 228, 255),
            .activeColor = RGBAToImVec4(26, 95, 180, 255)};

        ButtonConfig cancelSave{
            .id = "##cancelSave",
            .label = "Cancel",
            .icon = std::nullopt,
            .size = ImVec2(122.5F, 0),
            .onClick = []()
            {
                ImGui::CloseCurrentPopup();
                memset(ModelPresetSidebar::State::g_newPresetName, 0, sizeof(ModelPresetSidebar::State::g_newPresetName));
            },
            .iconSolid = false,
            .backgroundColor = Config::Color::SECONDARY,
            .hoverColor = Config::Color::PRIMARY,
            .activeColor = Config::Color::SECONDARY};

        std::vector<ButtonConfig> saveAsDialogButtons = {confirmSave, cancelSave};
        Widgets::Button::renderGroup(saveAsDialogButtons, ImGui::GetCursorPosX(), ImGui::GetCursorPosY(), 10);

        ImGui::EndPopup();
    }

    // Revert to the previous style
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
}

/**
 * @brief Renders the model settings sidebar with the specified width.
 *
 * @param sidebarWidth The width of the sidebar.
 */
void ModelPresetSidebar::renderModelPresetsSelection(const float sidebarWidth)
{
    ImGui::Spacing();
    ImGui::Spacing();

    Widgets::Label::render({.id = "##modelpresets",
                            .label = "Model Presets",
                            .icon = ICON_FA_BOX_OPEN,
                            .size = ImVec2(Config::Icon::DEFAULT_FONT_SIZE, 0),
                            .isBold = true});

    ImGui::Spacing();
    ImGui::Spacing();

    // Get the current presets and create a vector of names
    const auto &presets = g_presetManager->getPresets();
    std::vector<const char *> presetNames;
    for (const auto &preset : presets)
    {
        presetNames.push_back(preset.name.c_str());
    }

    int currentIndex = g_presetManager->getCurrentPresetIndex();

    // Render the ComboBox for model presets
    float comboBoxWidth = sidebarWidth - 54;
    if (Widgets::ComboBox::render("##modelpresets",
                                  presetNames.data(),
                                  presetNames.size(),
                                  currentIndex,
                                  comboBoxWidth))
    {
        g_presetManager->switchPreset(currentIndex);
    }

    // Delete button
    ImGui::SameLine();
    ButtonConfig deleteButton{
        .id = "##delete",
        .label = std::nullopt,
        .icon = ICON_FA_TRASH,
        .size = ImVec2(24, 0),
        .onClick = []()
        {
            if (g_presetManager->getPresets().size() > 1)
            { // Prevent deleting last preset
                auto &currentPreset = g_presetManager->getCurrentPreset();
                if (g_presetManager->deletePreset(currentPreset.name))
                {
                    // Force reload presets after successful deletion
                    g_presetManager->loadPresets();
                }
            }
        },
        .iconSolid = true,
        .backgroundColor = Config::Color::TRANSPARENT,
        .hoverColor = RGBAToImVec4(191, 88, 86, 255),
        .activeColor = RGBAToImVec4(165, 29, 45, 255)};

    // Only enable delete button if we have more than one preset
    if (presets.size() <= 1)
    {
        deleteButton.state = ButtonState::DISABLED;
    }

    Widgets::Button::render(deleteButton);

    ImGui::Spacing();
    ImGui::Spacing();

    // Save and Save as New buttons
    ButtonConfig saveButton{
        .id = "##save",
        .label = "Save",
        .icon = std::nullopt,
        .size = ImVec2(sidebarWidth / 2 - 15, 0),
        .onClick = []()
        {
            bool hasChanges = g_presetManager->hasUnsavedChanges();
            if (hasChanges)
            {
                auto currentPreset = g_presetManager->getCurrentPreset();
                bool saved = g_presetManager->savePreset(currentPreset);
                std::cout << "Save result: " << (saved ? "success" : "failed") << std::endl;
                if (saved)
                {
                    g_presetManager->loadPresets();
                }
            }
        },
        .iconSolid = false,
        .backgroundColor = g_presetManager->hasUnsavedChanges() ? RGBAToImVec4(26, 95, 180, 255) : RGBAToImVec4(26, 95, 180, 128),
        .hoverColor = RGBAToImVec4(53, 132, 228, 255),
        .activeColor = RGBAToImVec4(26, 95, 180, 255)};

    ButtonConfig saveAsNewButton{
        .id = "##saveasnew",
        .label = "Save as New",
        .icon = std::nullopt,
        .size = ImVec2(sidebarWidth / 2 - 15, 0),
        .onClick = []()
        {
            ModelPresetSidebar::State::g_showSaveAsDialog = true;
        }};

    std::vector<ButtonConfig> buttons = {saveButton, saveAsNewButton};
    Widgets::Button::renderGroup(buttons, 9, ImGui::GetCursorPosY(), 10);

    ImGui::Spacing();
    ImGui::Spacing();

    ModelPresetSidebar::renderSaveAsDialog();
}

/**
 * @brief Exports the current model presets to a JSON file.
 */
void ModelPresetSidebar::exportPresets()
{
    nfdu8char_t *outPath = nullptr;
    nfdu8filteritem_t filters[2] = {{"JSON Files", "json"}};
    const nfdsavedialogu8args_t args{
        .filterList = filters,
        .filterCount = 1,
    };
    nfdresult_t result = NFD_SaveDialogU8_With(&outPath, &args);

    if (result == NFD_OKAY)
    {
        std::filesystem::path savePath(outPath);
        // Optionally, enforce the .json extension
        if (savePath.extension() != ".json")
        {
            savePath += ".json";
        }

        // Free the memory allocated by NFD
        NFD_FreePathU8(outPath);

        // Save the preset to the chosen path
        const auto &currentPreset = g_presetManager->getCurrentPreset();
        bool success = g_presetManager->savePresetToPath(currentPreset, savePath.string());

        if (success)
        {
            std::cout << "Preset saved successfully to: " << savePath << std::endl;
            // Optionally, display a success message in the UI
        }
        else
        {
            std::cerr << "Failed to save preset to: " << savePath << std::endl;
            // Optionally, display an error message in the UI
        }
    }
    else if (result == NFD_CANCEL)
    {
        // User canceled the dialog; no action needed
        std::cout << "Save dialog canceled by the user." << std::endl;
    }
    else
    {
        // Handle error
        std::cerr << "Error from NFD: " << NFD_GetError() << std::endl;
    }
}

/**
 * @brief Renders the model settings sidebar with the specified width.
 *
 * @param sidebarWidth The width of the sidebar.
 */
void ModelPresetSidebar::render(float &sidebarWidth)
{
    ImGuiIO &io = ImGui::GetIO();
    const float sidebarHeight = io.DisplaySize.y;

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - sidebarWidth, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(sidebarWidth, sidebarHeight), ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(Config::ModelPresetSidebar::MIN_SIDEBAR_WIDTH, sidebarHeight),
        ImVec2(Config::ModelPresetSidebar::MAX_SIDEBAR_WIDTH, sidebarHeight));

    ImGuiWindowFlags sidebarFlags = ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoCollapse |
                                    ImGuiWindowFlags_NoTitleBar |
                                    ImGuiWindowFlags_NoBackground |
                                    ImGuiWindowFlags_NoScrollbar;

    ImGui::Begin("Model Settings", nullptr, sidebarFlags);

    ImVec2 currentSize = ImGui::GetWindowSize();
    sidebarWidth = currentSize.x;

    ModelPresetSidebar::renderModelPresetsSelection(sidebarWidth);
    ImGui::Separator();
    ModelPresetSidebar::renderSamplingSettings(sidebarWidth);
    ImGui::Separator();

    ImGui::Spacing();
    ImGui::Spacing();

    // Export button
    ButtonConfig exportButton{
        .id = "##export",
        .label = "Export as JSON",
        .icon = std::nullopt,
        .size = ImVec2(sidebarWidth - 20, 0),
        .onClick = ModelPresetSidebar::exportPresets,
        .iconSolid = false,
        .backgroundColor = Config::Color::SECONDARY,
        .hoverColor = Config::Color::PRIMARY,
        .activeColor = Config::Color::SECONDARY};
    Widgets::Button::render(exportButton);

    ImGui::End();
}