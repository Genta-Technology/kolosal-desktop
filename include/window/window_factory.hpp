#pragma once

#ifdef _WIN32
#include "win32_window.hpp"
#endif

#include <memory>

class WindowFactory {
public:
	static std::unique_ptr<Window> createWindow()
	{
#ifdef _WIN32
		HINSTANCE hInstance = GetModuleHandle(NULL);
		return std::make_unique<Win32Window>(hInstance);
#else
		// TODO: Implement for other platforms
#endif
	}
};