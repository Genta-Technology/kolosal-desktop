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

#ifndef KOLOSAL_H
#define KOLOSAL_H

#include <string>
#include <vector>
#include <chrono>
#include <tuple>
#include <stack>
#include <regex>
#include <array>
#include <optional>
#include <filesystem>
#include "nfd.h"
#include "imgui.h"
#include "IconsFontAwesome5.h"
#include "IconsFontAwesome5Brands.h"
#include "json.hpp"

#define CHAT_HISTORY_DIRECTORY "chat_history"
#define PRESETS_DIRECTORY "presets"

using json = nlohmann::json;

//-----------------------------------------------------------------------------
// [SECTION] Constants and Configurations
//-----------------------------------------------------------------------------

namespace Config
{
    // Global constants for padding
    constexpr float FRAME_PADDING_X = 10.0F;
    constexpr float FRAME_PADDING_Y = 10.0F;

    // Constants to replace magic numbers
    namespace Font
    {
        constexpr float DEFAULT_FONT_SIZE = 16.0F;
    } // namespace Font

    namespace Icon
    {
        constexpr float DEFAULT_FONT_SIZE = 14.0F;
    } // namespace Icon

    namespace BackgroundColor
    {
        constexpr float R = 0.1F;
        constexpr float G = 0.1F;
        constexpr float B = 0.1F;
        constexpr float A = 1.0F;
    } // namespace BackgroundColor

    namespace UserColor
    {
        constexpr float COMPONENT = 47.0F / 255.0F;
    } // namespace UserColor

    namespace Bubble
    {
        constexpr float WIDTH_RATIO = 0.75F;
        constexpr float PADDING = 15.0F;
        constexpr float RIGHT_PADDING = 20.0F;
        constexpr float BOT_PADDING_X = 20.0F;
    } // namespace Bubble

    namespace Timing
    {
        constexpr float TIMESTAMP_OFFSET_Y = 5.0F;
    } // namespace Timing

    namespace Button
    {
        constexpr float WIDTH = 30.0F;
        constexpr float SPACING = 10.0F;
        constexpr float RADIUS = 5.0F;
    } // namespace Button

    namespace InputField
    {
        constexpr size_t TEXT_SIZE = 1024;

        constexpr float CHILD_ROUNDING = 10.0F;
        constexpr float FRAME_ROUNDING = 12.0F;

        constexpr ImVec4 INPUT_FIELD_BG_COLOR = ImVec4(0.15F, 0.15F, 0.15F, 1.0F);
    } // namespace InputField

    namespace ChatHistorySidebar
    {
        constexpr float SIDEBAR_WIDTH = 150.0F;
        constexpr float MIN_SIDEBAR_WIDTH = 150.0F;
        constexpr float MAX_SIDEBAR_WIDTH = 400.0F;
    } // namespace ChatHistorySidebar

    namespace ModelPresetSidebar
    {
        constexpr float SIDEBAR_WIDTH = 200.0F;
        constexpr float MIN_SIDEBAR_WIDTH = 200.0F;
        constexpr float MAX_SIDEBAR_WIDTH = 400.0F;
    } // namespace ModelSettings

    namespace Color
    {
        constexpr ImVec4 TRANSPARENT = ImVec4(0.0F, 0.0F, 0.0F, 0.0F);
        constexpr ImVec4 PRIMARY = ImVec4(0.3F, 0.3F, 0.3F, 0.5F);
        constexpr ImVec4 SECONDARY = ImVec4(0.3F, 0.3F, 0.3F, 0.3F);
        constexpr ImVec4 DISABLED = ImVec4(0.3F, 0.3F, 0.3F, 0.1F);
    } // namespace Color

    namespace Slider
    {
        constexpr ImVec4 TRACK_COLOR = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
        constexpr ImVec4 GRAB_COLOR = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);

        constexpr float TRACK_THICKNESS = 0.2f;
        constexpr float GRAB_RADIUS = 100.0f;
        constexpr float GRAB_MIN_SIZE = 5.0f;
    } // namespace Slider

    namespace ComboBox
    {
        constexpr ImVec4 COMBO_BG_COLOR = ImVec4(0.15F, 0.15F, 0.15F, 1.0F);
        constexpr ImVec4 COMBO_BORDER_COLOR = ImVec4(0.0F, 0.0F, 0.0F, 0.0F);
        constexpr ImVec4 TEXT_COLOR = ImVec4(1.0F, 1.0F, 1.0F, 1.0F);
        constexpr ImVec4 BUTTON_HOVERED_COLOR = ImVec4(0.3F, 0.3F, 0.3F, 0.5F);
        constexpr ImVec4 BUTTON_ACTIVE_COLOR = ImVec4(0.3F, 0.3F, 0.3F, 0.5F);
        constexpr ImVec4 POPUP_BG_COLOR = ImVec4(0.12F, 0.12F, 0.12F, 1.0F);

        constexpr float FRAME_ROUNDING = 5.0F;
        constexpr float POPUP_ROUNDING = 2.0F;
    } // namespace ComboBox

    constexpr float HALF_DIVISOR = 2.0F;
    constexpr float BOTTOM_MARGIN = 10.0F;
    constexpr float INPUT_HEIGHT = 100.0F;
    constexpr float CHAT_WINDOW_CONTENT_WIDTH = 750.0F;
} // namespace Config

//-----------------------------------------------------------------------------
// [SECTION] Structs and Enums
//-----------------------------------------------------------------------------

/**
 * @brief A struct to store the ImFont pointers for different markdown styles
 *
 * The MarkdownFonts struct stores the ImFont pointers for different markdown styles,
 * such as regular, bold, italic, bold italic, and code.
 */
struct MarkdownFonts
{
    ImFont *regular = nullptr;
    ImFont *bold = nullptr;
    ImFont *italic = nullptr;
    ImFont *boldItalic = nullptr;
    ImFont *code = nullptr;
};

/**
 * @brief A struct to store the ImFont pointers for different icon fonts
 *
 * The IconFonts struct stores the ImFont pointers for different icon fonts,
 * such as regular and brands.
 */
struct IconFonts
{
    ImFont *regular = nullptr;
    ImFont *solid = nullptr;
    ImFont *brands = nullptr;
};

enum ButtonState
{
    NORMAL,
    DISABLED,
    ACTIVE
};

enum Alignment
{
    LEFT,
    CENTER,
    RIGHT
};

/**
 * @brief A struct to store the configuration for a button
 *
 * The ButtonConfig struct stores the configuration for a button, including the label,
 * icon, size, padding, and the onClick function.
 */
struct ButtonConfig
{
    std::string id;
    std::optional<std::string> label;
    std::optional<std::string> icon;
    ImVec2 size;
    std::optional<float> gap = 5.0F;
    std::function<void()> onClick;
    std::optional<bool> iconSolid;
    std::optional<ImVec4> backgroundColor = Config::Color::TRANSPARENT;
    std::optional<ImVec4> hoverColor = Config::Color::SECONDARY;
    std::optional<ImVec4> activeColor = Config::Color::PRIMARY;
    std::optional<ButtonState> state = ButtonState::NORMAL;
    std::optional<Alignment> alignment = Alignment::CENTER;
};

/**
 * @brief A struct to store the configuration for a label
 *
 * The LabelConfig struct stores the configuration for a label, including the label,
 * icon, size, icon padding, gap, and whether the label is bold.
 */
struct LabelConfig
{
    std::string id;
    std::string label;
    std::optional<std::string> icon = "";
    ImVec2 size;
    std::optional<float> iconPaddingX = 5.0F;
    std::optional<float> iconPaddingY = 5.0F;
    std::optional<float> gap = 5.0F;
    std::optional<bool> isBold = false;
    std::optional<bool> iconSolid = false;
    std::optional<Alignment> alignment = Alignment::CENTER;
};

/**
 * @brief A struct to store each model preset configuration.
 *
 * The ModelPreset struct stores the configuration for each model preset, including
 * the ID, last modified timestamp, name, system prompt, sampling settings, and generation settings.
 */
struct ModelPreset
{
    int id;
    int lastModified;

    std::string name;
    std::string systemPrompt;

    // sampling
    float temperature;
    float top_p;
    // TODO: Use int instead of float
    // I use float right now because ImGui::SliderFloat requires a float
    // so it needed to create a new custom slider for int
    float top_k;
    int random_seed;

    // generation
    // TODO: Use int instead of float
    float max_new_tokens;
    // TODO: Use int instead of float
    float min_length;

    ModelPreset(
        const int id = 0,
        const int lastModified = 0,
        const std::string &name = "",
        const std::string &systemPrompt = "",
        float temperature = 0.7f,
        float top_p = 0.9f,
        float top_k = 50.0f,
        int random_seed = 42,
        float min_length = 0.0f,
        float max_new_tokens = 2048.0f) : id(id), lastModified(lastModified), name(name), systemPrompt(systemPrompt), temperature(temperature),
                                          top_p(top_p), top_k(top_k), random_seed(random_seed),
                                          min_length(min_length), max_new_tokens(max_new_tokens) {}
};

/**
 * @brief A struct to store each chat message
 *
 * The Message struct represents a chat message with the message content, a flag
 * to indicate whether the message is from the user, and a timestamp.
 */
struct Message
{
    int id;
    bool isLiked;
    bool isDisliked;
    std::string role;
    std::string content;
    std::chrono::system_clock::time_point timestamp;

    Message(
        int id = 0,
        const std::string &role = "user",
        const std::string &content = "",
        bool isLiked = false,
        bool isDisliked = false,
        const std::chrono::system_clock::time_point &timestamp = std::chrono::system_clock::now())
        : id(id), isLiked(isLiked), isDisliked(isDisliked),
          role((role == "user" || role == "assistant") ? role : throw std::invalid_argument("Invalid role: " + role)),
          content(content), timestamp(timestamp) {}
};

/**
 * @brief A struct to store the chat history
 *
 * The ChatHistory struct represents the chat history with the chat ID, chat name,
 * and a vector of messages.
 */
struct ChatHistory
{
    int id;
    int lastModified;
    std::string name;
    std::vector<Message> messages;

    ChatHistory(const int id = 0, const int lastModified = 0, const std::string &name = "untitled",
                const std::vector<Message> &messages = {})
        : id(id), lastModified(lastModified), name(name), messages(messages) {}
};

//-----------------------------------------------------------------------------
// [SECTION] Forward Declarations and Global Variables
//-----------------------------------------------------------------------------

// Forward declaration of GLFWwindow to avoid including GLFW/glfw3.h
struct GLFWwindow;

// Forward declaration of global variables

extern MarkdownFonts                        g_mdFonts;
extern IconFonts                            g_iconFonts;
extern std::unique_ptr<class ChatManager>   g_chatManager;
extern std::unique_ptr<class PresetManager> g_presetManager;

//-----------------------------------------------------------------------------
// [SECTION] Classes
//-----------------------------------------------------------------------------

/**
 * @brief A class to manage the chat history
 *
 * The ChatManager class is responsible for loading, saving, and managing the chat history.
 * It provides functionality to switch between chats, create new chats, and handle user and assistant messages.
 * It also provides functionality to export the chat history to a file.
 */
class ChatManager
{
public:
    ChatManager(const std::string &chatsDirectory);

    void createChatsDirectoryIfNotExists();
    void initializeDefaultChatHistory();
    auto getDefaultChatHistories() const -> std::vector<ChatHistory>;

    auto loadChats() -> bool;
    auto saveChat(const ChatHistory &chat, bool createNewFile = false) -> bool;
    auto saveChatToPath(const ChatHistory &chat, const std::string &filePath) -> bool;
    auto deleteChat(const std::string &chatName) -> bool;

    void createNewChat();
    void switchChat(int newIndex);
    auto hasUnsavedChanges() const -> bool;
    void resetCurrentChat();

    auto getChatFilePath(const std::string &chatName) const -> std::string;
    auto isValidChatName(const std::string &name) const -> bool;
    void saveDefaultChatHistory();

    void handleUserMessage(const std::string &messageContent);
    void handleAssistantMessage(const std::string &messageContent);

    auto getCurrentChatIndex() const -> int;
    auto getChatHistory(const int index) const -> ChatHistory;
    auto getChatHistoryCount() const -> int;
    auto getCurrentChatHistory() const -> ChatHistory;
    void setCurrentChatHistory(const ChatHistory &chatHistory);

private:
    std::string chatsPath;
    int currentChatIndex;
    bool hasInitialized;
    ChatHistory defaultChatHistory;
    std::vector<ChatHistory> loadedChats;
    std::vector<ChatHistory> originalChats;

    // Encryption key (hardcoded for now)
    const std::string encryptionKey = "hardcoded_key";

    // Encryption and decryption methods
    auto encryptData(const std::string &data) -> std::string;
    auto decryptData(const std::string &data) -> std::string;
};

/**
 * @brief A class to manage presets for the model settings
 *
 * The PresetManager class is responsible for loading, saving, and deleting model
 * presets. It also provides functionality to switch between presets and reset
 * the current preset to the default values.
 */
class PresetManager
{
public:
    explicit PresetManager(const std::string &presetsDirectory);

    // Core functionality
    auto loadPresets() -> bool;
    auto savePreset(const ModelPreset &preset, bool createNewFile = false) -> bool;
    auto savePresetToPath(const ModelPreset &preset, const std::string &filePath) -> bool;
    auto deletePreset(const std::string &presetName) -> bool;
    void switchPreset(int newIndex);
    void resetCurrentPreset();

    // Getters and setters
    auto getPresets() const -> const std::vector<ModelPreset> & { return loadedPresets; }
    auto getCurrentPreset() const -> const ModelPreset & { return loadedPresets[currentPresetIndex]; }
    auto getCurrentPreset() -> ModelPreset & { return loadedPresets[currentPresetIndex]; }
    auto getCurrentPresetIndex() const -> int { return currentPresetIndex; }
    auto getDefaultPreset() const -> const ModelPreset & { return defaultPreset; }
    auto hasUnsavedChanges() const -> bool;

private:
    std::string presetsPath;
    std::vector<ModelPreset> loadedPresets;
    std::vector<ModelPreset> originalPresets;
    ModelPreset defaultPreset;
    int currentPresetIndex;
    bool hasInitialized = false;

    // Helper methods
    void createPresetsDirectoryIfNotExists();
    void initializeDefaultPreset();
    auto getDefaultPresets() const -> std::vector<ModelPreset>;
    auto getPresetFilePath(const std::string &presetName) const -> std::string;
    auto isValidPresetName(const std::string &name) const -> bool;
    void saveDefaultPresets();
};

//-----------------------------------------------------------------------------
// [SECTION] Function Prototypes
//-----------------------------------------------------------------------------

// Initialization and Cleanup Functions
auto initializeGLFW() -> bool;
auto createWindow() -> GLFWwindow *;
auto initializeGLAD() -> bool;
auto LoadIconFont(ImGuiIO &io, const char *iconFontPath, float fontSize) -> ImFont *;
auto LoadFont(ImGuiIO &imguiIO, const char *fontPath, ImFont *fallbackFont, float fontSize) -> ImFont *;
void setupImGui(GLFWwindow *window);
void mainLoop(GLFWwindow *window);
void cleanup(GLFWwindow *window);

//-----------------------------------------------------------------------------
// [SECTION] Utility Functions
//-----------------------------------------------------------------------------

auto RGBAToImVec4(float r, float g, float b, float a) -> ImVec4;

auto timePointToString(const std::chrono::system_clock::time_point &tp) -> std::string;
auto stringToTimePoint(const std::string &str) -> std::chrono::system_clock::time_point;

//-----------------------------------------------------------------------------
// [SECTION] JSON Serialization and Deserialization Functions
//-----------------------------------------------------------------------------

// Message Serialization and Deserialization
void to_json(json &j, const Message &m);
void from_json(const json &j, Message &m);

// ChatHistory Serialization and Deserialization
void to_json(json &j, const ChatHistory &c);
void from_json(const json &j, ChatHistory &c);

// Model Preset Serialization and Deserialization
void to_json(json &j, const ModelPreset &p);
void from_json(const json &j, ModelPreset &p);

//-----------------------------------------------------------------------------
// [SECTION] Custom UI Functions
//-----------------------------------------------------------------------------

// Custom UI Functions
namespace Widgets
{
    namespace Button
    {
        void render(const ButtonConfig &config);
        void renderGroup(const std::vector<ButtonConfig> &buttons, float startX, float startY, float spacing = Config::Button::SPACING);
    } // namespace Button

    namespace Label
    {
        void render(const LabelConfig &config);
        void render(const LabelConfig &config, ImVec2 rectMin, ImVec2 rectMax);
    } // namespace Label

    namespace InputField
    {
        void setStyle(float frameRounding, const ImVec2 &framePadding, const ImVec4 &bgColor, const ImVec4 &hoverColor, const ImVec4 &activeColor);
        void restoreStyle();
        void handleSubmission(char *inputText, bool &focusInputField, const std::function<void(const std::string &)> &processInput, bool clearInput);
        void renderMultiline(
            const char *label, char *inputTextBuffer, const ImVec2 &inputSize,
            const std::string &placeholderText, ImGuiInputTextFlags inputFlags,
            const std::function<void(const std::string &)> &processInput, bool &focusInputField);
        void render(
            const char *label, char *inputTextBuffer, const ImVec2 &inputSize,
            const std::string &placeholderText, ImGuiInputTextFlags inputFlags,
            const std::function<void(const std::string &)> &processInput, bool &focusInputField);
    } // namespace InputField

    namespace Slider
    {
        void render(const char *label, float &value, float minValue, float maxValue, const float sliderWidth, const char *format = "%.2f", const float paddingX = 5.0F, const float inputWidth = 32.0F);
    } // namespace Slider

    namespace IntInputField
    {
        void render(const char *label, int &value, const float inputWidth, const float paddingX = 5.0F);
    } // namespace IntInputField

    namespace ComboBox
    {
        auto render(const char *label, const char **items, int itemsCount, int &selectedItem, float width) -> bool;
    } // namespace ComboBox

} // namespace Widgets

namespace ChatHistorySidebar
{
    void render(float &sidebarWidth);
    void renderChatHistoryList(float contentWidth);
} // namespace ChatHistorySidebar

namespace ChatWindow
{
    void render(float inputHeight, float leftSidebarWidth, float rightSidebarWidth);
    void renderSidebar(float &sidebarWidth);
    void renderChatHistory(const ChatHistory chatHistory, float contentWidth);
    void renderInputField(float inputHeight, float inputWidth);

    namespace MessageBubble
    {
        void renderMessage(const Message msg, int index, float contentWidth);
        void pushIDAndColors(const Message msg, int index);
        auto calculateDimensions(const Message msg, float windowWidth) -> std::tuple<float, float, float>;
        void renderMessageContent(const Message msg, float bubbleWidth, float bubblePadding);
        void renderTimestamp(const Message msg, float bubblePadding);
        void renderButtons(const Message msg, int index, float bubbleWidth, float bubblePadding);
    } // namespace MessageBubble

} // namespace ChatWindow

namespace ModelPresetSidebar
{
    namespace State
    {
        extern bool g_showSaveAsDialog;
        extern char g_newPresetName[256];
    }

    void render(float &sidebarWidth);
    void renderModelPresetsSelection(const float sidebarWidth);
    void renderSamplingSettings(const float sidebarWidth);
    void renderSaveAsDialog();
    void confirmSaveAsDialog();
    void exportPresets();
} // namespace ModelSettings

#endif // KOLOSAL_H
