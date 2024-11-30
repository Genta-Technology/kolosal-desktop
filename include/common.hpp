#pragma once

#include <chrono>
#include <string>
#include <sstream>
#include <iomanip>

inline auto timePointToString(const std::chrono::system_clock::time_point& tp) -> std::string
{
	std::time_t time = std::chrono::system_clock::to_time_t(tp);
	std::tm tm = *std::localtime(&time);
	std::ostringstream oss;
	oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
	return oss.str();
}

inline auto stringToTimePoint(const std::string& str) -> std::chrono::system_clock::time_point
{
	std::istringstream iss(str);
	std::tm tm = {};
	iss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
	std::time_t time = std::mktime(&tm);
	return std::chrono::system_clock::from_time_t(time);
}

inline auto RGBAToImVec4(const float r, const float g, const float b, const float a) -> ImVec4
{
	return ImVec4(r / 255, g / 255, b / 255, a / 255);
}