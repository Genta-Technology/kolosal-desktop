#pragma once

#include "imgui.h"
#include "config.hpp"
#include "ui/widgets.hpp"
#include "chat/chat_manager.hpp"

inline void pushIDAndColors(const Chat::Message msg, int index)
{
    ImGui::PushID(index);

    // Set background color to #2f2f2f for user
    ImVec4 bgColor = ImVec4(
        Config::UserColor::COMPONENT,
        Config::UserColor::COMPONENT,
        Config::UserColor::COMPONENT,
        1.0F);

    // Set background color to transparent for assistant
    if (msg.role == "assistant")
    {
        bgColor = ImVec4(0.0F, 0.0F, 0.0F, 0.0F);
    }

    ImGui::PushStyleColor(ImGuiCol_ChildBg, bgColor);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0F, 1.0F, 1.0F, 1.0F)); // White text
}

inline auto calculateDimensions(const Chat::Message msg, float windowWidth) -> std::tuple<float, float, float>
{
    float bubbleWidth = windowWidth * Config::Bubble::WIDTH_RATIO;
    float bubblePadding = Config::Bubble::PADDING;
    float paddingX = windowWidth - bubbleWidth - Config::Bubble::RIGHT_PADDING;

    if (msg.role == "assistant")
    {
        bubbleWidth = windowWidth;
        paddingX = 0;
    }

    return { bubbleWidth, bubblePadding, paddingX };
}

inline void renderMessageContent(const Chat::Message msg, float bubbleWidth, float bubblePadding)
{
    ImGui::SetCursorPosX(bubblePadding);
    ImGui::SetCursorPosY(bubblePadding);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + bubbleWidth - (bubblePadding * 2));
    ImGui::TextWrapped("%s", msg.content.c_str());
    ImGui::PopTextWrapPos();
}

inline void renderTimestamp(const Chat::Message msg, float bubblePadding)
{
    // Set timestamp color to a lighter gray
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7F, 0.7F, 0.7F, 1.0F)); // Light gray for timestamp

    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - ImGui::GetTextLineHeightWithSpacing() // Align timestamp at the bottom
        - (bubblePadding - Config::Timing::TIMESTAMP_OFFSET_Y));
    ImGui::SetCursorPosX(bubblePadding); // Align timestamp to the left
    ImGui::TextWrapped("%s", timePointToString(msg.timestamp).c_str());

    ImGui::PopStyleColor(); // Restore original text color
}

inline void renderButtons(const Chat::Message msg, int index, float bubbleWidth, float bubblePadding)
{
    ImVec2 textSize = ImGui::CalcTextSize(msg.content.c_str(), nullptr, true, bubbleWidth - bubblePadding * 2);
    float buttonPosY = textSize.y + bubblePadding;

    if (msg.role == "user")
    {
        ButtonConfig copyButtonConfig;
        copyButtonConfig.id = "##copy" + std::to_string(index);
        copyButtonConfig.label = std::nullopt;
        copyButtonConfig.icon = ICON_MS_CONTENT_COPY;
        copyButtonConfig.size = ImVec2(Config::Button::WIDTH, 0);
        copyButtonConfig.onClick = [&msg]()
            {
                ImGui::SetClipboardText(msg.content.c_str());
                std::cout << "Copied message content to clipboard" << std::endl;
            };
        std::vector<ButtonConfig> userButtons = { copyButtonConfig };

        Button::renderGroup(
            userButtons,
            bubbleWidth - bubblePadding - Config::Button::WIDTH,
            buttonPosY);
    }
    else
    {
        ButtonConfig likeButtonConfig;
        likeButtonConfig.id = "##like" + std::to_string(index);
        likeButtonConfig.label = std::nullopt;
        likeButtonConfig.icon = ICON_MS_THUMB_UP;
        likeButtonConfig.size = ImVec2(Config::Button::WIDTH, 0);
        likeButtonConfig.onClick = [index]()
            {
                std::cout << "Like button clicked for message " << index << std::endl;
            };

        ButtonConfig dislikeButtonConfig;
        dislikeButtonConfig.id = "##dislike" + std::to_string(index);
        dislikeButtonConfig.label = std::nullopt;
        dislikeButtonConfig.icon = ICON_MS_THUMB_DOWN;
        dislikeButtonConfig.size = ImVec2(Config::Button::WIDTH, 0);
        dislikeButtonConfig.onClick = [index]()
            {
                std::cout << "Dislike button clicked for message " << index << std::endl;
            };

        std::vector<ButtonConfig> assistantButtons = { likeButtonConfig, dislikeButtonConfig };

        Button::renderGroup(
            assistantButtons,
            bubbleWidth - bubblePadding * 2 - 10 - (2 * Config::Button::WIDTH + Config::Button::SPACING),
            buttonPosY);
    }
}

inline void renderMessage(const Chat::Message& msg, int index, float contentWidth)
{
    pushIDAndColors(msg, index);
    float windowWidth = contentWidth;
    auto [bubbleWidth, bubblePadding, paddingX] = calculateDimensions(msg, windowWidth);

    ImVec2 textSize = ImGui::CalcTextSize(msg.content.c_str(), nullptr, true, bubbleWidth - bubblePadding * 2);
    float estimatedHeight = textSize.y + bubblePadding * 2 + ImGui::GetTextLineHeightWithSpacing();

    ImGui::SetCursorPosX(paddingX);

    if (msg.role == "user")
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, Config::InputField::CHILD_ROUNDING);
    }

    ImGui::BeginGroup();
    ImGui::BeginChild(
        ("MessageCard" + std::to_string(index)).c_str(),
        ImVec2(bubbleWidth, estimatedHeight),
        false,
        ImGuiWindowFlags_NoScrollbar
    );

    renderMessageContent(msg, bubbleWidth, bubblePadding);
	ImGui::Spacing();
    renderTimestamp(msg, bubblePadding);
    renderButtons(msg, index, bubbleWidth, bubblePadding);

    ImGui::EndChild();
    ImGui::EndGroup();

    if (msg.role == "user") {
        ImGui::PopStyleVar();
    }

    ImGui::PopStyleColor(2);
    ImGui::PopID();
    ImGui::Spacing();
}

inline void renderChatHistory(const Chat::ChatHistory chatHistory, float contentWidth)
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
    const std::vector<Chat::Message>& messages = chatHistory.messages;
    for (size_t i = 0; i < messages.size(); ++i)
    {
        renderMessage(messages[i], static_cast<int>(i), contentWidth);
    }

    // If the user was at the bottom and new messages were added, scroll to bottom
    if (newMessageAdded && isAtBottom)
    {
        ImGui::SetScrollHereY(1.0F);
    }

    // Update the last message count
    lastMessageCount = currentMessageCount;
}

static std::string newChatName;

inline void renderRenameChatDialog(bool& showRenameChatDialog)
{
    if (showRenameChatDialog)
    {
        ImGui::OpenPopup("Rename Chat");

        // Reset the flag to prevent the dialog from opening multiple times
        showRenameChatDialog = false;
    }

    // Change the window title background color
    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.125F, 0.125F, 0.125F, 1.0F));       // Inactive state color
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.125F, 0.125F, 0.125F, 1.0F)); // Active state color

    // Apply rounded corners to the window
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);

    if (ImGui::BeginPopupModal("Rename Chat", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        static bool focusNewChatName = true;

        // Populate newChatName if it's empty (e.g., dialog is opened for the first time)
        if (newChatName.empty())
        {
            auto currentChatName = Chat::ChatManager::getInstance().getCurrentChatName();
            if (currentChatName.has_value())
            {
                newChatName = currentChatName.value();
            }
        }

        // Input parameters for processing the input
        auto processInput = [](const std::string& input) {
            Chat::ChatManager::getInstance().renameCurrentChat(input);
            ImGui::CloseCurrentPopup();
            newChatName.clear(); // Safely clear the string
            };

        InputFieldConfig inputConfig(
            "##newchatname",            // ID
            ImVec2(250, 0),             // Size
            newChatName,                // Input text buffer
            focusNewChatName);          // Focus
        inputConfig.flags = ImGuiInputTextFlags_EnterReturnsTrue;
        inputConfig.processInput = processInput;
        inputConfig.frameRounding = 5.0F;
        InputField::render(inputConfig);

        ImGui::Spacing();

        // Configure the confirm button
        ButtonConfig confirmRename;
        confirmRename.id = "##confirmRename";
        confirmRename.label = "Rename";
        confirmRename.icon = std::nullopt;
        confirmRename.size = ImVec2(122.5F, 0);
        confirmRename.onClick = [&]() {
            Chat::ChatManager::getInstance().renameCurrentChat(newChatName);
            ImGui::CloseCurrentPopup();
            newChatName.clear(); // Clear string after renaming
            };
        confirmRename.backgroundColor = RGBAToImVec4(26, 95, 180, 255);
        confirmRename.hoverColor = RGBAToImVec4(53, 132, 228, 255);
        confirmRename.activeColor = RGBAToImVec4(26, 95, 180, 255);

        // Configure the cancel button
        ButtonConfig cancelRename;
        cancelRename.id = "##cancelRename";
        cancelRename.label = "Cancel";
        cancelRename.icon = std::nullopt;
        cancelRename.size = ImVec2(122.5F, 0);
        cancelRename.onClick = [&]() {
            ImGui::CloseCurrentPopup();
            newChatName.clear(); // Clear string on cancel
            };
        cancelRename.backgroundColor = RGBAToImVec4(26, 95, 180, 255);
        cancelRename.hoverColor = RGBAToImVec4(53, 132, 228, 255);
        cancelRename.activeColor = RGBAToImVec4(26, 95, 180, 255);

        std::vector<ButtonConfig> renameChatDialogButtons = { confirmRename, cancelRename };
        Button::renderGroup(renameChatDialogButtons, ImGui::GetCursorPosX(), ImGui::GetCursorPosY(), 10);

        ImGui::EndPopup();
    }

    // Revert to the previous style
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
}

inline void renderChatFeatureButtons(const float startX = 0, const float startY = 0)
{
    static bool openModal = false;

    // Configure the button
    std::vector<ButtonConfig> buttons;

    ButtonConfig openModelManager;
    openModelManager.id = "##openModalButton";
    openModelManager.label = "Open Modal";
    openModelManager.size = ImVec2(100, 0);
    openModelManager.alignment = Alignment::LEFT;
    openModelManager.onClick = [&]() { openModal = true; };

    buttons.push_back(openModelManager);

    // Render the button using renderGroup
    Button::renderGroup(buttons, startX, startY);

    // Open the modal window if the button was clicked
    if (openModal)
    {
        ImGui::OpenPopup("MyModal");
        openModal = false;
    }

    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.0F, 0.0F, 0.0F, 0.5F));

    ImVec2 windowSize = ImGui::GetWindowSize();
    ImVec2 modalSize = ImVec2(windowSize.x * 0.9F, windowSize.y * 0.9F);

    // Center the modal window
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(modalSize);

    // Render the modal window
    if (ImGui::BeginPopupModal("MyModal", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar))
    {
        // Add X button in top-right corner
        float xButtonSize = 32.0f;
        float padding = 10.0f;
        ImVec2 windowSize = ImGui::GetWindowSize();

        // Save cursor pos
        ImVec2 originalPos = ImGui::GetCursorPos();

        ButtonConfig closeButton;
        closeButton.id = "##closeModal";
        closeButton.label = "X";
        closeButton.size = ImVec2(xButtonSize, xButtonSize);
        closeButton.onClick = []() { ImGui::CloseCurrentPopup(); };

        std::vector<ButtonConfig> closeButtons = { closeButton };
        Button::renderGroup(closeButtons, windowSize.x - xButtonSize - 16, 16);

        // Restore cursor position
        ImGui::SetCursorPos(ImVec2(originalPos.x + 16, originalPos.y + 16));

        LabelConfig modalTitle;
        modalTitle.id = "##modalTitle";
        modalTitle.label = "Model Manager";
        modalTitle.size = ImVec2(0, 0);
		modalTitle.fontType = FontsManager::BOLD;
        modalTitle.alignment = Alignment::LEFT;
        Label::render(modalTitle);

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();

        // start content

        {
            // Create a card for a model
            ImGui::BeginGroup();

            // Set the card background color
            ImGui::PushStyleColor(ImGuiCol_ChildBg, RGBAToImVec4(37, 37, 37, 255));

            // Set the card border rounding
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0F);

            ImGui::BeginChild("ModelCard", ImVec2(200, 200), true);

            // Render the model card content
            ImGui::Text("meta-llama/Llama-3.2-1B");
            ImGui::Text("Model Description");

            ImGui::EndChild();

			if (ImGui::IsItemHovered())
			{
                // Push a new style color for the border
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0F, 1.0F, 1.0F, 1.0F)); // White border
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0F); // Border size

                // Draw the border
                ImGui::RenderFrameBorder(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), 8.0F);

                // Pop the style color and var
                ImGui::PopStyleVar();
                ImGui::PopStyleColor();
			}

            ImGui::PopStyleVar();
            ImGui::PopStyleColor();

            ImGui::EndGroup();
        }

        ImGui::EndPopup();
    }

    ImGui::PopStyleColor();
}

inline void renderInputField(const float inputHeight, const float inputWidth)
{
    static std::string inputTextBuffer(Config::InputField::TEXT_SIZE, '\0');
    static bool focusInputField = true;

    // Define the input size
    ImVec2 inputSize = ImVec2(inputWidth, inputHeight);

    // Define a lambda to process the submitted input
    auto processInput = [&](const std::string& input)
        {
            auto& chatManager = Chat::ChatManager::getInstance();
            auto currentChat = chatManager.getCurrentChat();

            // Check if we have a current chat
            if (!currentChat.has_value())
            {
                throw std::runtime_error("No chat available to send message to");
            }

            // Handle user message
            {
                Chat::Message userMessage;
                userMessage.id = static_cast<int>(currentChat.value().messages.size()) + 1;
                userMessage.role = "user";
                userMessage.content = input;

                // Add message directly to current chat
                chatManager.addMessageToCurrentChat(userMessage);
            }

            // Handle assistant response
            // TODO: Implement assistant response through callback
            {
                Chat::Message assistantMessage;
                assistantMessage.id = static_cast<int>(currentChat.value().messages.size()) + 2;
                assistantMessage.role = "assistant";
                assistantMessage.content = "Hello! I am an assistant. How can I help you today?";

                chatManager.addMessageToCurrentChat(assistantMessage);
            }
        };

    // input field settings
    InputFieldConfig inputConfig(
        "##chatinput",	  													      // ID
        ImVec2(inputSize.x, inputSize.y - Config::Font::DEFAULT_FONT_SIZE - 20),  // Size (excluding button height)
        inputTextBuffer,  														  // Input text buffer
        focusInputField); 														  // Focus
    {
        inputConfig.placeholderText = "Type a message and press Enter to send (Ctrl+Enter or Shift+Enter for new line)";
        inputConfig.flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CtrlEnterForNewLine | ImGuiInputTextFlags_ShiftEnterForNewLine;
        inputConfig.processInput = processInput;
    }

    // Set background color and create child window
    ImVec2 screenPos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Draw background rectangle
    drawList->AddRectFilled(
        screenPos,
        ImVec2(screenPos.x + inputWidth, screenPos.y + inputHeight),
        ImGui::ColorConvertFloat4ToU32(Config::InputField::INPUT_FIELD_BG_COLOR),
        Config::InputField::FRAME_ROUNDING  // corner rounding
    );

    ImGui::BeginGroup();

    // Render the input field
    InputField::renderMultiline(inputConfig);

    {
        // Calculate position for feature buttons
        // Get current cursor position for relative positioning
        ImVec2 cursorPos = ImGui::GetCursorPos();
        float buttonX = cursorPos.x + 10;
        float buttonY = cursorPos.y;

        // Render the feature buttons
        renderChatFeatureButtons(buttonX, buttonY);
    }

    ImGui::EndGroup();
}

inline void renderChatWindow(float inputHeight, float leftSidebarWidth, float rightSidebarWidth)
{
    ImGuiIO& imguiIO = ImGui::GetIO();

    // Calculate the size of the chat window based on the sidebar width
    ImVec2 windowSize = ImVec2(imguiIO.DisplaySize.x - rightSidebarWidth - leftSidebarWidth, imguiIO.DisplaySize.y - Config::TITLE_BAR_HEIGHT);

    // Set window to cover the remaining display area
    ImGui::SetNextWindowPos(ImVec2(leftSidebarWidth, Config::TITLE_BAR_HEIGHT), ImGuiCond_Always);
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
    float renameButtonWidth = contentWidth;
    static bool showRenameChatDialog = false;

    // Center the rename button horizontally
    if (paddingX > 0.0F)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + paddingX);
    }

    // Render the rename button
    ButtonConfig renameButtonConfig;
    renameButtonConfig.id = "##renameChat";
    renameButtonConfig.label = Chat::ChatManager::getInstance().getCurrentChatName();
    renameButtonConfig.size = ImVec2(renameButtonWidth, 30);
    renameButtonConfig.gap = 10.0F;
    renameButtonConfig.onClick = []()
        {
            showRenameChatDialog = true;
        };
    renameButtonConfig.alignment = Alignment::CENTER;
    renameButtonConfig.hoverColor = ImVec4(0.1, 0.1, 0.1, 0.5);
    Button::render(renameButtonConfig);

    // Render the rename chat dialog
    renderRenameChatDialog(showRenameChatDialog);

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();

    // Center the content horizontally
    if (paddingX > 0.0F)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + paddingX);
    }

    // Begin the main scrolling region for the chat history
    float availableHeight = ImGui::GetContentRegionAvail().y - inputHeight - Config::BOTTOM_MARGIN;
    ImGui::BeginChild("ChatHistoryRegion", ImVec2(contentWidth, availableHeight), false, ImGuiWindowFlags_NoScrollbar);

    // Render chat history
    renderChatHistory(Chat::ChatManager::getInstance().getCurrentChat().value(), contentWidth);

    ImGui::EndChild(); // End of ChatHistoryRegion

    // Add some spacing or separator if needed
    ImGui::Spacing();

    // Center the input field horizontally by calculating left padding
    float inputFieldPaddingX = (availableWidth - contentWidth) / 2.0F;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + inputFieldPaddingX);

    // Render the input field at the bottom, centered
    renderInputField(inputHeight, contentWidth);

    ImGui::End(); // End of Chatbot window

    // Restore the window border size
    ImGui::PopStyleVar();
}