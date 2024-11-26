#include <string>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

class ConfigManager
{
public:
    static ConfigManager& getInstance()
    {
        static ConfigManager instance;
        return instance;
    }

    void loadConfig(const std::string& filePath)
    {
        std::ifstream file(filePath);
        if (!file.is_open())
        {
            throw std::runtime_error("Unable to open config file: " + filePath);
        }

        file >> jsonConfig;
        file.close();
    }

    template <typename T>
    T get(const std::string& key) const
    {
        try
        {
            return jsonConfig.at(key).get<T>();
        }
        catch (const std::exception& e)
        {
            throw std::runtime_error("Key not found: " + key);
        }
    }

    nlohmann::json getRawConfig() const { return jsonConfig; }

private:
    ConfigManager() = default;
    ~ConfigManager() = default;

    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    nlohmann::json jsonConfig;
};