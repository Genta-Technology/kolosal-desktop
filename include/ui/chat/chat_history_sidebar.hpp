#pragma once

#include "imgui.h"
#include "config.hpp"
#include "ui/widgets.hpp"
#include "chat/chat_manager.hpp"

inline void renderChatHistoryList(ImVec2 contentArea)
{
	// Render chat history buttons scroll region
	ImGui::BeginChild("ChatHistoryButtons", contentArea, false, ImGuiWindowFlags_NoScrollbar);

	// Get sorted chats from ChatManager
	const auto& chats = Chat::ChatManager::getInstance().getChats();
	const auto currentChatName = Chat::ChatManager::getInstance().getCurrentChatName();

	for (const auto& chat : chats)
	{
		ButtonConfig chatButtonConfig;
		chatButtonConfig.id = "##chat" + std::to_string(chat.id);
		chatButtonConfig.label = chat.name;
		chatButtonConfig.icon = ICON_FA_COMMENT;
		chatButtonConfig.size = ImVec2(contentArea.x - 20, 0);
		chatButtonConfig.gap = 10.0F;
		chatButtonConfig.onClick = [chatName = chat.name]() {
			Chat::ChatManager::getInstance().switchToChat(chatName);
			};

		// Set active state if this is the current chat
		chatButtonConfig.state = (currentChatName && *currentChatName == chat.name)
			? ButtonState::ACTIVE
			: ButtonState::NORMAL;

		chatButtonConfig.alignment = Alignment::LEFT;

		// Add tooltip showing last modified time
		if (ImGui::IsItemHovered()) {
			std::time_t time = static_cast<std::time_t>(chat.lastModified);
			std::string timeStr = std::ctime(&time);
			ImGui::SetTooltip("Last modified: %s", timeStr.c_str());
		}

		Button::render(chatButtonConfig);
		ImGui::Spacing();
	}

	ImGui::EndChild();
}

inline void renderChatHistorySidebar(float& sidebarWidth)
{
	ImGuiIO& io = ImGui::GetIO();
	const float sidebarHeight = io.DisplaySize.y - Config::TITLE_BAR_HEIGHT;

	ImGui::SetNextWindowPos(ImVec2(0, Config::TITLE_BAR_HEIGHT), ImGuiCond_Always);
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

	LabelConfig labelConfig;
	labelConfig.id = "##chathistory";
	labelConfig.label = "Recents";
	labelConfig.size = ImVec2(Config::Icon::DEFAULT_FONT_SIZE, 0);
	labelConfig.iconPaddingX = 10.0F;
	labelConfig.isBold = true;
	Label::render(labelConfig);

	// Calculate label height
	ImVec2 labelSize = ImGui::CalcTextSize(labelConfig.label.c_str());
	float labelHeight = labelSize.y;

	// Button dimensions
	float buttonHeight = 24.0f;

	ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 28);
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ((labelHeight - buttonHeight) / 2.0f));

	ButtonConfig createNewChatButtonConfig;
	createNewChatButtonConfig.id = "##createNewChat";
	createNewChatButtonConfig.icon = ICON_FA_PEN_TO_SQUARE;
	createNewChatButtonConfig.size = ImVec2(buttonHeight, 24);
	createNewChatButtonConfig.onClick = []() {
		Chat::ChatManager::getInstance().createNewChat(
			Chat::ChatManager::getDefaultChatName() + " " + std::to_string(Chat::ChatManager::getInstance().getChatsSize()));
		};
	createNewChatButtonConfig.alignment = Alignment::CENTER;
	Button::render(createNewChatButtonConfig);

	ImGui::Spacing();

	renderChatHistoryList(ImVec2(sidebarWidth, sidebarHeight - labelHeight));

	ImGui::End();
}