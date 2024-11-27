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
#include "model/preset_manager.hpp"
#include "config.hpp"
#include "ui/fonts.hpp"
#include "ui/widgets.hpp"
#include "ui/chat/chat_history_sidebar.hpp"
#include "ui/chat/chat_section.hpp"
#include "ui/chat/preset_sidebar.hpp"

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

    FontsManager::GetInstance().LoadFonts(imguiIO);

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
                ImGui::PushFont(FontsManager::GetInstance().GetIconFont(FontsManager::REGULAR));
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
                ImGui::PushFont(FontsManager::GetInstance().GetIconFont(FontsManager::REGULAR));
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
	Model::initializePresetManager();

    // Initialize NFD
    NFD_Init();

    // Get initial window size
    int display_w, display_h;
    RECT rect;
    if (GetClientRect(hwnd, &rect)) 
    {
        display_w = rect.right - rect.left;
        display_h = rect.bottom - rect.top;
    }
    else 
    {
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
            renderModelPresetSidebar(modelPresetSidebarWidth);
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