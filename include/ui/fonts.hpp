#pragma once

#include "IconFontAwesome6.h"
#include "IconFontAwesome6Brands.h"

#include <iostream>
#include <imgui.h>

class FontsManager {
public:
	static FontsManager& GetInstance()
	{
		static FontsManager instance;
		return instance;
	}

	void LoadFonts(ImGuiIO& imguiIO)
	{
		// Load markdown fonts
		mdFonts.regular = LoadFont(imguiIO, IMGUI_FONT_PATH_INTER_REGULAR, imguiIO.Fonts->AddFontDefault(), Config::Font::DEFAULT_FONT_SIZE);
		mdFonts.bold = LoadFont(imguiIO, IMGUI_FONT_PATH_INTER_BOLD, mdFonts.regular, Config::Font::DEFAULT_FONT_SIZE);
		mdFonts.italic = LoadFont(imguiIO, IMGUI_FONT_PATH_INTER_ITALIC, mdFonts.regular, Config::Font::DEFAULT_FONT_SIZE);
		mdFonts.boldItalic = LoadFont(imguiIO, IMGUI_FONT_PATH_INTER_BOLDITALIC, mdFonts.bold, Config::Font::DEFAULT_FONT_SIZE);
		mdFonts.code = LoadFont(imguiIO, IMGUI_FONT_PATH_FIRACODE_REGULAR, mdFonts.regular, Config::Font::DEFAULT_FONT_SIZE);

		// Load icon fonts
		iconFonts.regular = LoadIconFont(imguiIO, IMGUI_FONT_PATH_FA_REGULAR, Config::Icon::DEFAULT_FONT_SIZE);
		iconFonts.solid = LoadIconFont(imguiIO, IMGUI_FONT_PATH_FA_SOLID, Config::Icon::DEFAULT_FONT_SIZE);
		iconFonts.brands = LoadIconFont(imguiIO, IMGUI_FONT_PATH_FA_BRANDS, Config::Icon::DEFAULT_FONT_SIZE);

		// Set the default font
		imguiIO.FontDefault = mdFonts.regular;
	}

	enum FontType
	{
		REGULAR,
		BOLD,
		ITALIC,
		BOLDITALIC,
		CODE,
		SOLID,      // Icon font
		BRANDS	    // Icon font
	};

	ImFont* GetMarkdownFont(const FontType style) const
	{
		switch (style)
		{
		case FontType::REGULAR: return mdFonts.regular;
		case FontType::BOLD: return mdFonts.bold;
		case FontType::ITALIC: return mdFonts.italic;
		case FontType::BOLDITALIC: return mdFonts.boldItalic;
		case FontType::CODE: return mdFonts.code;
		default: return nullptr;
		}
	}

	ImFont* GetIconFont(const FontType style) const
	{
		switch (style)
		{
		case FontType::REGULAR: return iconFonts.regular;
		case FontType::SOLID: return iconFonts.solid;
		case FontType::BRANDS: return iconFonts.brands;
		default: return nullptr;
		}
	}

private:
	// Private constructor to prevent instantiation
	FontsManager() = default;

	// Delete copy constructor and assignment operator
	FontsManager(const FontsManager&) = delete;
	FontsManager& operator=(const FontsManager&) = delete;

	// MarkdownFonts and IconFonts members
	struct MarkdownFonts {
		ImFont* regular = nullptr;
		ImFont* bold = nullptr;
		ImFont* italic = nullptr;
		ImFont* boldItalic = nullptr;
		ImFont* code = nullptr;
	} mdFonts;

	struct IconFonts {
		ImFont* regular = nullptr;
		ImFont* solid = nullptr;
		ImFont* brands = nullptr;
	} iconFonts;

	// Private methods for loading fonts
	ImFont* LoadFont(ImGuiIO& imguiIO, const char* fontPath, ImFont* fallbackFont, float fontSize)
	{
		ImFont* font = imguiIO.Fonts->AddFontFromFileTTF(fontPath, fontSize);
		if (font == nullptr)
		{
			std::cerr << "Failed to load font: " << fontPath << std::endl;
			return fallbackFont;
		}
		return font;
	}

	ImFont* LoadIconFont(ImGuiIO& imguiIO, const char* iconFontPath, float fontSize)
	{
		static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
		ImFontConfig icons_config;
		icons_config.MergeMode = true;
		icons_config.PixelSnapH = true;
		icons_config.GlyphMinAdvanceX = fontSize;

		if (!mdFonts.regular)
		{
			mdFonts.regular = LoadFont(imguiIO, IMGUI_FONT_PATH_INTER_REGULAR, imguiIO.Fonts->AddFontDefault(), fontSize);
		}

		ImFont* iconFont = imguiIO.Fonts->AddFontFromFileTTF(iconFontPath, fontSize, &icons_config, icons_ranges);
		if (iconFont == nullptr)
		{
			std::cerr << "Failed to load icon font: " << iconFontPath << std::endl;
			return mdFonts.regular;
		}

		return iconFont;
	}
};