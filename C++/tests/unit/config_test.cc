#include <gtest/gtest.h>
#include "utils/config.h"
#include <fstream>
#include <filesystem>

using namespace oj;

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        ConfigManager::getInstance().reset();
    }
    
    void TearDown() override {
        ConfigManager::getInstance().reset();
    }
};

TEST_F(ConfigTest, LoadFromFile) {
    std::string testFile = "/tmp/test_config.yaml";
    std::ofstream ofs(testFile);
    ofs << "server:\n  host: 127.0.0.1\n  port: 9090\n";
    ofs.close();
    
    EXPECT_TRUE(ConfigManager::getInstance().loadFromFile(testFile));
    EXPECT_EQ(ConfigManager::getInstance().getString("server.host"), "127.0.0.1");
    EXPECT_EQ(ConfigManager::getInstance().getInt("server.port"), 9090);
    
    std::filesystem::remove(testFile);
}

TEST_F(ConfigTest, GetDefaultValue) {
    EXPECT_EQ(ConfigManager::getInstance().getString("nonexistent", "default"), "default");
    EXPECT_EQ(ConfigManager::getInstance().getInt("nonexistent", 42), 42);
    EXPECT_EQ(ConfigManager::getInstance().getBool("nonexistent", true), true);
}

TEST_F(ConfigTest, HasKey) {
    std::string testFile = "/tmp/test_config2.yaml";
    std::ofstream ofs(testFile);
    ofs << "test:\n  key: value\n";
    ofs.close();
    
    ConfigManager::getInstance().loadFromFile(testFile);
    EXPECT_TRUE(ConfigManager::getInstance().hasKey("test.key"));
    EXPECT_FALSE(ConfigManager::getInstance().hasKey("test.nonexistent"));
    
    std::filesystem::remove(testFile);
}

TEST_F(ConfigTest, Reset) {
    std::string testFile = "/tmp/test_config3.yaml";
    std::ofstream ofs(testFile);
    ofs << "test: value\n";
    ofs.close();
    
    ConfigManager::getInstance().loadFromFile(testFile);
    EXPECT_TRUE(ConfigManager::getInstance().hasKey("test"));
    
    ConfigManager::getInstance().reset();
    EXPECT_FALSE(ConfigManager::getInstance().hasKey("test"));
    
    std::filesystem::remove(testFile);
}
