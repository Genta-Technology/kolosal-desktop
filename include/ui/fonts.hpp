#pragma once

#include "IconsMaterialSymbols.h"

#include <iostream>
#include <imgui.h>
#include <array>
#include <algorithm>

class FontsManager
{
public:
    static FontsManager& GetInstance()
    {
        static FontsManager instance;
        return instance;
    }

    enum FontType
    {
        REGULAR,
        BOLD,
        ITALIC,
        BOLDITALIC,
        CODE
    };

    enum IconType
    {
        OUTLINED
    };

    enum SizeLevel
    {
        SM = 0,    // Small
        MD,        // Medium
        LG,        // Large
        XL,        // Extra Large
        SIZE_COUNT
    };

    ImFont* GetMarkdownFont(const FontType style, SizeLevel sizeLevel = MD) const
    {
        // Clamp the size level to the available range
        sizeLevel = std::clamp(sizeLevel, SizeLevel::SM, SizeLevel::XL);

        switch (style)
        {
        case REGULAR: return mdFonts.regular[sizeLevel];
        case BOLD: return mdFonts.bold[sizeLevel];
        case ITALIC: return mdFonts.italic[sizeLevel];
        case BOLDITALIC: return mdFonts.boldItalic[sizeLevel];
        case CODE: return mdFonts.code[sizeLevel];
        default: return nullptr;
        }
    }

    ImFont* GetIconFont(const IconType style, SizeLevel sizeLevel = MD) const
    {
        // Clamp the size level to the available range
        sizeLevel = std::clamp(sizeLevel, SizeLevel::SM, SizeLevel::XL);

        switch (style)
        {
        case OUTLINED: return iconFonts.outlined[sizeLevel];
        default: return nullptr;
        }
    }

private:
    // Private constructor that now handles initialization
    FontsManager()
    {
        // Get ImGui IO
        ImGuiIO& imguiIO = ImGui::GetIO();

        // Font sizes mapping based on SizeLevel enum
        static const std::array<float, SizeLevel::SIZE_COUNT> fontSizes = {
            12.0f, // SM
            18.0f, // MD
            24.0f, // LG
            36.0f, // XL
        };

        // Preload default font once
        ImFont* defaultFont = imguiIO.Fonts->AddFontDefault();

        // Load markdown fonts
        LoadMarkdownFonts(imguiIO, defaultFont, fontSizes);

        // Load icon fonts
        LoadIconFonts(imguiIO, fontSizes);

        // Set the default font
        imguiIO.FontDefault = mdFonts.regular[SizeLevel::MD];
    }

    // Delete copy constructor and assignment operator
    FontsManager(const FontsManager&) = delete;
    FontsManager& operator=(const FontsManager&) = delete;

    // Rest of the private members and methods remain the same
    struct MarkdownFonts {
        ImFont* regular[SizeLevel::SIZE_COUNT]{};
        ImFont* bold[SizeLevel::SIZE_COUNT]{};
        ImFont* italic[SizeLevel::SIZE_COUNT]{};
        ImFont* boldItalic[SizeLevel::SIZE_COUNT]{};
        ImFont* code[SizeLevel::SIZE_COUNT]{};
    } mdFonts;

    struct IconFonts {
        ImFont* outlined[SizeLevel::SIZE_COUNT]{};
    } iconFonts;

    // Private methods for loading fonts
    void LoadMarkdownFonts(ImGuiIO& imguiIO, ImFont* fallbackFont, const std::array<float, SizeLevel::SIZE_COUNT>& fontSizes)
    {
        const char* mdFontPaths[] = {
            IMGUI_FONT_PATH_INTER_REGULAR,
            IMGUI_FONT_PATH_INTER_BOLD,
            IMGUI_FONT_PATH_INTER_ITALIC,
            IMGUI_FONT_PATH_INTER_BOLDITALIC,
            IMGUI_FONT_PATH_FIRACODE_REGULAR
        };

        for (int8_t i = SizeLevel::SM; i <= SizeLevel::XL; ++i)
        {
            float size = fontSizes[i];
            mdFonts.regular[i] = LoadFont(imguiIO, mdFontPaths[REGULAR], fallbackFont, size);
            mdFonts.bold[i] = LoadFont(imguiIO, mdFontPaths[BOLD], mdFonts.regular[i], size);
            mdFonts.italic[i] = LoadFont(imguiIO, mdFontPaths[ITALIC], mdFonts.regular[i], size);
            mdFonts.boldItalic[i] = LoadFont(imguiIO, mdFontPaths[BOLDITALIC], mdFonts.bold[i], size);
            mdFonts.code[i] = LoadFont(imguiIO, mdFontPaths[CODE], mdFonts.regular[i], size);
        }
    }

    void LoadIconFonts(ImGuiIO& imguiIO, const std::array<float, SizeLevel::SIZE_COUNT>& fontSizes)
    {
        const char* iconFontPath = IMGUI_FONT_PATH_OUTLINED;

        for (int8_t i = SizeLevel::SM; i <= SizeLevel::XL; ++i)
        {
            float size = fontSizes[i];
            iconFonts.outlined[i] = LoadIconFont(imguiIO, iconFontPath, size);
        }
    }

    ImFont* LoadFont(ImGuiIO& imguiIO, const char* fontPath, ImFont* fallbackFont, float fontSize)
    {
        ImFont* font = imguiIO.Fonts->AddFontFromFileTTF(fontPath, fontSize);
        if (!font)
        {
            std::cerr << "Failed to load font: " << fontPath << std::endl;
            return fallbackFont;
        }
        return font;
    }

    ImFont* LoadIconFont(ImGuiIO& imguiIO, const char* iconFontPath, float fontSize)
    {
        static const ImWchar icons_ranges[] = { ICON_MIN_MS, ICON_MAX_MS, 0 };
        ImFontConfig icons_config;
        icons_config.MergeMode = false;
        icons_config.PixelSnapH = true;
        icons_config.GlyphMinAdvanceX = fontSize;

        ImFont* iconFont = imguiIO.Fonts->AddFontFromFileTTF(iconFontPath, fontSize, &icons_config, icons_ranges);
        if (iconFont == nullptr)
        {
            std::cerr << "Failed to load icon font: " << iconFontPath << std::endl;
            return mdFonts.regular[MD];
        }

        return iconFont;
    }
};