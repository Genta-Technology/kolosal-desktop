#pragma once

#include "model_persistence.hpp"

#include <string>
#include <vector>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <future>
#include <curl/curl.h>

namespace Model
{

    class ModelManager
    {
    public:
        static ModelManager &getInstance()
        {
            static ModelManager instance(std::make_unique<FileModelPersistence>("models"));
            return instance;
        }

        ModelManager(const ModelManager &) = delete;
        ModelManager &operator=(const ModelManager &) = delete;
        ModelManager(ModelManager &&) = delete;
        ModelManager &operator=(ModelManager &&) = delete;

        void initialize(std::unique_ptr<IModelPersistence> persistence)
        {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            m_persistence = std::move(persistence);
            m_currentModelName = std::nullopt;
            m_currentModelIndex = 0;
            loadModelsAsync();
        }

        // Switch to a specific model variant. If not downloaded, trigger download.
        bool switchModel(const std::string &modelName, const std::string &variantType)
        {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            auto it = m_modelNameToIndex.find(modelName);
            if (it == m_modelNameToIndex.end())
            {
                return false; // Model not found
            }

            m_currentModelName = modelName;
            m_currentVariantType = variantType;
            m_currentModelIndex = it->second;

            // If not downloaded, start download
            ModelVariant *variant = getVariantLocked(m_currentModelIndex, m_currentVariantType);
            if (variant && !variant->isDownloaded && variant->downloadProgress == 0.0)
            {
                startDownloadAsyncLocked(m_currentModelIndex, m_currentVariantType);
            }

            return true;
        }

        bool downloadModel(size_t modelIndex, const std::string &variantType)
        {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            if (modelIndex >= m_models.size())
            {
                return false; // Invalid index
            }

            ModelVariant *variant = getVariantLocked(modelIndex, variantType);
            if (!variant)
                return false;

            // If already downloaded or currently downloading (progress > 0 but not finished), do nothing
            if (variant->isDownloaded || variant->downloadProgress > 0.0)
            {
                return false;
            }

            // Start new download
            startDownloadAsyncLocked(modelIndex, variantType);
            return true;
        }

        bool isModelDownloaded(size_t modelIndex, const std::string &variantType) const
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            if (modelIndex >= m_models.size())
                return false;

            const ModelVariant *variant = getVariantLocked(modelIndex, variantType);
            return variant ? variant->isDownloaded : false;
        }

        double getModelDownloadProgress(size_t modelIndex, const std::string &variantType) const
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            if (modelIndex >= m_models.size())
                return 0.0;

            const ModelVariant *variant = getVariantLocked(modelIndex, variantType);
            return variant ? variant->downloadProgress : 0.0;
        }

        std::vector<ModelData> getModels() const
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            return m_models;
        }

        std::optional<std::string> getCurrentModelName() const
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            return m_currentModelName;
        }

        std::string getCurrentVariantType() const
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            return m_currentVariantType;
        }

        double getCurrentVariantProgress() const
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            const ModelVariant *variant = getVariantLocked(m_currentModelIndex, m_currentVariantType);
            return variant ? variant->downloadProgress : 0.0;
        }

    private:
        explicit ModelManager(std::unique_ptr<IModelPersistence> persistence)
            : m_persistence(std::move(persistence)),
              m_currentModelName(std::nullopt),
              m_currentModelIndex(0)
        {
            loadModelsAsync();
        }

        void loadModelsAsync() {
            std::async(std::launch::async, [this]() {
                auto models = m_persistence->loadAllModels().get();

                // After loading, check file existence if isDownloaded is true
                for (auto& model : models) 
                {
                    checkAndFixDownloadStatus(model.fullPrecision);
                    checkAndFixDownloadStatus(model.quantized4Bit);
                }

                std::unique_lock<std::shared_mutex> lock(m_mutex);
                m_models = std::move(models);
                m_modelNameToIndex.clear();

                for (size_t i = 0; i < m_models.size(); ++i) 
                {
                    m_modelNameToIndex[m_models[i].name] = i;
                }

                // Find the first model that is already downloaded (either variant)
                bool anyDownloaded = false;
                for (size_t i = 0; i < m_models.size(); ++i) 
                {
                    const auto& model = m_models[i];
                    if (model.fullPrecision.isDownloaded) 
                    {
                        m_currentModelName = model.name;
                        m_currentModelIndex = i;
                        m_currentVariantType = "Full Precision";
                        anyDownloaded = true;
                        break;
                    } 
                    else if (model.quantized4Bit.isDownloaded) 
                    {
                        m_currentModelName = model.name;
                        m_currentModelIndex = i;
                        m_currentVariantType = "4-bit Quantized";
                        anyDownloaded = true;
                        break;
                    }
                }

                // If no model is downloaded at start, no model is selected
                if (!anyDownloaded) 
                {
                    m_currentModelName = std::nullopt;
                    m_currentVariantType.clear();
                    // We can leave m_currentModelIndex as-is or reset it. 
                    // Since no model is selected, it won't be used.
                    m_currentModelIndex = 0;
                }
            });
        }

        void checkAndFixDownloadStatus(ModelVariant& variant) 
        {
            if (variant.isDownloaded) 
            {
                // Check if file exists
                if (!std::filesystem::exists(variant.path)) 
                {
                    // File doesn't exist, reset
                    variant.isDownloaded = false;
                    variant.downloadProgress = 0.0;
                }
            }
        }

        ModelVariant *getVariantLocked(size_t modelIndex, const std::string &variantType) const
        {
            if (modelIndex >= m_models.size())
                return nullptr;

            const auto &model = m_models[modelIndex];
            if (variantType == "Full Precision")
            {
                return const_cast<ModelVariant *>(&model.fullPrecision);
            }
            else
            {
                return const_cast<ModelVariant *>(&model.quantized4Bit);
            }
        }

        void startDownloadAsyncLocked(size_t modelIndex, const std::string &variantType)
        {
            if (modelIndex >= m_models.size())
                return;

            ModelVariant* variant = getVariantLocked(modelIndex, variantType);
            if (!variant)
                return;

            ModelData* model = &m_models[modelIndex];

            m_downloadFutures.emplace_back(m_persistence->downloadModelVariant(*model, *variant));
        }

        mutable std::shared_mutex m_mutex;
        std::unique_ptr<IModelPersistence> m_persistence;
        std::vector<ModelData> m_models;
        std::unordered_map<std::string, size_t> m_modelNameToIndex;
        std::optional<std::string> m_currentModelName;
        std::string m_currentVariantType;
        size_t m_currentModelIndex;
        std::vector<std::future<void>> m_downloadFutures;
    };

    inline void initializeModelManager()
    {
        ModelManager::getInstance();
    }

    inline void initializeModelManagerWithCustomPersistence(std::unique_ptr<IModelPersistence> persistence)
    {
        ModelManager::getInstance().initialize(std::move(persistence));
    }

} // namespace Model