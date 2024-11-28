// TODO: refactor to use builder pattern

#pragma once

#include <string>
#include <optional>
#include <functional>
#include <algorithm>
#include <imgui.h>

#include "config.hpp"
#include "ui/fonts.hpp"

enum ButtonState
{
    NORMAL,
    DISABLED,
    ACTIVE
};

enum Alignment
{
    LEFT,
    CENTER,
    RIGHT
};

/**
 * @brief A struct to store the configuration for a button
 *
 * The ButtonConfig struct stores the configuration for a button, including the label,
 * icon, size, padding, and the onClick function.
 */
struct ButtonConfig
{
    std::string id;
    std::optional<std::string> label;
    std::optional<std::string> icon;
    ImVec2 size;
    std::optional<float> gap = 5.0F;
    std::function<void()> onClick;
    std::optional<bool> iconSolid;
    std::optional<ImVec4> backgroundColor = Config::Color::TRANSPARENT_COL;
    std::optional<ImVec4> hoverColor = Config::Color::SECONDARY;
    std::optional<ImVec4> activeColor = Config::Color::PRIMARY;
    std::optional<ButtonState> state = ButtonState::NORMAL;
    std::optional<Alignment> alignment = Alignment::CENTER;
};

/**
 * @brief A struct to store the configuration for a label
 *
 * The LabelConfig struct stores the configuration for a label, including the label,
 * icon, size, icon padding, gap, and whether the label is bold.
 */
struct LabelConfig
{
    std::string id;
    std::string label;
    std::optional<std::string> icon = "";
    ImVec2 size;
    std::optional<float> iconPaddingX = 5.0F;
    std::optional<float> iconPaddingY = 5.0F;
    std::optional<float> gap = 5.0F;
    std::optional<bool> isBold = false;
    std::optional<bool> iconSolid = false;
    std::optional<Alignment> alignment = Alignment::CENTER;
};

/**
 * @brief A struct to store the configuration for an input field
 *
 * The InputFieldConfig struct stores the configuration for an input field, including the ID,
 * size, input text buffer, placeholder text, flags, process input function, focus input field flag,
 * frame rounding, padding, background color, hover color, active color, and text color.
 */
struct InputFieldConfig 
{
    std::string id;
    ImVec2 size;
    std::string& inputTextBuffer;
    bool& focusInputField;
    std::string placeholderText = "";
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_None;
    std::function<void(const std::string&)> processInput;
    float frameRounding = Config::InputField::FRAME_ROUNDING;
    ImVec2 padding = ImVec2(Config::FRAME_PADDING_X, Config::FRAME_PADDING_Y);
    ImVec4 backgroundColor = Config::InputField::INPUT_FIELD_BG_COLOR;
    ImVec4 hoverColor = Config::InputField::INPUT_FIELD_BG_COLOR;
    ImVec4 activeColor = Config::InputField::INPUT_FIELD_BG_COLOR;
    ImVec4 textColor = ImVec4(1.0F, 1.0F, 1.0F, 1.0F);

    // Constructor
    InputFieldConfig(
        const std::string& id,
        const ImVec2& size,
        std::string& inputTextBuffer,
        bool& focusInputField)
        : id(id),
        size(size),
        inputTextBuffer(inputTextBuffer),
        focusInputField(focusInputField) {}
};

namespace Label
{
    /**
     * @brief Renders a label with the specified configuration.
     *
     * @param config The configuration for the label.
     */
    void render(const LabelConfig& config)
    {
        bool hasIcon = !config.icon.value().empty();

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + config.iconPaddingX.value());
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + config.iconPaddingY.value());

        if (hasIcon)
        {
            // Select font based on icon style
            if (config.iconSolid.value())
            {
                ImGui::PushFont(FontsManager::GetInstance().GetIconFont(FontsManager::SOLID));
            }
            else
            {
                ImGui::PushFont(FontsManager::GetInstance().GetIconFont(FontsManager::REGULAR));
            }

            // Render icon
            ImGui::Text("%s", config.icon.value().c_str());
            ImGui::SameLine(0, (config.size.x / 4) + config.gap.value());

            ImGui::PopFont(); // Pop icon font
        }

        // Render label text with specified font weight
        if (config.isBold.value())
        {
            ImGui::PushFont(FontsManager::GetInstance().GetMarkdownFont(FontsManager::BOLD));
        }
        else
        {
            ImGui::PushFont(FontsManager::GetInstance().GetMarkdownFont(FontsManager::REGULAR));
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
    void render(const LabelConfig& config, ImVec2 rectMin, ImVec2 rectMax)
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
                ImGui::PushFont(FontsManager::GetInstance().GetIconFont(FontsManager::SOLID));
            }
            else
            {
                ImGui::PushFont(FontsManager::GetInstance().GetIconFont(FontsManager::REGULAR));
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
                ImGui::PushFont(FontsManager::GetInstance().GetMarkdownFont(FontsManager::BOLD));
            }
            else
            {
                ImGui::PushFont(FontsManager::GetInstance().GetMarkdownFont(FontsManager::REGULAR));
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
                ImGui::PushFont(FontsManager::GetInstance().GetIconFont(FontsManager::SOLID));
            }
            else
            {
                ImGui::PushFont(FontsManager::GetInstance().GetIconFont(FontsManager::REGULAR));
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
                ImGui::PushFont(FontsManager::GetInstance().GetMarkdownFont(FontsManager::BOLD));
            }
            else
            {
                ImGui::PushFont(FontsManager::GetInstance().GetMarkdownFont(FontsManager::REGULAR));
            }

            ImGui::TextUnformatted(truncatedLabel.c_str());
            ImGui::PopFont();
        }

        // Pop the clipping rectangle
        ImGui::PopClipRect();
    }
}

namespace Button
{
    /**
     * @brief Renders a single button with the specified configuration.
     *
     * @param config The configuration for the button.
     */
    void render(const ButtonConfig& config)
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
        Label::render(labelConfig, buttonMin, buttonMax);

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
    void renderGroup(const std::vector<ButtonConfig>& buttons, float startX, float startY, float spacing = Config::Button::SPACING)
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
} // namespace Button

namespace InputField
{
    /**
     * @brief Sets the style for the input field.
     *
     * @param frameRounding The rounding of the input field frame.
     * @param framePadding The padding of the input field frame.
     * @param bgColor The background color of the input field.
     */
    void setStyle(const InputFieldConfig& config)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, config.frameRounding);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, config.padding);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, config.backgroundColor);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, config.hoverColor);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, config.activeColor);

        // Set text color
        ImGui::PushStyleColor(ImGuiCol_Text, config.textColor);
    }

    /**
     * @brief Restores the default style for the input field.
     */
    void restoreStyle()
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
    void handleSubmission(char* inputText, bool& focusInputField, const std::function<void(const std::string&)>& processInput, bool clearInput)
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
    void renderMultiline(const InputFieldConfig& config)
    {
        // Set style
        setStyle(config);

        // Set keyboard focus initially, then reset
        if (config.focusInputField)
        {
            ImGui::SetKeyboardFocusHere();
            config.focusInputField = false;
        }

        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + config.size.x - 15);

        // Draw the input field
        if (ImGui::InputTextMultiline(config.id.c_str(), config.inputTextBuffer.data(), Config::InputField::TEXT_SIZE, config.size, config.flags) && config.processInput)
        {
            InputField::handleSubmission(config.inputTextBuffer.data(), config.focusInputField, config.processInput,
                (config.flags & ImGuiInputTextFlags_CtrlEnterForNewLine) ||
                (config.flags & ImGuiInputTextFlags_ShiftEnterForNewLine));
        }

        ImGui::PopTextWrapPos();

        // Draw placeholder if input is empty
        if (strlen(config.inputTextBuffer.data()) == 0)
        {
            // Allow overlapping rendering
            ImGui::SetItemAllowOverlap();

            // Get the current window's draw list
            ImDrawList* drawList = ImGui::GetWindowDrawList();

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
                config.placeholderText.c_str(),
                nullptr,
                wrapWidth);
        }

        // Restore original style
        restoreStyle();
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
    void render(const InputFieldConfig& config) {
        // Set style
        setStyle(config);

        // Set keyboard focus initially, then reset
        if (config.focusInputField) {
            ImGui::SetKeyboardFocusHere();
            config.focusInputField = false;
        }

        ImGui::PushItemWidth(config.size.x);

        // Prepare callback data
        struct InputTextCallback_UserData {
            std::string* Str;
        };

        InputTextCallback_UserData user_data;
        user_data.Str = &config.inputTextBuffer;

        auto callback = [](ImGuiInputTextCallbackData* data) -> int {
            if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
                InputTextCallback_UserData* user_data = (InputTextCallback_UserData*)data->UserData;
                std::string* str = user_data->Str;
                str->resize(data->BufTextLen);
                data->Buf = (char*)str->c_str();
            }
            return 0;
            };

        ImGuiInputTextFlags flags = config.flags | ImGuiInputTextFlags_CallbackResize;

        // Draw the input field
        if (ImGui::InputText(config.id.c_str(), (char*)config.inputTextBuffer.c_str(), config.inputTextBuffer.capacity() + 1, flags, callback, &user_data)
            && config.processInput) {
            handleSubmission((char*)config.inputTextBuffer.c_str(), config.focusInputField, config.processInput, false);
        }

        // Draw placeholder if input is empty
        if (config.inputTextBuffer.empty()) {
            // Allow overlapping rendering
            ImGui::SetItemAllowOverlap();

            // Get the current window's draw list
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            // Get the input field's bounding box
            ImVec2 inputMin = ImGui::GetItemRectMin();
            ImVec2 inputMax = ImGui::GetItemRectMax();

            // Calculate the position for the placeholder text
            ImVec2 placeholderPos = ImVec2(inputMin.x + Config::FRAME_PADDING_X, inputMin.y + (inputMax.y - inputMin.y) * 0.5f - ImGui::GetFontSize() * 0.5f);

            // Set placeholder text color (light gray)
            ImU32 placeholderColor = ImGui::GetColorU32(ImVec4(0.7f, 0.7f, 0.7f, 1.0f));

            // Render the placeholder text
            drawList->AddText(placeholderPos, placeholderColor, config.placeholderText.c_str());
        }

        // Restore original style
        restoreStyle();
    }
} // namespace InputField

namespace Slider
{
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
    void render(const char* label, float& value, float minValue, float maxValue, const float sliderWidth, const char* format = "%.2f", const float paddingX = 5.0F, const float inputWidth = 32.0F)
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
        Label::render(labelConfig);

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
} // namespace Slider

namespace IntInputField
{
    /**
     * @brief Renders an integer input field with the specified configuration.
     *
     * @param label The label for the input field.
     * @param value The value of the input field.
     * @param inputWidth The width of the input field.
     * @param paddingX The horizontal padding for the input field.
     */
    void render(const char* label, int& value, const float inputWidth, const float paddingX = 5.0F)
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
        Label::render(labelConfig);

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
} // namespace IntInputField

namespace ComboBox
{
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
    auto render(const char* label, const char** items, int itemsCount, int& selectedItem, float width, float height = 28.0F) -> bool
    {
        // Calculate frame padding based on desired height
        ImVec2 framePadding = ImGui::GetStyle().FramePadding;
        float defaultHeight = ImGui::GetFrameHeight(); // Default frame height
        framePadding.y = (height - defaultHeight) * 0.5f; // Adjust vertical padding to achieve desired height

        // Push style variables for frame and popup rounding
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, Config::ComboBox::FRAME_ROUNDING);
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, Config::ComboBox::POPUP_ROUNDING);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, framePadding); // Adjust button height through padding

        // Push style colors
        ImGui::PushStyleColor(ImGuiCol_FrameBg, Config::ComboBox::COMBO_BG_COLOR);
        ImGui::PushStyleColor(ImGuiCol_Border, Config::ComboBox::COMBO_BORDER_COLOR);
        ImGui::PushStyleColor(ImGuiCol_Text, Config::ComboBox::TEXT_COLOR);
        ImGui::PushStyleColor(ImGuiCol_Button, Config::ComboBox::COMBO_BG_COLOR);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Config::ComboBox::BUTTON_HOVERED_COLOR);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, Config::ComboBox::BUTTON_ACTIVE_COLOR);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, Config::ComboBox::POPUP_BG_COLOR);

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
} // namespace ComboBox