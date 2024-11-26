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

#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <glad/glad.h>
#include <gl/gl.h>
#include <dwmapi.h>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <tuple>
#include <stack>
#include <regex>
#include <array>
#include <optional>
#include <filesystem>

#include "nfd.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"
#include "IconFontAwesome6.h"
#include "IconFontAwesome6Brands.h"
#include "json.hpp"
#include "resource.h"

using json = nlohmann::json;

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
        float max_new_tokens = 2048.0f) : 
        id(id), lastModified(lastModified), name(name), systemPrompt(systemPrompt), temperature(temperature),
        top_p(top_p), top_k(top_k), random_seed(random_seed),
        min_length(min_length), max_new_tokens(max_new_tokens) {}
};

//-----------------------------------------------------------------------------
// [SECTION] Forward Declarations and Global Variables
//-----------------------------------------------------------------------------

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Forward declaration of global variables

// Window and OpenGL context
extern std::unique_ptr<class BorderlessWindow> g_borderlessWindow;
extern HGLRC                                   g_openglContext;
extern HDC                                     g_deviceContext;

// ImGui context
extern std::unique_ptr<class PresetManager> g_presetManager;

// textures
extern GLuint      g_shaderProgram;
extern GLuint      g_gradientTexture;
extern const char* g_quadVertexShaderSource;
extern const char* g_quadFragmentShaderSource;
extern GLuint      g_quadVAO;
extern GLuint      g_quadVBO;
extern GLuint      g_quadEBO;

//-----------------------------------------------------------------------------
// [SECTION] Classes
//-----------------------------------------------------------------------------

/**
 * @brief A class to create and manage a borderless window.
 *
 * The BorderlessWindow class provides functionality to create a window without borders,
 * and allows enabling or disabling borderless mode, borderless shadow, resizing, and dragging.
 */
class BorderlessWindow 
{
public:
    /**
     * @brief Constructs a new BorderlessWindow object.
     */
    BorderlessWindow(HINSTANCE hinstance);

    /**
     * @brief Sets the borderless mode of the window.
     *
     * @param enabled True to enable borderless mode, false to disable it.
     */
    auto set_borderless(bool enabled) -> void;

    /**
     * @brief Sets the borderless shadow of the window.
     *
     * @param enabled True to enable borderless shadow, false to disable it.
     */
    auto set_borderless_shadow(bool enabled) -> void;

	/**
	 * @brief get the current state of the window
     */
    bool isActive() const { return is_active; }

    HWND handle;                    ///< Handle to the window.
private:
    /**
     * @brief Window procedure for handling window messages.
     *
     * @param hwnd Handle to the window.
     * @param msg The message.
     * @param wparam Additional message information.
     * @param lparam Additional message information.
     * @return LRESULT The result of the message processing.
     */
    static auto CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) noexcept -> LRESULT;

    /**
     * @brief Performs hit testing to determine the location of the cursor.
     *
     * @param cursor The cursor position.
     * @return LRESULT The result of the hit test.
     */
    auto hit_test(POINT cursor) const->LRESULT;

    bool borderless = true;         ///< Indicates if the window is currently borderless.
    bool borderless_resize = true;  ///< Indicates if the window allows resizing by dragging the borders while borderless.
    bool borderless_drag = true;    ///< Indicates if the window allows moving by dragging the client area.
    bool borderless_shadow = true;  ///< Indicates if the window displays a native aero shadow while borderless.

	bool is_active = true;		    ///< Indicates if the window is currently active.

	HINSTANCE hInstance;		    ///< Handle to the instance.
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

// gradient background
namespace GradientBackground
{
    void checkShaderCompileErrors(GLuint shader, const std::string& type);
    void checkProgramLinkErrors(GLuint program);
    void generateGradientTexture(int width, int height);
    auto compileShader(GLenum type, const char* source) -> GLuint;
    auto createShaderProgram(const char* vertexSource, const char* fragmentSource) -> GLuint;
    void setupFullScreenQuad();
    void renderGradientBackground(HWND hwnd, int display_w, int display_h, float transitionProgress, float easedProgress);
} // namespace GradientBackground

bool initializeOpenGL(HWND hwnd);
void setupImGui(HWND hwnd);
void titleBar(HWND hwnd);
void mainLoop(HWND hwnd);
void cleanup();

//-----------------------------------------------------------------------------
// [SECTION] Utility Functions
//-----------------------------------------------------------------------------

auto RGBAToImVec4(float r, float g, float b, float a) -> ImVec4;

//-----------------------------------------------------------------------------
// [SECTION] JSON Serialization and Deserialization Functions
//-----------------------------------------------------------------------------

// Model Preset Serialization and Deserialization
void to_json(json &j, const ModelPreset &p);
void from_json(const json &j, ModelPreset &p);

//-----------------------------------------------------------------------------
// [SECTION] Custom UI Functions
//-----------------------------------------------------------------------------

namespace ModelPresetSidebar
{
    void render(float &sidebarWidth);
    void renderModelPresetsSelection(const float sidebarWidth);
    void renderSamplingSettings(const float sidebarWidth);
    void renderSaveAsDialog(bool &showSaveAsDialog);
    void confirmSaveAsDialog(std::string &newPresetName);
    void exportPresets();
} // namespace ModelSettings

#endif // KOLOSAL_H
