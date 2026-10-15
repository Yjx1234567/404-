#include "utils/config.h"
#include "utils/logger.h"
#include <fstream>
#include <sstream>

namespace oj {

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::loadFromFile(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        config_ = YAML::LoadFile(filepath);
        filepath_ = filepath;
        return true;
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to load config file: " + std::string(e.what()));
        return false;
    }
}

bool ConfigManager::reload() {
    if (filepath_.empty()) {
        return false;
    }
    return loadFromFile(filepath_);
}

void ConfigManager::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = YAML::Node();
    filepath_.clear();
}

std::string ConfigManager::getString(const std::string& key, const std::string& defaultValue) const {
    return get<std::string>(key, defaultValue);
}

int ConfigManager::getInt(const std::string& key, int defaultValue) const {
    return get<int>(key, defaultValue);
}

bool ConfigManager::getBool(const std::string& key, bool defaultValue) const {
    return get<bool>(key, defaultValue);
}

bool ConfigManager::hasKey(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        getValue<std::string>(key);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace oj
