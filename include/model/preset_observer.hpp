#pragma once

#include "model_preset.hpp"

namespace Model
{
    class IPresetObserver
    {
    public:
        virtual ~IPresetObserver() = default;
        virtual void onPresetAdded(const ModelPreset& preset) = 0;
        virtual void onPresetRemoved(const std::string& presetName) = 0;
        virtual void onPresetUpdated(const ModelPreset& preset) = 0;
        virtual void onPresetSelected(const ModelPreset& preset) = 0;
    };
} // namespace Model