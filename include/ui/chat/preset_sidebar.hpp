#pragma once

#include "imgui.h"
#include "model/preset_manager.hpp"
#include "ui/widgets.hpp"
#include "config.hpp"
#include "nfd.h"

/**
 * @brief Renders the model settings sidebar with the specified width.
 *
 * @param sidebarWidth The width of the sidebar.
 */
void renderSamplingSettings(const float sidebarWidth)
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
    auto currentPresetOpt = Model::PresetManager::getInstance().getCurrentPreset();
    if (!currentPresetOpt)
    {
        // Handle the case where there's no current preset
        return;
    }
    // Get the reference to the actual preset
    Model::ModelPreset& currentPreset = currentPresetOpt->get();

    // System prompt input
    static bool focusSystemPrompt = true;
    ImVec2 inputSize = ImVec2(sidebarWidth - 20, 100);

    // Provide a processInput lambda to update the systemPrompt
    InputFieldConfig inputFieldConfig(
        "##systemprompt",           // ID
        inputSize,                  // Size
        currentPreset.systemPrompt, // Input text buffer
        focusSystemPrompt);         // Focus
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
void confirmSaveAsDialog(std::string& newPresetName)
{
    if (!newPresetName.empty())
    {
        // Start the asynchronous copy operation and wait for it to complete
        if (Model::PresetManager::getInstance().copyCurrentPresetAs(newPresetName).get())
        {
            Model::PresetManager::getInstance().switchPreset(newPresetName);
            ImGui::CloseCurrentPopup();
            newPresetName.clear();
        }
        else
        {
            // Handle failure (e.g., show an error message)
            std::cerr << "Failed to copy preset." << std::endl;
        }
    }
}

static std::string newPresetName;

/**
 * @brief Renders the "Save Preset As" dialog for saving a model preset under a new name.
 */
void renderSaveAsDialog(bool& showSaveAsDialog)
{
    if (showSaveAsDialog)
    {
        ImGui::OpenPopup("Save Preset As");
        showSaveAsDialog = false;
    }

    // Style adjustments
    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.125F, 0.125F, 0.125F, 1.0F));       // Inactive state color
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.125F, 0.125F, 0.125F, 1.0F)); // Active state color
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);

    if (ImGui::BeginPopupModal("Save Preset As", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        static bool focusNewPresetName = true;

        // Set the new preset name to the current preset name by default
        if (newPresetName.empty())
        {
            auto currentPresetOpt = Model::PresetManager::getInstance().getCurrentPreset();
            if (currentPresetOpt)
            {
                newPresetName = currentPresetOpt->get().name;
            }
        }

        auto processInput = [](const std::string& input) {
            if (Model::PresetManager::getInstance().copyCurrentPresetAs(input).get())
            {
                Model::PresetManager::getInstance().switchPreset(input);
                ImGui::CloseCurrentPopup();
                newPresetName.clear();
            }
            };

        // Input field for the new preset name
        InputFieldConfig newPresetNameInputConfig(
            "##newpresetname",       // ID
            ImVec2(250, 0),          // Size
            newPresetName,           // Input text buffer
            focusNewPresetName);     // Focus
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
        confirmSaveConfig.onClick = [&]() {
            if (Model::PresetManager::getInstance().copyCurrentPresetAs(newPresetName).get())
            {
                Model::PresetManager::getInstance().switchPreset(newPresetName);
                ImGui::CloseCurrentPopup();
                newPresetName.clear();
            }
            };
        confirmSaveConfig.iconSolid = false;
        confirmSaveConfig.backgroundColor = RGBAToImVec4(26, 95, 180, 255);
        confirmSaveConfig.hoverColor = RGBAToImVec4(53, 132, 228, 255);
        confirmSaveConfig.activeColor = RGBAToImVec4(26, 95, 180, 255);

        ButtonConfig cancelSaveConfig;
        cancelSaveConfig.id = "##cancelSave";
        cancelSaveConfig.label = "Cancel";
        cancelSaveConfig.icon = std::nullopt;
        cancelSaveConfig.size = ImVec2(122.5F, 0);
        cancelSaveConfig.onClick = [&]()
            {
                ImGui::CloseCurrentPopup();
                newPresetName.clear();
            };
        cancelSaveConfig.iconSolid = false;
        cancelSaveConfig.backgroundColor = RGBAToImVec4(26, 95, 180, 255);
        cancelSaveConfig.hoverColor = RGBAToImVec4(53, 132, 228, 255);
        cancelSaveConfig.activeColor = RGBAToImVec4(26, 95, 180, 255);

        std::vector<ButtonConfig> saveAsDialogButtons = { confirmSaveConfig, cancelSaveConfig };
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
void renderModelPresetsSelection(const float sidebarWidth)
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
    const std::vector<Model::ModelPreset>& presets = Model::PresetManager::getInstance().getPresets();
    std::vector<const char*> presetNames;
    for (const Model::ModelPreset& preset : presets)
    {
        presetNames.push_back(preset.name.c_str());
    }

    // Get the current preset index
    int currentIndex = 0;
    auto currentPresetOpt = Model::PresetManager::getInstance().getCurrentPreset();
    if (currentPresetOpt)
    {
        // Get the reference to the current preset
        const Model::ModelPreset& currentPreset = currentPresetOpt->get();
        currentIndex = static_cast<int>(Model::PresetManager::getInstance().getSortedPresetIndex(currentPreset.name));
    }

    // Render the ComboBox for model presets
    float comboBoxWidth = sidebarWidth - 54;
    if (ComboBox::render("##modelpresets",
        presetNames.data(),
        static_cast<int>(presetNames.size()),
        currentIndex,
        comboBoxWidth))
    {
        // User has selected a new preset
        const char* selectedPresetName = presetNames[currentIndex];
        Model::PresetManager::getInstance().switchPreset(selectedPresetName);
    }

    ImGui::SameLine();

    // Delete button
    {
        ButtonConfig deleteButtonConfig;
        deleteButtonConfig.id = "##delete";
        deleteButtonConfig.label = std::nullopt;
        deleteButtonConfig.icon = ICON_FA_TRASH;
        deleteButtonConfig.size = ImVec2(24, 0);
        deleteButtonConfig.onClick = [&]()
            {
                if (Model::PresetManager::getInstance().getPresets().size() > 1)
                { // Prevent deleting last preset
                    auto currentPresetOpt = Model::PresetManager::getInstance().getCurrentPreset();
                    if (currentPresetOpt)
                    {
                        const std::string& presetName = currentPresetOpt->get().name;
                        // Start the asynchronous deletion and wait for completion
                        if (Model::PresetManager::getInstance().deletePreset(presetName).get())
                        {
                            // Update UI by reloading presets
                            // Since we're not using observers, we need to refresh the presets and current index
                        }
                        else
                        {
                            // Handle failure
                            std::cerr << "Failed to delete preset." << std::endl;
                        }
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
        saveButtonConfig.onClick = [&]()
            {
                bool hasChanges = Model::PresetManager::getInstance().hasUnsavedChanges();
                if (hasChanges)
                {
                    // Start the asynchronous save operation and wait for completion
                    if (Model::PresetManager::getInstance().saveCurrentPreset().get())
                    {
                        // Preset saved successfully
                    }
                    else
                    {
                        // Handle failure
                        std::cerr << "Failed to save preset." << std::endl;
                    }
                }
            };
        saveButtonConfig.iconSolid = false;
        saveButtonConfig.backgroundColor = Model::PresetManager::getInstance().hasUnsavedChanges() ? RGBAToImVec4(26, 95, 180, 255) : RGBAToImVec4(26, 95, 180, 128);
        saveButtonConfig.hoverColor = RGBAToImVec4(53, 132, 228, 255);
        saveButtonConfig.activeColor = RGBAToImVec4(26, 95, 180, 255);

        ButtonConfig saveAsNewButtonConfig;
        saveAsNewButtonConfig.id = "##saveasnew";
        saveAsNewButtonConfig.label = "Save as New";
        saveAsNewButtonConfig.icon = std::nullopt;
        saveAsNewButtonConfig.size = ImVec2(sidebarWidth / 2 - 15, 0);
        saveAsNewButtonConfig.onClick = [&]()
            {
                showSaveAsDialog = true;
            };

        std::vector<ButtonConfig> buttons = { saveButtonConfig, saveAsNewButtonConfig };

        // Render the buttons
        Button::renderGroup(buttons, 9, ImGui::GetCursorPosY(), 10);

    } // End of save and save as new buttons

    ImGui::Spacing();
    ImGui::Spacing();

    // Render the "Save As" dialog if needed
    renderSaveAsDialog(showSaveAsDialog);
}

/**
 * @brief Exports the current model presets to a JSON file.
 */
void exportPresets()
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
        bool success = Model::PresetManager::getInstance().saveCurrentPresetToPath(savePath).get();

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
void renderModelPresetSidebar(float& sidebarWidth)
{
    ImGuiIO& io = ImGui::GetIO();
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

    renderModelPresetsSelection(sidebarWidth);
    ImGui::Separator();
    renderSamplingSettings(sidebarWidth);
    ImGui::Separator();

    ImGui::Spacing();
    ImGui::Spacing();

    // Export button
    ButtonConfig exportButtonConfig;
    exportButtonConfig.id = "##export";
    exportButtonConfig.label = "Export as JSON";
    exportButtonConfig.icon = std::nullopt;
    exportButtonConfig.size = ImVec2(sidebarWidth - 20, 0);
    exportButtonConfig.onClick = [&]()
        {
            exportPresets();
        };
    exportButtonConfig.iconSolid = false;
    exportButtonConfig.backgroundColor = Config::Color::SECONDARY;
    exportButtonConfig.hoverColor = Config::Color::PRIMARY;
    exportButtonConfig.activeColor = Config::Color::SECONDARY;
    Button::render(exportButtonConfig);

    ImGui::End();
}