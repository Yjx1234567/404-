#pragma once

#include <yaml-cpp/yaml.h>
#include <string>
#include <mutex>
#include <memory>

namespace oj {

class ConfigManager {
public:
    static ConfigManager& getInstance();
    
    bool loadFromFile(const std::string& filepath);
    bool reload();
    void reset();
    
    template<typename T>
    T get(const std::string& key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return getValue<T>(key);
    }
    
    template<typename T>
    T get(const std::string& key, const T& defaultValue) const {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            return getValue<T>(key);
        } catch (...) {
            return defaultValue;
        }
    }
    
    std::string getString(const std::string& key, const std::string& defaultValue = "") const;
    int getInt(const std::string& key, int defaultValue = 0) const;
    bool getBool(const std::string& key, bool defaultValue = false) const;
    
    bool hasKey(const std::string& key) const;
    
private:
    ConfigManager() = default;
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    
    template<typename T>
    T getValue(const std::string& key) const {
        std::vector<std::string> parts;
        size_t start = 0;
        size_t pos = key.find('.');
        while (pos != std::string::npos) {
            parts.push_back(key.substr(start, pos - start));
            start = pos + 1;
            pos = key.find('.', start);
        }
        parts.push_back(key.substr(start));
        
        // 注意：必须深拷贝后再逐级索引。yaml-cpp 的 Node 是共享句柄，
        // 直接 `node = node[part]` 复用中间句柄会在首次遍历后破坏 config_
        // 的底层数据（Ubuntu 22.04 / yaml-cpp 0.7 实测复现），导致后续
        // 所有配置读取退化为默认值。
        YAML::Node node = YAML::Clone(config_);
        for (const auto& part : parts) {
            if (!node[part]) {
                throw std::runtime_error("Config key not found: " + key);
            }
            node = node[part];
        }
        return node.as<T>();
    }
    
    mutable std::mutex mutex_;
    YAML::Node config_;
    std::string filepath_;
};

} // namespace oj
