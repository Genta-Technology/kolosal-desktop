#pragma once

#include <vector>
#include <string>
#include <chrono>
#include <stdexcept>
#include <sstream>

// nlohmann/json library
#include "json.hpp"

using json = nlohmann::json;

namespace Chat
{
    auto timePointToString(const std::chrono::system_clock::time_point& tp) -> std::string
    {
        std::time_t time = std::chrono::system_clock::to_time_t(tp);
        std::tm tm = *std::localtime(&time);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    auto stringToTimePoint(const std::string& str) -> std::chrono::system_clock::time_point
    {
        std::istringstream iss(str);
        std::tm tm = {};
        iss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        std::time_t time = std::mktime(&tm);
        return std::chrono::system_clock::from_time_t(time);
    }

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
            const std::string& role = "user",
            const std::string& content = "",
            bool isLiked = false,
            bool isDisliked = false,
            const std::chrono::system_clock::time_point& timestamp = std::chrono::system_clock::now())
            : id(id)
            , isLiked(isLiked)
            , isDisliked(isDisliked)
            , role((role == "user" || role == "assistant") // Check if the role is either user or assistant
                ? role
                : throw std::invalid_argument("Invalid role: " + role))
            , content(content)
            , timestamp(timestamp) {
        }
    };

    void to_json(json& j, const Message& msg)
    {
        j = json{
            {"id", msg.id},
            {"isLiked", msg.isLiked},
            {"isDisliked", msg.isDisliked},
            {"role", msg.role},
            {"content", msg.content},
            {"timestamp", timePointToString(msg.timestamp)} };
    }

    void from_json(const json& j, Message& msg)
    {
        msg.id = j.at("id").get<int>();
        msg.isLiked = j.at("isLiked").get<bool>();
        msg.isDisliked = j.at("isDisliked").get<bool>();
        msg.role = j.at("role").get<std::string>();
        msg.content = j.at("content").get<std::string>();
        std::string timestampStr = j.at("timestamp").get<std::string>();
        msg.timestamp = stringToTimePoint(timestampStr);
    }

    struct ChatHistory
    {
        int id;
        int lastModified;
        std::string name;
        std::vector<Message> messages;

        ChatHistory(
            const int id = 0,
            const int lastModified = 0,
            const std::string& name = "untitled",
            const std::vector<Message>& messages = {})
            : id(id)
            , lastModified(lastModified)
            , name(name)
            , messages(messages) {
        }
    };

    void to_json(json& j, const ChatHistory& chatHistory)
    {
        j = json{
            {"id", chatHistory.id},
            {"lastModified", chatHistory.lastModified},
            {"name", chatHistory.name},
            {"messages", chatHistory.messages} };
    }

    void from_json(const json& j, ChatHistory& chatHistory)
    {
        j.at("id").get_to(chatHistory.id);
        j.at("lastModified").get_to(chatHistory.lastModified);
        j.at("name").get_to(chatHistory.name);
        j.at("messages").get_to(chatHistory.messages);
    }

} // namespace Chat