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
#include "chat/chat_manager.hpp"
#include "config.hpp"
#include "ui/widgets.hpp"
#include "ui/chat/chat_history_sidebar.hpp"
#include "ui/chat/chat_section.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <iostream>
#include <cstring>
#include <stdexcept>
#include <system_error>
#include <sstream>
#include <iomanip>
#include <array>
#include <fstream>
#include <ctime>

//-----------------------------------------------------------------------------
// [SECTION] Global Variables
//-----------------------------------------------------------------------------

std::unique_ptr<BorderlessWindow> g_borderlessWindow;
HGLRC                             g_openglContext = nullptr;
HDC                               g_deviceContext = nullptr;

MarkdownFonts                     g_mdFonts;
IconFonts                         g_iconFonts;
std::unique_ptr<PresetManager>    g_presetManager;

//-----------------------------------------------------------------------------
// [SECTION] Shaders
//-----------------------------------------------------------------------------

GLuint g_shaderProgram   = 0;
GLuint g_gradientTexture = 0;

const char* g_quadVertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

void main()
{
    TexCoord = aTexCoord;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

const char* g_quadFragmentShaderSource = R"(
#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D gradientTexture;
uniform float uTransitionProgress;

void main()
{
    vec4 color = texture(gradientTexture, TexCoord);
    color.a *= uTransitionProgress; // Adjust the alpha based on transition progress
    FragColor = color;
}
)";

GLuint g_quadVAO = 0;
GLuint g_quadVBO = 0;
GLuint g_quadEBO = 0;

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
    initializeDefaultPreset();
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

    return current.name           != original.name          ||
           current.systemPrompt   != original.systemPrompt  ||
           current.temperature    != original.temperature   ||
           current.top_p          != original.top_p         ||
           current.top_k          != original.top_k         ||
           current.random_seed    != original.random_seed   ||
           current.min_length     != original.min_length    ||
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
    if (strlen(name.data()) == 0 || name.length() > 256) // 256 + 1 for null terminator
    {
		std::cerr << "Preset name is empty or too long: " << name << std::endl;
        return false;
    }

    // Check for invalid filesystem characters
    const std::string invalidChars = R"(<>:"/\|?*)";
	if (name.find_first_of(invalidChars) != std::string::npos)
	{
		std::cerr << "Preset name contains invalid characters: " << name << std::endl;
		return false;
	}

    return true;
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
// [SECTION] Borderless Window Class
//-----------------------------------------------------------------------------

namespace 
{
    // we cannot just use WS_POPUP style
    // WS_THICKFRAME: without this the window cannot be resized and so aero snap, de-maximizing and minimizing won't work
    // WS_SYSMENU: enables the context menu with the move, close, maximize, minize... commands (shift + right-click on the task bar item)
    // WS_CAPTION: enables aero minimize animation/transition
    // WS_MAXIMIZEBOX, WS_MINIMIZEBOX: enable minimize/maximize
    enum class Style : DWORD 
    {
        windowed = WS_OVERLAPPEDWINDOW | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
        aero_borderless = WS_POPUP | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX,
        basic_borderless = WS_POPUP | WS_THICKFRAME | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX
    };

    auto maximized(HWND hwnd) -> bool 
    {
        WINDOWPLACEMENT placement;
        if (!::GetWindowPlacement(hwnd, &placement)) 
        {
            return false;
        }

        return placement.showCmd == SW_MAXIMIZE;
    }

    /* Adjust client rect to not spill over monitor edges when maximized.
     * rect(in/out): in: proposed window rect, out: calculated client rect
     * Does nothing if the window is not maximized.
     */
    auto adjust_maximized_client_rect(HWND window, RECT& rect) -> void 
    {
        if (!maximized(window)) 
        {
            return;
        }

        auto monitor = ::MonitorFromWindow(window, MONITOR_DEFAULTTONULL);
        if (!monitor) 
        {
            return;
        }

        MONITORINFO monitor_info{};
        monitor_info.cbSize = sizeof(monitor_info);
        if (!::GetMonitorInfoW(monitor, &monitor_info)) 
        {
            return;
        }

        // when maximized, make the client area fill just the monitor (without task bar) rect,
        // not the whole window rect which extends beyond the monitor.
        rect = monitor_info.rcWork;
    }

    auto last_error(const std::string& message) -> std::system_error 
    {
        return std::system_error(
            std::error_code(::GetLastError(), std::system_category()),
            message
        );
    }

    auto window_class(WNDPROC wndproc, HINSTANCE hInstance) -> const wchar_t* 
    {
        static const wchar_t* window_class_name = [&] {
            WNDCLASSEXW wcx{};
            wcx.cbSize        = sizeof(wcx);
            wcx.style         = CS_HREDRAW | CS_VREDRAW;
            wcx.hInstance     = hInstance;
            wcx.lpfnWndProc   = wndproc;
            wcx.lpszClassName = L"BorderlessWindowClass";
            wcx.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
            wcx.hCursor       = ::LoadCursorW(hInstance, IDC_ARROW);
			wcx.hIcon         = ::LoadIconW(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
            wcx.hIconSm       = ::LoadIconW(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
            const ATOM result = ::RegisterClassExW(&wcx);
            if (!result) {
                throw last_error("failed to register window class");
            }
            return wcx.lpszClassName;
            }();
        return window_class_name;
    }

    auto composition_enabled() -> bool 
    {
        BOOL composition_enabled = FALSE;
        bool success = ::DwmIsCompositionEnabled(&composition_enabled) == S_OK;
        return composition_enabled && success;
    }

    auto select_borderless_style() -> Style 
    {
        return composition_enabled() ? Style::aero_borderless : Style::basic_borderless;
    }

    auto set_shadow(HWND handle, bool enabled) -> void 
    {
        if (composition_enabled()) 
        {
            static const MARGINS shadow_state[2]{ { 0,0,0,0 },{ 1,1,1,1 } };
            ::DwmExtendFrameIntoClientArea(handle, &shadow_state[enabled]);
        }
    }

    auto create_window(WNDPROC wndproc, HINSTANCE hInstance, void* userdata) -> HWND
    {
        auto handle = CreateWindowExW(
            0, window_class(wndproc, hInstance), L"Kolosal AI",
            static_cast<DWORD>(Style::aero_borderless), CW_USEDEFAULT, CW_USEDEFAULT,
            1280, 720, nullptr, nullptr, hInstance, userdata
        );
        if (!handle) 
        {
            throw last_error("failed to create window");
        }
        return handle;
    }
}

BorderlessWindow::BorderlessWindow(HINSTANCE hInstance)
    : hInstance(hInstance), 
      handle{ create_window(&BorderlessWindow::WndProc, hInstance, this) },
      borderless_drag{ false },
      borderless_resize{ true }
{
    HICON hIcon = LoadIconW(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
    if (hIcon)
    {
        SendMessage(handle, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        SendMessage(handle, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    }

    set_borderless(borderless);
    set_borderless_shadow(borderless_shadow);
    ::ShowWindow(handle, SW_SHOW);
}

void BorderlessWindow::set_borderless(bool enabled) 
{
    Style new_style = (enabled) ? select_borderless_style() : Style::windowed;
    Style old_style = static_cast<Style>(::GetWindowLongPtrW(handle, GWL_STYLE));

    if (new_style != old_style) 
    {
        borderless = enabled;

        ::SetWindowLongPtrW(handle, GWL_STYLE, static_cast<LONG>(new_style));

        // when switching between borderless and windowed, restore appropriate shadow state
        set_shadow(handle, borderless_shadow && (new_style != Style::windowed));

        // redraw frame
        ::SetWindowPos(handle, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);
        ::ShowWindow(handle, SW_SHOW);
    }
}

void BorderlessWindow::set_borderless_shadow(bool enabled) 
{
    if (borderless) 
    {
        borderless_shadow = enabled;
        set_shadow(handle, enabled);
    }
}

LRESULT CALLBACK BorderlessWindow::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) noexcept 
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) 
    {
        return true;
    }

    if (msg == WM_NCCREATE) 
    {
        auto userdata = reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams;
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(userdata));
    }
    if (auto window_ptr = reinterpret_cast<BorderlessWindow*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA))) 
    {
        auto& window = *window_ptr;

        switch (msg) 
        {
        case WM_NCCALCSIZE: 
        {
            if (wparam == TRUE && window.borderless) 
            {
                auto& params = *reinterpret_cast<NCCALCSIZE_PARAMS*>(lparam);
                adjust_maximized_client_rect(hwnd, params.rgrc[0]);
                return 0;
            }
            break;
        }
        case WM_NCHITTEST: 
        {
            if (window.borderless) 
            {
                return window.hit_test(POINT{
                    GET_X_LPARAM(lparam),
                    GET_Y_LPARAM(lparam)
                    });
            }
            break;
        }
        case WM_NCACTIVATE: 
        {
            window.is_active = (wparam != FALSE);
            break;
        }
        case WM_ACTIVATE: 
        {
            window.is_active = (wparam != WA_INACTIVE);
            break;
        }
        case WM_CLOSE: 
        {
            ::DestroyWindow(hwnd);
            return 0;
        }
        case WM_DESTROY: 
        {
            PostQuitMessage(0);
            return 0;
        }
        case WM_SIZE: 
        {
            // Handle window resizing if necessary
            return 0;
        }
        default:
            break;
        }
    }

    return ::DefWindowProcW(hwnd, msg, wparam, lparam);
}

auto BorderlessWindow::hit_test(POINT cursor) const -> LRESULT {
    // Identify borders and corners to allow resizing the window.
    const POINT border{
        ::GetSystemMetrics(SM_CXFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER),
        ::GetSystemMetrics(SM_CYFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER)
    };
    RECT window;
    if (!::GetWindowRect(handle, &window)) {
        return HTNOWHERE;
    }

    // Check if the cursor is within the custom title bar
    if ((cursor.y >= window.top && cursor.y < window.top + Config::TITLE_BAR_HEIGHT) &&
		(cursor.x <= window.right - 45 * 3)) { // 45px * 3 = 135px (close, minimize, maximize buttons)
        return HTCAPTION;
    }

    const auto drag = HTCLIENT; // Always return HTCLIENT for client area

    enum region_mask {
        client = 0b0000,
        left = 0b0001,
        right = 0b0010,
        top = 0b0100,
        bottom = 0b1000,
    };

    const auto result =
        left * (cursor.x < (window.left + border.x)) |
        right * (cursor.x >= (window.right - border.x)) |
        top * (cursor.y < (window.top + border.y)) |
        bottom * (cursor.y >= (window.bottom - border.y));

    switch (result) {
    case left: return borderless_resize ? HTLEFT : HTCLIENT;
    case right: return borderless_resize ? HTRIGHT : HTCLIENT;
    case top: return borderless_resize ? HTTOP : HTCLIENT;
    case bottom: return borderless_resize ? HTBOTTOM : HTCLIENT;
    case top | left: return borderless_resize ? HTTOPLEFT : HTCLIENT;
    case top | right: return borderless_resize ? HTTOPRIGHT : HTCLIENT;
    case bottom | left: return borderless_resize ? HTBOTTOMLEFT : HTCLIENT;
    case bottom | right: return borderless_resize ? HTBOTTOMRIGHT : HTCLIENT;
    case client: return HTCLIENT; // Ensure client area is not draggable
    default: return HTNOWHERE;
    }
}

//-----------------------------------------------------------------------------
// [SECTION] Font and Icon Font Loading Functions
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
auto Fonts::LoadFont(ImGuiIO& imguiIO, const char* fontPath, ImFont* fallbackFont, float fontSize) -> ImFont*
{
    ImFont* font = imguiIO.Fonts->AddFontFromFileTTF(fontPath, fontSize);
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
auto Fonts::LoadIconFont(ImGuiIO& io, const char* iconFontPath, float fontSize) -> ImFont*
{
    static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
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
    ImFont* iconFont = io.Fonts->AddFontFromFileTTF(iconFontPath, fontSize, &icons_config, icons_ranges);
    if (iconFont == nullptr)
    {
        std::cerr << "Failed to load icon font: " << iconFontPath << std::endl;
        return g_mdFonts.regular;
    }

    return iconFont;
}

//-----------------------------------------------------------------------------
// [SECTION] Gradient Color Helper Functions
//-----------------------------------------------------------------------------

void GradientBackground::checkShaderCompileErrors(GLuint shader, const std::string& type)
{
    GLint success;
    GLchar infoLog[1024];
    if (type != "PROGRAM")
    {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "| ERROR::SHADER_COMPILATION_ERROR of type: " << type << "|\n"
                << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    }
}

void GradientBackground::checkProgramLinkErrors(GLuint program)
{
    GLint success;
    GLchar infoLog[1024];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(program, 1024, NULL, infoLog);
        std::cerr << "| ERROR::PROGRAM_LINKING_ERROR |\n"
            << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
    }
}

void GradientBackground::generateGradientTexture(int width, int height)
{
    // Delete the existing texture if it exists
    if (g_gradientTexture != 0)
    {
        glDeleteTextures(1, &g_gradientTexture);
    }

    // Create a new texture
    glGenTextures(1, &g_gradientTexture);
    glBindTexture(GL_TEXTURE_2D, g_gradientTexture);

    // Allocate a buffer to hold the gradient data
    std::vector<unsigned char> gradientData(width * height * 4);

    // Define the start and end colors (RGBA)
    ImVec4 colorTopLeft = ImVec4(0.05f, 0.07f, 0.12f, 1.0f);     // Dark Blue
    ImVec4 colorBottomRight = ImVec4(0.16f, 0.14f, 0.08f, 1.0f); // Dark Green

    // Generate the gradient data
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            // Calculate the interpolation factor 't' based on both x and y
            float t_x = static_cast<float>(x) / (width - 1);
            float t_y = static_cast<float>(y) / (height - 1);
            float t = (t_x + t_y) / 2.0f; // Average of t_x and t_y for diagonal effect

            // Interpolate the colors using 't'
            ImVec4 pixelColor = ImLerp(colorTopLeft, colorBottomRight, t);

            // Convert color components to bytes (0-255)
            unsigned char r = static_cast<unsigned char>(pixelColor.x * 255);
            unsigned char g = static_cast<unsigned char>(pixelColor.y * 255);
            unsigned char b = static_cast<unsigned char>(pixelColor.z * 255);
            unsigned char a = static_cast<unsigned char>(pixelColor.w * 255);

            int index = (y * width + x) * 4;
            gradientData[index + 0] = r;
            gradientData[index + 1] = g;
            gradientData[index + 2] = b;
            gradientData[index + 3] = a;
        }
    }

    // Upload the gradient data to the texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, gradientData.data());

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Unbind the texture
    glBindTexture(GL_TEXTURE_2D, 0);
}

auto GradientBackground::compileShader(GLenum type, const char* source) -> GLuint
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    // Check for compile errors
    checkShaderCompileErrors(shader, (type == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT");

    return shader;
}

auto GradientBackground::createShaderProgram(const char* vertexSource, const char* fragmentSource) -> GLuint
{
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

    // Link shaders into a program
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    // Check for linking errors
    checkProgramLinkErrors(program);

    // Clean up shaders (they are no longer needed after linking)
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}

void GradientBackground::setupFullScreenQuad()
{
    float quadVertices[] = {
        // Positions    // Texture Coords
        -1.0f,  1.0f,    0.0f, 1.0f, // Top-left
        -1.0f, -1.0f,    0.0f, 0.0f, // Bottom-left
         1.0f, -1.0f,    1.0f, 0.0f, // Bottom-right
         1.0f,  1.0f,    1.0f, 1.0f  // Top-right
    };

    unsigned int quadIndices[] = {
        0, 1, 2, // First triangle
        0, 2, 3  // Second triangle
    };

    glGenVertexArrays(1, &g_quadVAO);
    glGenBuffers(1, &g_quadVBO);
    glGenBuffers(1, &g_quadEBO);

    glBindVertexArray(g_quadVAO);

    // Vertex Buffer
    glBindBuffer(GL_ARRAY_BUFFER, g_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    // Element Buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_quadEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadIndices), quadIndices, GL_STATIC_DRAW);

    // Position Attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    // Texture Coordinate Attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);
}

void GradientBackground::renderGradientBackground(HWND hwnd, int display_w, int display_h, float transitionProgress, float easedProgress)
{
    // Get the framebuffer size
    RECT newRect;
    if (GetClientRect(hwnd, &newRect)) {
        int new_display_w = newRect.right - newRect.left;
        int new_display_h = newRect.bottom - newRect.top;

        if (new_display_w != display_w || new_display_h != display_h) {
            display_w = new_display_w;
            display_h = new_display_h;
            // Update gradient texture when window size changes
            generateGradientTexture(display_w, display_h);
            glViewport(0, 0, display_w, display_h);
        }
    }
    else {
        display_w = 800;
        display_h = 600;
    }

    // Set the viewport and clear the screen
    glViewport(0, 0, display_w, display_h);
    glClearColor(0, 0, 0, 0); // Clear with transparent color if blending is enabled
    glClear(GL_COLOR_BUFFER_BIT);

    // Disable depth test and face culling
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    // Render the gradient texture as background
    if (transitionProgress > 0.0f)
    {
        // Enable blending
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Use the shader program
        glUseProgram(g_shaderProgram);

        // Bind the gradient texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_gradientTexture);

        // Set the sampler uniform
        glUniform1i(glGetUniformLocation(g_shaderProgram, "gradientTexture"), 0);

        // Set the transition progress uniform
        GLint locTransitionProgress = glGetUniformLocation(g_shaderProgram, "uTransitionProgress");
        glUniform1f(locTransitionProgress, easedProgress); // Use easedProgress

        // Render the full-screen quad
        glBindVertexArray(g_quadVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        // Unbind the shader program
        glUseProgram(0);

        // Disable blending if necessary
        // glDisable(GL_BLEND);
    }
}

//-----------------------------------------------------------------------------
// [SECTION] Win32 and OpenGL Initialization Functions
//-----------------------------------------------------------------------------

bool initializeOpenGL(HWND hwnd) 
{
    PIXELFORMATDESCRIPTOR pfd = 
    {
        sizeof(PIXELFORMATDESCRIPTOR),   // Size Of This Pixel Format Descriptor
        1,                               // Version Number
        PFD_DRAW_TO_WINDOW |             // Format Must Support Window
        PFD_SUPPORT_OPENGL |             // Format Must Support OpenGL
        PFD_DOUBLEBUFFER,                // Must Support Double Buffering
        PFD_TYPE_RGBA,                   // Request An RGBA Format
        32,                              // Select Our Color Depth
        0, 0, 0, 0, 0, 0,                // Color Bits Ignored
        0,                               // No Alpha Buffer
        0,                               // Shift Bit Ignored
        0,                               // No Accumulation Buffer
        0, 0, 0, 0,                      // Accumulation Bits Ignored
        24,                              // 24Bit Z-Buffer (Depth Buffer)
        8,                               // 8Bit Stencil Buffer
        0,                               // No Auxiliary Buffer
        PFD_MAIN_PLANE,                  // Main Drawing Layer
        0,                               // Reserved
        0, 0, 0                          // Layer Masks Ignored
    };

    g_deviceContext = GetDC(hwnd);
    if (!g_deviceContext) 
    {
        MessageBoxA(nullptr, "Failed to get device context", "Error", MB_OK | MB_ICONERROR);
        return false;
    }

    int pixelFormat = ChoosePixelFormat(g_deviceContext, &pfd);
    if (pixelFormat == 0) 
    {
        MessageBoxA(nullptr, "Failed to choose pixel format", "Error", MB_OK | MB_ICONERROR);
        return false;
    }

    if (!SetPixelFormat(g_deviceContext, pixelFormat, &pfd)) 
    {
        MessageBoxA(nullptr, "Failed to set pixel format", "Error", MB_OK | MB_ICONERROR);
        return false;
    }

    g_openglContext = wglCreateContext(g_deviceContext);
    if (!g_openglContext) 
    {
        MessageBoxA(nullptr, "Failed to create OpenGL context", "Error", MB_OK | MB_ICONERROR);
        return false;
    }

    if (!wglMakeCurrent(g_deviceContext, g_openglContext)) 
    {
        MessageBoxA(nullptr, "Failed to make OpenGL context current", "Error", MB_OK | MB_ICONERROR);
        return false;
    }

    // Initialize GLAD or any OpenGL loader here
    if (!gladLoadGL()) 
    {
        MessageBoxA(nullptr, "Failed to initialize GLAD", "Error", MB_OK | MB_ICONERROR);
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
// [SECTION] ImGui Setup and Main Loop
//-----------------------------------------------------------------------------

/**
 * @brief Sets up the ImGui context and initializes the platform/renderer backends.
 *
 * @param window A pointer to the Win32 window.
 */
void setupImGui(HWND hwnd) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& imguiIO = ImGui::GetIO();

    // Load fonts and set up styles
    g_mdFonts.regular    = Fonts::LoadFont(imguiIO, IMGUI_FONT_PATH_INTER_REGULAR,    imguiIO.Fonts->AddFontDefault(), Config::Font::DEFAULT_FONT_SIZE);
    g_mdFonts.bold       = Fonts::LoadFont(imguiIO, IMGUI_FONT_PATH_INTER_BOLD,       g_mdFonts.regular,               Config::Font::DEFAULT_FONT_SIZE);
    g_mdFonts.italic     = Fonts::LoadFont(imguiIO, IMGUI_FONT_PATH_INTER_ITALIC,     g_mdFonts.regular,               Config::Font::DEFAULT_FONT_SIZE);
    g_mdFonts.boldItalic = Fonts::LoadFont(imguiIO, IMGUI_FONT_PATH_INTER_BOLDITALIC, g_mdFonts.bold,                  Config::Font::DEFAULT_FONT_SIZE);
    g_mdFonts.code       = Fonts::LoadFont(imguiIO, IMGUI_FONT_PATH_FIRACODE_REGULAR, g_mdFonts.regular,               Config::Font::DEFAULT_FONT_SIZE);

    // Load icon fonts
    g_iconFonts.regular  = Fonts::LoadIconFont(imguiIO, IMGUI_FONT_PATH_FA_REGULAR, Config::Icon::DEFAULT_FONT_SIZE);
    g_iconFonts.solid    = Fonts::LoadIconFont(imguiIO, IMGUI_FONT_PATH_FA_SOLID,   Config::Icon::DEFAULT_FONT_SIZE);
    g_iconFonts.brands   = Fonts::LoadIconFont(imguiIO, IMGUI_FONT_PATH_FA_BRANDS,  Config::Icon::DEFAULT_FONT_SIZE);

    imguiIO.FontDefault = g_mdFonts.regular;

    // Enable power save mode
    imguiIO.ConfigFlags |= ImGuiConfigFlags_EnablePowerSavingMode;

    // Adjust ImGui style to match the window's rounded corners
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f; // Match the corner radius
    style.WindowBorderSize = 0.0f; // Disable ImGui's window border

    ImGui::StyleColorsDark();

    // Initialize ImGui Win32 and OpenGL3 backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplOpenGL3_Init("#version 330");
}

GLuint LoadTextureFromFile(const char* filename)
{
    int width, height, channels;
    unsigned char* data = stbi_load(filename, &width, &height, &channels, 4); // Force RGBA
    if (!data)
    {
        fprintf(stderr, "Failed to load texture: %s\n", filename);
        return 0;
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Load texture data into OpenGL
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    // Set texture parameters for scaling
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Optional: Prevent texture wrapping (clamp to edges)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);

    return texture;
}

void titleBar(HWND hwnd)
{
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* draw_list = ImGui::GetForegroundDrawList();

    // Title bar setup
    {
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, Config::TITLE_BAR_HEIGHT)); // Adjust height as needed
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); // No padding
        ImGui::Begin("TitleBar", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBackground);
    }

    // Render the logo
    {
        static GLuint logoTexture = 0;
        static bool textureLoaded = false;

        if (!textureLoaded)
        {
            logoTexture = LoadTextureFromFile(KOLOSAL_LOGO_PATH);
            textureLoaded = true;
        }

        if (logoTexture)
        {
            const float logoWidth = 20.0F;
            ImGui::SetCursorPos(ImVec2(18, (Config::TITLE_BAR_HEIGHT - logoWidth) / 2)); // Position the logo (adjust as needed)
            ImGui::Image((ImTextureID)(uintptr_t)logoTexture, ImVec2(logoWidth, logoWidth)); // Adjust size as needed
            ImGui::SameLine();
        }
    }

    // Title Bar Buttons
    {
        float buttonWidth = 45.0f; // Adjust as needed
        float buttonHeight = Config::TITLE_BAR_HEIGHT; // Same as the title bar height
        float buttonSpacing = 0.0f; // No spacing
        float x = io.DisplaySize.x - buttonWidth * 3;
        float y = 0.0f;

        // Style variables for hover effects
        ImU32 hoverColor = IM_COL32(255, 255, 255, (int)(255 * 0.3f)); // Adjust alpha as needed
        ImU32 closeHoverColor = IM_COL32(232, 17, 35, (int)(255 * 0.5f)); // Red color for close button

        // Minimize button
        {
            ImGui::SetCursorPos(ImVec2(x, y));
            ImGui::PushID("MinimizeButton");
            if (ImGui::InvisibleButton("##MinimizeButton", ImVec2(buttonWidth, buttonHeight)))
            {
                // Handle minimize
                ShowWindow(hwnd, SW_MINIMIZE);
            }

            // Hover effect
            if (ImGui::IsItemHovered())
            {
                ImVec2 p_min = ImGui::GetItemRectMin();
                ImVec2 p_max = ImGui::GetItemRectMax();
                draw_list->AddRectFilled(p_min, p_max, hoverColor);
            }

            // Render minimize icon
            {
                ImVec2 iconPos = ImGui::GetItemRectMin();
                iconPos.x += ((buttonWidth - ImGui::CalcTextSize(ICON_FA_WINDOW_MINIMIZE).x) / 2.0f) - 2.5;
                iconPos.y += ((buttonHeight - ImGui::CalcTextSize(ICON_FA_WINDOW_MINIMIZE).y) / 2.0f) - 5;

                // Select icon font
                ImGui::PushFont(g_iconFonts.regular);
                draw_list->AddText(iconPos, IM_COL32(255, 255, 255, 255), ICON_FA_WINDOW_MINIMIZE);
                ImGui::PopFont();
            }

            ImGui::PopID();

        } // Minimize button

        // Maximize/Restore button
        {
            x += buttonWidth + buttonSpacing;

            // Maximize/Restore button
            ImGui::SetCursorPos(ImVec2(x, y));
            ImGui::PushID("MaximizeButton");
            if (ImGui::InvisibleButton("##MaximizeButton", ImVec2(buttonWidth, buttonHeight)))
            {
                // Handle maximize/restore
                if (IsZoomed(hwnd))
                    ShowWindow(hwnd, SW_RESTORE);
                else
                    ShowWindow(hwnd, SW_MAXIMIZE);
            }

            // Hover effect
            if (ImGui::IsItemHovered())
            {
                ImVec2 p_min = ImGui::GetItemRectMin();
                ImVec2 p_max = ImGui::GetItemRectMax();
                draw_list->AddRectFilled(p_min, p_max, hoverColor);
            }

            // Render maximize or restore icon
            {
                const char* icon = IsZoomed(hwnd) ? ICON_FA_WINDOW_RESTORE : ICON_FA_WINDOW_MAXIMIZE;
                ImVec2 iconPos = ImGui::GetItemRectMin();
                iconPos.x += ((buttonWidth - ImGui::CalcTextSize(icon).x) / 2.0f) - 2.5;
                iconPos.y += (buttonHeight - ImGui::CalcTextSize(icon).y) / 2.0f;

                // Select icon font
                ImGui::PushFont(g_iconFonts.regular);
                draw_list->AddText(iconPos, IM_COL32(255, 255, 255, 255), icon);
                ImGui::PopFont();
            }

            ImGui::PopID();

        } // Maximize/Restore button

        // Close button
        {
            x += buttonWidth + buttonSpacing;

            ImGui::SetCursorPos(ImVec2(x, y));
            ImGui::PushID("CloseButton");
            if (ImGui::InvisibleButton("##CloseButton", ImVec2(buttonWidth, buttonHeight)))
            {
                // Handle close
                PostMessage(hwnd, WM_CLOSE, 0, 0);
            }

            // Hover effect
            if (ImGui::IsItemHovered())
            {
                ImVec2 p_min = ImGui::GetItemRectMin();
                ImVec2 p_max = ImGui::GetItemRectMax();
                draw_list->AddRectFilled(p_min, p_max, closeHoverColor);
            }

            // Render close icon
            {
                ImVec2 p_min = ImGui::GetItemRectMin();
                ImVec2 p_max = ImGui::GetItemRectMax();
                float padding = 18.0F;
                ImU32 symbol_color = IM_COL32(255, 255, 255, 255);
                float thickness = 1.0f;

                draw_list->AddLine(
                    ImVec2(p_min.x + padding - 2, p_min.y + padding + 1),
                    ImVec2(p_max.x - padding + 2, p_max.y - padding),
                    symbol_color,
                    thickness
                );

                draw_list->AddLine(
                    ImVec2(p_max.x - padding + 2, p_min.y + padding),
                    ImVec2(p_min.x + padding - 2, p_max.y - padding - 1),
                    symbol_color,
                    thickness
                );
            }

            ImGui::PopID();

        } // Close button

	} // Title Bar Buttons

    ImGui::End();
    ImGui::PopStyleVar(3);
}

/**
 * @brief The main loop of the application, which handles rendering and event polling.
 *
 * @param window A pointer to the Win32 window.
 */
void mainLoop(HWND hwnd) 
{
    float inputHeight = Config::INPUT_HEIGHT; // Set your desired input field height here

    // Initialize sidebar width with a default value from the configuration
    float chatHistorySidebarWidth = Config::ChatHistorySidebar::SIDEBAR_WIDTH;
    float modelPresetSidebarWidth = Config::ModelPresetSidebar::SIDEBAR_WIDTH;

    // Initialize the chat manager and preset manager
    Chat::initializeChatManager();
    g_presetManager = std::make_unique<PresetManager>(PRESETS_DIRECTORY);

    // Initialize NFD
    NFD_Init();

    // Get initial window size
    int display_w, display_h;
    RECT rect;
    if (GetClientRect(hwnd, &rect)) {
        display_w = rect.right - rect.left;
        display_h = rect.bottom - rect.top;
    }
    else {
        display_w = 1280;
        display_h = 720;
    }

	// Gradient background setup
    GradientBackground::generateGradientTexture(display_w, display_h);
    g_shaderProgram = GradientBackground::createShaderProgram(g_quadVertexShaderSource, g_quadFragmentShaderSource);
    GradientBackground::setupFullScreenQuad();

    // Transition variables on/off focus
    float transitionProgress = 0.0f;
    const float transitionDuration = 0.3f; // Duration in seconds
    bool isTransitioning = false;
    bool targetActiveState = g_borderlessWindow->isActive();
    std::chrono::steady_clock::time_point transitionStartTime;
    bool previousActiveState = g_borderlessWindow->isActive();

    MSG msg = { 0 };
    const double targetFrameTime = 1.0 / 60.0; // Target frame time in seconds (for 30 FPS)

    while (msg.message != WM_QUIT) {
        auto frameStartTime = std::chrono::high_resolution_clock::now();

        if (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            continue;
        }

        // Detect active state changes
        bool currentActiveState = g_borderlessWindow->isActive();
        if (currentActiveState != previousActiveState) {
            isTransitioning = true;
            targetActiveState = currentActiveState;
            transitionStartTime = std::chrono::steady_clock::now();
        }
        previousActiveState = currentActiveState;

        // Update transition progress
        if (isTransitioning) {
            float elapsedTime = std::chrono::duration<float>(std::chrono::steady_clock::now() - transitionStartTime).count();
            float progress = elapsedTime / transitionDuration;
            if (progress >= 1.0f) {
                progress = 1.0f;
                isTransitioning = false;
            }
            if (targetActiveState) {
                transitionProgress = progress; // Fading in
            }
            else {
                transitionProgress = 1.0f - progress; // Fading out
            }
        }
        else {
            transitionProgress = targetActiveState ? 1.0f : 0.0f;
        }

        // Optional: Apply easing function
        float easedProgress = transitionProgress * transitionProgress * (3.0f - 2.0f * transitionProgress);

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

		// Render the title bar
		titleBar(hwnd);

        // Render your UI elements here
        {
            renderChatHistorySidebar(chatHistorySidebarWidth);
            ModelPresetSidebar::render(modelPresetSidebarWidth);
            renderChatWindow(inputHeight, chatHistorySidebarWidth, modelPresetSidebarWidth);
        }

        // Draw the blue border if the window is active
        if (g_borderlessWindow->isActive()) 
        {
            ImDrawList* draw_list = ImGui::GetForegroundDrawList();
            ImGuiIO& io = ImGui::GetIO();
            float thickness = 2.0f;
            ImVec4 color = ImVec4(0.0f, 0.478f, 0.843f, 1.0f); // Blue color
            ImU32 border_color = ImGui::ColorConvertFloat4ToU32(color);

            ImVec2 min = ImVec2(0.0f, 0.0f);
            ImVec2 max = io.DisplaySize;

            float corner_radius = 8.0f;

            draw_list->AddRect(min, max, border_color, corner_radius, 0, thickness);
        }

        // Render the ImGui frame
        ImGui::Render();

        GradientBackground::renderGradientBackground(hwnd, display_w, display_h, transitionProgress, easedProgress);

        // Render ImGui draw data
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SwapBuffers(g_deviceContext);

        // Frame timing code to limit FPS
        auto frameEndTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> frameDuration = frameEndTime - frameStartTime;
        double frameTime = frameDuration.count();

        if (frameTime < targetFrameTime) {
            std::this_thread::sleep_for(std::chrono::duration<double>(targetFrameTime - frameTime));
        }
    }

    NFD_Quit();
}

/**
 * @brief Cleans up ImGui and Win32 resources before exiting the application.
 *
 * @param window A pointer to the Win32 window to be destroyed.
 */
void cleanup() {
    // Clean up shaders resources
    {
        if (g_gradientTexture != 0) {
            glDeleteTextures(1, &g_gradientTexture);
            g_gradientTexture = 0;
        }

        if (g_quadVAO != 0) {
            glDeleteVertexArrays(1, &g_quadVAO);
            g_quadVAO = 0;
        }

        if (g_quadVBO != 0) {
            glDeleteBuffers(1, &g_quadVBO);
            g_quadVBO = 0;
        }

        if (g_quadEBO != 0) {
            glDeleteBuffers(1, &g_quadEBO);
            g_quadEBO = 0;
        }

        if (g_shaderProgram != 0) {
            glDeleteProgram(g_shaderProgram);
            g_shaderProgram = 0;
        }
    }
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    // Clean up OpenGL context and window
    {
        if (g_openglContext) {
            wglMakeCurrent(NULL, NULL);
            wglDeleteContext(g_openglContext);
            g_openglContext = nullptr;
        }

        if (g_deviceContext && g_borderlessWindow && g_borderlessWindow->handle) {
            ReleaseDC(g_borderlessWindow->handle, g_deviceContext);
            g_deviceContext = nullptr;
        }

        if (g_borderlessWindow && g_borderlessWindow->handle) {
            DestroyWindow(g_borderlessWindow->handle);
            g_borderlessWindow->handle = nullptr;
        }

        g_borderlessWindow.reset();
    }
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

//-----------------------------------------------------------------------------
// [SECTION] JSON Serialization and Deserialization
//-----------------------------------------------------------------------------

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

	LabelConfig labelConfig;
	labelConfig.id = "##systempromptlabel";
	labelConfig.label = "System Prompt";
	labelConfig.icon = ICON_FA_GEAR;
	labelConfig.size = ImVec2(Config::Icon::DEFAULT_FONT_SIZE, 0);
	labelConfig.isBold = true;
	Label::render(labelConfig);

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
    InputFieldConfig inputFieldConfig(
		"##systemprompt",           // ID
		inputSize, 				    // Size
		currentPreset.systemPrompt, // Input text buffer
		focusSystemPrompt);		    // Focus
	inputFieldConfig.placeholderText = "Enter your system prompt here...";
	inputFieldConfig.processInput = [&](const std::string& input)
		{
			currentPreset.systemPrompt = input;
		};
	InputField::renderMultiline(inputFieldConfig);

    ImGui::Spacing();
    ImGui::Spacing();

	// Model settings label
	LabelConfig modelSettingsLabelConfig;
	modelSettingsLabelConfig.id = "##modelsettings";
	modelSettingsLabelConfig.label = "Model Settings";
	modelSettingsLabelConfig.icon = ICON_FA_SLIDERS;
	modelSettingsLabelConfig.size = ImVec2(Config::Icon::DEFAULT_FONT_SIZE, 0);
	modelSettingsLabelConfig.isBold = true;
	Label::render(modelSettingsLabelConfig);

    ImGui::Spacing();
    ImGui::Spacing();

    // Sampling settings
    Slider::render("##temperature", currentPreset.temperature, 0.0f, 1.0f, sidebarWidth - 30);
    Slider::render("##top_p", currentPreset.top_p, 0.0f, 1.0f, sidebarWidth - 30);
    Slider::render("##top_k", currentPreset.top_k, 0.0f, 100.0f, sidebarWidth - 30, "%.0f");
    IntInputField::render("##random_seed", currentPreset.random_seed, sidebarWidth - 30);

    ImGui::Spacing();
    ImGui::Spacing();

    // Generation settings
    Slider::render("##min_length", currentPreset.min_length, 0.0f, 4096.0f, sidebarWidth - 30, "%.0f");
    Slider::render("##max_new_tokens", currentPreset.max_new_tokens, 0.0f, 4096.0f, sidebarWidth - 30, "%.0f");
}

/**
 * @brief Helper function to confirm the "Save Preset As" dialog.
 *
 * This function is called when the user clicks the "Save" button or pressed enter in the dialog.
 * It saves the current preset under the new name and closes the dialog.
 */
void ModelPresetSidebar::confirmSaveAsDialog(std::string &newPresetName)
{
    if (strlen(newPresetName.data()) > 0)
    {
        auto currentPreset = g_presetManager->getCurrentPreset();
        currentPreset.name = newPresetName.data();
        if (g_presetManager->savePreset(currentPreset, true))
        {
            g_presetManager->loadPresets(); // Reload to include the new preset
            ImGui::CloseCurrentPopup();
            memset(newPresetName.data(), 0, sizeof(newPresetName));
        }
    }
}

/**
 * @brief Renders the "Save Preset As" dialog for saving a model preset under a new name.
 */
void ModelPresetSidebar::renderSaveAsDialog(bool &showSaveAsDialog)
{
    if (showSaveAsDialog)
    {
        ImGui::OpenPopup("Save Preset As");
        showSaveAsDialog = false;
    }

    // Change the window title background color
    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.125F, 0.125F, 0.125F, 1.0F));       // Inactive state color
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.125F, 0.125F, 0.125F, 1.0F)); // Active state color

    // Apply rounded corners to the window
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);

    if (ImGui::BeginPopupModal("Save Preset As", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        static bool focusNewPresetName = true;
        static std::string newPresetName(256, '\0');

        // input parameters is needed to process the input
        auto processInput = [](const std::string &input)
        {
            confirmSaveAsDialog(newPresetName);
        };

        // Set the new preset name to the current preset name by default
        if (strlen(newPresetName.data()) == 0)
        {
            strncpy(newPresetName.data(), g_presetManager->getCurrentPreset().name.c_str(), sizeof(newPresetName));
            newPresetName[sizeof(newPresetName) - 1] = '\0'; // Ensure null-terminated
        }

		// Render the input field for the new preset name
        InputFieldConfig newPresetNameInputConfig(
			"##newpresetname",          // ID
			ImVec2(250, 0), 		    // Size
			newPresetName,              // Input text buffer
			focusNewPresetName);        // Focus
		newPresetNameInputConfig.placeholderText = "Enter new preset name...";
		newPresetNameInputConfig.flags = ImGuiInputTextFlags_EnterReturnsTrue;
		newPresetNameInputConfig.processInput = processInput;
		newPresetNameInputConfig.frameRounding = 5.0F;
		InputField::render(newPresetNameInputConfig);

        ImGui::Spacing();

		// Save and Cancel buttons
        ButtonConfig confirmSaveConfig;
		confirmSaveConfig.id = "##confirmSave";
		confirmSaveConfig.label = "Save";
		confirmSaveConfig.icon = std::nullopt;
		confirmSaveConfig.size = ImVec2(122.5F, 0);
		confirmSaveConfig.onClick = []()
			{
				confirmSaveAsDialog(newPresetName);
			};
		confirmSaveConfig.iconSolid = false;
		confirmSaveConfig.backgroundColor = g_presetManager->hasUnsavedChanges() ? RGBAToImVec4(26, 95, 180, 255) : RGBAToImVec4(26, 95, 180, 128);
		confirmSaveConfig.hoverColor = RGBAToImVec4(53, 132, 228, 255);
		confirmSaveConfig.activeColor = RGBAToImVec4(26, 95, 180, 255);

        ButtonConfig cancelSaveConfig;
		cancelSaveConfig.id = "##cancelSave";
		cancelSaveConfig.label = "Cancel";
		cancelSaveConfig.icon = std::nullopt;
		cancelSaveConfig.size = ImVec2(122.5F, 0);
		cancelSaveConfig.onClick = []()
			{
				ImGui::CloseCurrentPopup();
				memset(newPresetName.data(), 0, sizeof(newPresetName));
			};
		cancelSaveConfig.iconSolid = false;
		cancelSaveConfig.backgroundColor = RGBAToImVec4(26, 95, 180, 255);
		cancelSaveConfig.hoverColor = RGBAToImVec4(53, 132, 228, 255);
		cancelSaveConfig.activeColor = RGBAToImVec4(26, 95, 180, 255);

        std::vector<ButtonConfig> saveAsDialogButtons = {confirmSaveConfig, cancelSaveConfig};
        Button::renderGroup(saveAsDialogButtons, ImGui::GetCursorPosX(), ImGui::GetCursorPosY(), 10);

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
    static bool showSaveAsDialog = false;

    ImGui::Spacing();
    ImGui::Spacing();

	// Model presets label
    {
        LabelConfig labelConfig;
        labelConfig.id = "##modelpresets";
        labelConfig.label = "Model Presets";
        labelConfig.icon = ICON_FA_BOX_OPEN;
        labelConfig.size = ImVec2(Config::Icon::DEFAULT_FONT_SIZE, 0);
        labelConfig.isBold = true;
        Label::render(labelConfig);
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // Get the current presets and create a vector of names
    const std::vector<ModelPreset> &presets = g_presetManager->getPresets();
    std::vector<const char *> presetNames;
    for (const ModelPreset &preset : presets)
    {
        presetNames.push_back(preset.name.c_str());
    }

    int currentIndex = g_presetManager->getCurrentPresetIndex();

    // Render the ComboBox for model presets
    float comboBoxWidth = sidebarWidth - 54;
    if (ComboBox::render("##modelpresets",
                                  presetNames.data(),
                                  presetNames.size(),
                                  currentIndex,
                                  comboBoxWidth))
    {
        g_presetManager->switchPreset(currentIndex);
    }

    ImGui::SameLine();

    // Delete button
    {
        ButtonConfig deleteButtonConfig;
        deleteButtonConfig.id = "##delete";
        deleteButtonConfig.label = std::nullopt;
        deleteButtonConfig.icon = ICON_FA_TRASH;
        deleteButtonConfig.size = ImVec2(24, 0);
        deleteButtonConfig.onClick = []()
            {
                if (g_presetManager->getPresets().size() > 1)
                { // Prevent deleting last preset
                    auto& currentPreset = g_presetManager->getCurrentPreset();
                    if (g_presetManager->deletePreset(currentPreset.name))
                    {
                        // Force reload presets after successful deletion
                        g_presetManager->loadPresets();
                    }
                }
            };
        deleteButtonConfig.iconSolid = true;
        deleteButtonConfig.backgroundColor = Config::Color::TRANSPARENT_COL;
        deleteButtonConfig.hoverColor = RGBAToImVec4(191, 88, 86, 255);
        deleteButtonConfig.activeColor = RGBAToImVec4(165, 29, 45, 255);
        deleteButtonConfig.alignment = Alignment::CENTER;

        // Only enable delete button if we have more than one preset
        if (presets.size() <= 1)
        {
            deleteButtonConfig.state = ButtonState::DISABLED;
        }

        Button::render(deleteButtonConfig);

	} // End of delete button

    ImGui::Spacing();
    ImGui::Spacing();

    // Save and Save as New buttons
    {
        ButtonConfig saveButtonConfig;
        saveButtonConfig.id = "##save";
        saveButtonConfig.label = "Save";
        saveButtonConfig.icon = std::nullopt;
        saveButtonConfig.size = ImVec2(sidebarWidth / 2 - 15, 0);
        saveButtonConfig.onClick = []()
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
            };
        saveButtonConfig.iconSolid = false;
        saveButtonConfig.backgroundColor = g_presetManager->hasUnsavedChanges() ? RGBAToImVec4(26, 95, 180, 255) : RGBAToImVec4(26, 95, 180, 128);
        saveButtonConfig.hoverColor = RGBAToImVec4(53, 132, 228, 255);
        saveButtonConfig.activeColor = RGBAToImVec4(26, 95, 180, 255);

        ButtonConfig saveAsNewButtonConfig;
        saveAsNewButtonConfig.id = "##saveasnew";
        saveAsNewButtonConfig.label = "Save as New";
        saveAsNewButtonConfig.icon = std::nullopt;
        saveAsNewButtonConfig.size = ImVec2(sidebarWidth / 2 - 15, 0);
        saveAsNewButtonConfig.onClick = []()
            {
                showSaveAsDialog = true;
            };

        std::vector<ButtonConfig> buttons = { saveButtonConfig, saveAsNewButtonConfig };

		// Render the buttons
        Button::renderGroup(buttons, 9, ImGui::GetCursorPosY(), 10);

	} // End of save and save as new buttons

    ImGui::Spacing();
    ImGui::Spacing();

    ModelPresetSidebar::renderSaveAsDialog(showSaveAsDialog);
}

/**
 * @brief Exports the current model presets to a JSON file.
 */
void ModelPresetSidebar::exportPresets()
{
    // Initialize variables
    nfdu8char_t* outPath = nullptr;
    nfdu8filteritem_t filters[2] = { {"JSON Files", "json"} };

    // Zero out the args struct
    nfdsavedialogu8args_t args;
    memset(&args, 0, sizeof(nfdsavedialogu8args_t));

    // Set up filter arguments
    args.filterList = filters;
    args.filterCount = 1;

    // Show save dialog
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
        const auto& currentPreset = g_presetManager->getCurrentPreset();
        bool success = g_presetManager->savePresetToPath(currentPreset, savePath.string());

        if (success)
        {
            std::cout << "Preset saved successfully to: " << savePath << std::endl;
        }
        else
        {
            std::cerr << "Failed to save preset to: " << savePath << std::endl;
        }
    }
    else if (result == NFD_CANCEL)
    {
        std::cout << "Save dialog canceled by the user." << std::endl;
    }
    else
    {
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
    const float sidebarHeight = io.DisplaySize.y - Config::TITLE_BAR_HEIGHT;

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - sidebarWidth, Config::TITLE_BAR_HEIGHT), ImGuiCond_Always);
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
    ButtonConfig exportButtonConfig;
	exportButtonConfig.id = "##export";
	exportButtonConfig.label = "Export as JSON";
	exportButtonConfig.icon = std::nullopt;
	exportButtonConfig.size = ImVec2(sidebarWidth - 20, 0);
	exportButtonConfig.onClick = ModelPresetSidebar::exportPresets;
	exportButtonConfig.iconSolid = false;
	exportButtonConfig.backgroundColor = Config::Color::SECONDARY;
	exportButtonConfig.hoverColor = Config::Color::PRIMARY;
	exportButtonConfig.activeColor = Config::Color::SECONDARY;
	Button::render(exportButtonConfig);

    ImGui::End();
}