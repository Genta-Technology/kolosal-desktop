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

void renderChatHistorySidebar(float& sidebarWidth);
void renderChatWindow(float inputHeight, float chatHistorySidebarWidth, float modelPresetSidebarWidth);
void renderInputField(float inputHeight, float inputWidth);
void renderChatHistoryList(ImVec2 contentArea);

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
 * @brief Renders a label with the specified configuration inside a rectangle.
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

    // Calculate the size of the icon if present
    ImVec2 iconSize(0, 0);
    float iconPlusGapWidth = 0.0f;
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

        // Add gap to icon width if we have both icon and label
        iconPlusGapWidth = hasLabel ? (iconSize.x + config.gap.value_or(0.0f)) : iconSize.x;
    }

    // Calculate available width for label
    float availableLabelWidth = rectSize.x - iconPlusGapWidth - (2 * config.gap.value_or(5.0f));

    // Calculate label size and prepare truncated text if needed
    ImVec2 labelSize(0, 0);
    std::string truncatedLabel;
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

        // If label is too wide, we need to truncate it
        if (labelSize.x > availableLabelWidth)
        {
            float ellipsisWidth = ImGui::CalcTextSize("...").x;
            float targetWidth = availableLabelWidth - ellipsisWidth;

            // Binary search to find the right truncation point
            int left = 0;
            int right = config.label.length();
            truncatedLabel = config.label;

            while (left < right)
            {
                int mid = (left + right + 1) / 2;
                std::string testStr = config.label.substr(0, mid);
                float testWidth = ImGui::CalcTextSize(testStr.c_str()).x;

                if (testWidth <= targetWidth)
                {
                    left = mid;
                }
                else
                {
                    right = mid - 1;
                }
            }

            truncatedLabel = config.label.substr(0, left) + "...";
            labelSize = ImGui::CalcTextSize(truncatedLabel.c_str());
        }
        else
        {
            truncatedLabel = config.label;
        }

        ImGui::PopFont();
    }

    // Calculate total content width and height
    float contentWidth = iconPlusGapWidth + labelSize.x;
    float contentHeight = std::max(labelSize.y, iconSize.y);

    // Calculate vertical offset to center content
    float verticalOffset = rectMin.y + (rectSize.y - contentHeight) / 2.0f;

    // Calculate horizontal offset based on alignment
    float horizontalOffset;
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

        ImGui::PopFont();
    }

    // Render truncated label text with specified font weight, if it exists
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

        ImGui::TextUnformatted(truncatedLabel.c_str());
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
void Widgets::InputField::setStyle(const InputFieldConfig &config)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, config.frameRounding.value());
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, config.padding.value());
    ImGui::PushStyleColor(ImGuiCol_FrameBg, config.backgroundColor.value());
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, config.hoverColor.value());
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, config.activeColor.value());

    // Set text color
    ImGui::PushStyleColor(ImGuiCol_Text, config.textColor.value());
}

/**
 * @brief Restores the default style for the input field.
 */
void Widgets::InputField::restoreStyle()
{
    ImGui::PopStyleColor(4); // Restore FrameBg
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
void Widgets::InputField::renderMultiline(const InputFieldConfig &config)
{
    // Set style
    Widgets::InputField::setStyle(config);

    // Set keyboard focus initially, then reset
    if (config.focusInputField)
    {
        ImGui::SetKeyboardFocusHere();
        config.focusInputField = false;
    }

    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + config.size.x - 15);

    // Draw the input field
    if (ImGui::InputTextMultiline(config.id.c_str(), config.inputTextBuffer.data(), Config::InputField::TEXT_SIZE, config.size, config.flags.value()) && config.processInput.has_value())
    {
        Widgets::InputField::handleSubmission(config.inputTextBuffer.data(), config.focusInputField, config.processInput.value(),
                                              (config.flags.value() & ImGuiInputTextFlags_CtrlEnterForNewLine) ||
                                                  (config.flags.value() & ImGuiInputTextFlags_ShiftEnterForNewLine));
    }

    ImGui::PopTextWrapPos();

    // Draw placeholder if input is empty
    if (strlen(config.inputTextBuffer.data()) == 0)
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
        float wrapWidth = config.size.x - (2 * Config::FRAME_PADDING_X);

        // Render the placeholder text using AddText with wrapping
        drawList->AddText(
            ImGui::GetFont(),
            ImGui::GetFontSize(),
            placeholderPos,
            placeholderColor,
            config.placeholderText.value().c_str(),
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
void Widgets::InputField::render(const InputFieldConfig &config)
{
    // Set style
    Widgets::InputField::setStyle(config);

    // Set keyboard focus initially, then reset
    if (config.focusInputField)
    {
        ImGui::SetKeyboardFocusHere();
        config.focusInputField = false;
    }

    // set size of input field
    ImGui::PushItemWidth(config.size.x);

    // Draw the single-line input field
    if (ImGui::InputText(config.id.c_str(), config.inputTextBuffer.data(), Config::InputField::TEXT_SIZE, config.flags.value()) && config.processInput.has_value())
    {
        Widgets::InputField::handleSubmission(config.inputTextBuffer.data(), config.focusInputField, config.processInput.value(), false);
    }

    // Draw placeholder if input is empty
    if (strlen(config.inputTextBuffer.data()) == 0)
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
        drawList->AddText(placeholderPos, placeholderColor, config.placeholderText.value().c_str());
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
    LabelConfig labelConfig;
	labelConfig.id = label;
	labelConfig.label = renderLabel;
	labelConfig.size = ImVec2(0, 0);
	labelConfig.isBold = false;
	labelConfig.iconSolid = false;
    Widgets::Label::render(labelConfig);

    // Move the cursor to the right edge minus the input field width and padding
    ImGui::SameLine();

    // Apply custom styling for InputFloat
    ImGui::PushStyleColor(ImGuiCol_FrameBg, Config::Color::TRANSPARENT_COL);
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
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, Config::Color::TRANSPARENT_COL);
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
	LabelConfig labelConfig;
	labelConfig.id = label;
	labelConfig.label = renderLabel;
	labelConfig.size = ImVec2(0, 0);
	labelConfig.isBold = false;
	labelConfig.iconSolid = false;
    Widgets::Label::render(labelConfig);

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
auto Widgets::ComboBox::render(const char* label, const char** items, int itemsCount, int& selectedItem, float width, float height) -> bool
{
    // Calculate frame padding based on desired height
    ImVec2 framePadding = ImGui::GetStyle().FramePadding;
    float defaultHeight = ImGui::GetFrameHeight(); // Default frame height
    framePadding.y = (height - defaultHeight) * 0.5f; // Adjust vertical padding to achieve desired height

    // Push style variables for frame and popup rounding
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, Config::ComboBox::FRAME_ROUNDING);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, Config::ComboBox::POPUP_ROUNDING);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  framePadding); // Adjust button height through padding

    // Push style colors
    ImGui::PushStyleColor(ImGuiCol_FrameBg,       Config::ComboBox::COMBO_BG_COLOR);
    ImGui::PushStyleColor(ImGuiCol_Border,        Config::ComboBox::COMBO_BORDER_COLOR);
    ImGui::PushStyleColor(ImGuiCol_Text,          Config::ComboBox::TEXT_COLOR);
    ImGui::PushStyleColor(ImGuiCol_Button,        Config::ComboBox::COMBO_BG_COLOR);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Config::ComboBox::BUTTON_HOVERED_COLOR);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  Config::ComboBox::BUTTON_ACTIVE_COLOR);
    ImGui::PushStyleColor(ImGuiCol_PopupBg,       Config::ComboBox::POPUP_BG_COLOR);

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
    ImGui::PopStyleVar(3);   // Number of style vars pushed (FrameRounding, PopupRounding, FramePadding)

    return changed; // Return true if the selected item has changed
}

//-----------------------------------------------------------------------------
// [SECTION] Chat Window Rendering Functions
//-----------------------------------------------------------------------------

void pushIDAndColors(const Chat::Message msg, int index)
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

auto calculateDimensions(const Chat::Message msg, float windowWidth) -> std::tuple<float, float, float>
{
	float bubbleWidth = windowWidth * Config::Bubble::WIDTH_RATIO;
	float bubblePadding = Config::Bubble::PADDING;
	float paddingX = windowWidth - bubbleWidth - Config::Bubble::RIGHT_PADDING;

    if (msg.role == "assistant")
    {
		bubbleWidth = windowWidth;
		paddingX = 0;
    }

    return {bubbleWidth, bubblePadding, paddingX};
}

void renderMessageContent(const Chat::Message msg, float bubbleWidth, float bubblePadding)
{
    ImGui::SetCursorPosX(bubblePadding);
    ImGui::SetCursorPosY(bubblePadding);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + bubbleWidth - (bubblePadding * 2));
    ImGui::TextWrapped("%s", msg.content.c_str());
    ImGui::PopTextWrapPos();
}

void renderTimestamp(const Chat::Message msg, float bubblePadding)
{
    // Set timestamp color to a lighter gray
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7F, 0.7F, 0.7F, 1.0F)); // Light gray for timestamp

    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - ImGui::GetTextLineHeightWithSpacing() // Align timestamp at the bottom
                         - (bubblePadding - Config::Timing::TIMESTAMP_OFFSET_Y));
    ImGui::SetCursorPosX(bubblePadding); // Align timestamp to the left
    ImGui::TextWrapped("%s", Chat::timePointToString(msg.timestamp).c_str());

    ImGui::PopStyleColor(); // Restore original text color
}

void renderButtons(const Chat::Message msg, int index, float bubbleWidth, float bubblePadding)
{
    ImVec2 textSize = ImGui::CalcTextSize(msg.content.c_str(), nullptr, true, bubbleWidth - bubblePadding * 2);
    float buttonPosY = textSize.y + bubblePadding;

    if (msg.role == "user")
    {
        ButtonConfig copyButtonConfig;
		copyButtonConfig.id = "##copy" + std::to_string(index);
		copyButtonConfig.label = std::nullopt;
		copyButtonConfig.icon = ICON_FA_COPY;
		copyButtonConfig.size = ImVec2(Config::Button::WIDTH, 0);
		copyButtonConfig.onClick = [&msg]()
			{
				ImGui::SetClipboardText(msg.content.c_str());
				std::cout << "Copied message content to clipboard" << std::endl;
			};
        std::vector<ButtonConfig> userButtons = {copyButtonConfig};

        Widgets::Button::renderGroup(
            userButtons,
            bubbleWidth - bubblePadding - Config::Button::WIDTH,
            buttonPosY);
    }
    else
    {
		ButtonConfig likeButtonConfig;
		likeButtonConfig.id = "##like" + std::to_string(index);
		likeButtonConfig.label = std::nullopt;
		likeButtonConfig.icon = ICON_FA_THUMBS_UP;
		likeButtonConfig.size = ImVec2(Config::Button::WIDTH, 0);
		likeButtonConfig.onClick = [index]()
			{
				std::cout << "Like button clicked for message " << index << std::endl;
			};

		ButtonConfig dislikeButtonConfig;
		dislikeButtonConfig.id = "##dislike" + std::to_string(index);
		dislikeButtonConfig.label = std::nullopt;
		dislikeButtonConfig.icon = ICON_FA_THUMBS_DOWN;
		dislikeButtonConfig.size = ImVec2(Config::Button::WIDTH, 0);
		dislikeButtonConfig.onClick = [index]()
			{
				std::cout << "Dislike button clicked for message " << index << std::endl;
			};

        std::vector<ButtonConfig> assistantButtons = {likeButtonConfig, dislikeButtonConfig};

        Widgets::Button::renderGroup(
            assistantButtons,
            bubbleWidth - bubblePadding - (2 * Config::Button::WIDTH + Config::Button::SPACING),
            buttonPosY);
    }
}

void renderMessage(const Chat::Message& msg, int index, float contentWidth) 
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

void renderChatHistory(const Chat::ChatHistory chatHistory, float contentWidth)
{
    static size_t lastMessageCount = 0;
    size_t currentMessageCount = chatHistory.messages.size();

    // Check if new messages have been added
    bool newMessageAdded = currentMessageCount > lastMessageCount;

    // Save the scroll position before rendering
    float scrollY    = ImGui::GetScrollY();
    float scrollMaxY = ImGui::GetScrollMaxY();
    bool isAtBottom  = (scrollMaxY <= 0.0F) || (scrollY >= scrollMaxY - 1.0F);

    // Render messages
    const std::vector<Chat::Message> &messages = chatHistory.messages;
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

void renderRenameChatDialog(bool& showRenameChatDialog)
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
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);

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
        Widgets::InputField::render(inputConfig);

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
        confirmRename.iconSolid = false;
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
        cancelRename.iconSolid = false;
        cancelRename.backgroundColor = RGBAToImVec4(26, 95, 180, 255);
        cancelRename.hoverColor = RGBAToImVec4(53, 132, 228, 255);
        cancelRename.activeColor = RGBAToImVec4(26, 95, 180, 255);

        std::vector<ButtonConfig> renameChatDialogButtons = { confirmRename, cancelRename };
        Widgets::Button::renderGroup(renameChatDialogButtons, ImGui::GetCursorPosX(), ImGui::GetCursorPosY(), 10);

        ImGui::EndPopup();
    }

    // Revert to the previous style
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
}

void renderChatWindow(float inputHeight, float leftSidebarWidth, float rightSidebarWidth)
{
    ImGuiIO &imguiIO = ImGui::GetIO();

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
    Widgets::Button::render(renameButtonConfig);

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

void renderInputField(float inputHeight, float inputWidth)
{
    static std::string inputTextBuffer(Config::InputField::TEXT_SIZE, '\0');
    static bool focusInputField = true;

    // Define the input size
    ImVec2 inputSize = ImVec2(inputWidth, inputHeight);

    // Define a lambda to process the submitted input
    auto processInput = [&](const std::string& input)
    {
        auto& chatManager = Chat::ChatManager::getInstance();
		auto  currentChat = chatManager.getCurrentChat();

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

    // Render the input field widget with a placeholder
    InputFieldConfig inputConfig(
		"##chatinput",      // ID
		inputSize,          // Size
		inputTextBuffer,    // Input text buffer
		focusInputField);   // Focus
	inputConfig.placeholderText = "Type a message and press Enter to send (Ctrl+Enter or Shift+Enter for new line)";
	inputConfig.flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CtrlEnterForNewLine | ImGuiInputTextFlags_ShiftEnterForNewLine;
	inputConfig.processInput = processInput;
    Widgets::InputField::renderMultiline(inputConfig);
}

//-----------------------------------------------------------------------------
// [SECTION] Chat History Sidebar Rendering Functions
//-----------------------------------------------------------------------------

void renderChatHistorySidebar(float& sidebarWidth) 
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
    Widgets::Label::render(labelConfig);

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
    Widgets::Button::render(createNewChatButtonConfig);

    ImGui::Spacing();

    renderChatHistoryList(ImVec2(sidebarWidth, sidebarHeight - labelHeight));

    ImGui::End();
}

void renderChatHistoryList(ImVec2 contentArea) 
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

        // Optional: Add tooltip showing last modified time
        if (ImGui::IsItemHovered()) {
            std::time_t time = static_cast<std::time_t>(chat.lastModified);
            std::string timeStr = std::ctime(&time);
            ImGui::SetTooltip("Last modified: %s", timeStr.c_str());
        }

        Widgets::Button::render(chatButtonConfig);
        ImGui::Spacing();
    }

    ImGui::EndChild();
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
	Widgets::Label::render(labelConfig);

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
	Widgets::InputField::renderMultiline(inputFieldConfig);

    ImGui::Spacing();
    ImGui::Spacing();

	// Model settings label
	LabelConfig modelSettingsLabelConfig;
	modelSettingsLabelConfig.id = "##modelsettings";
	modelSettingsLabelConfig.label = "Model Settings";
	modelSettingsLabelConfig.icon = ICON_FA_SLIDERS;
	modelSettingsLabelConfig.size = ImVec2(Config::Icon::DEFAULT_FONT_SIZE, 0);
	modelSettingsLabelConfig.isBold = true;
	Widgets::Label::render(modelSettingsLabelConfig);

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
		Widgets::InputField::render(newPresetNameInputConfig);

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
        Widgets::Label::render(labelConfig);
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
    if (Widgets::ComboBox::render("##modelpresets",
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

        Widgets::Button::render(deleteButtonConfig);

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
        Widgets::Button::renderGroup(buttons, 9, ImGui::GetCursorPosY(), 10);

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
	Widgets::Button::render(exportButtonConfig);

    ImGui::End();
}