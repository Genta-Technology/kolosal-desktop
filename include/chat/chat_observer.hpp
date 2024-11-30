#pragma once

#include "chat_history.hpp"

namespace Chat
{
	/**
	 * @brief Interface for chat observers
	 */
	class IChatObserver {
	public:
		virtual ~IChatObserver() = default;
		virtual void onChatModified(const ChatHistory& chat) = 0;
		virtual void onChatDeleted(const std::string& chatName) = 0;
		virtual void onChatCreated(const ChatHistory& chat) = 0;
		virtual void onCurrentChatChanged(const std::string& oldChat, const std::string& chatName) = 0;
	};

} // namespace Chat