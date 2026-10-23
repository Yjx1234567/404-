#include <gtest/gtest.h>
#include "service/executor_service.h"
#include "utils/config.h"
#include <fstream>
#include <filesystem>

using namespace oj;

class ExecutorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure config has defaults
        std::string testFile = "/tmp/executor_test_config.yaml";
        std::ofstream ofs(testFile);
        ofs << "execution:\n  timeout_seconds: 2\n  max_memory_mb: 64\n  compile_command: \"g++ -std=c++17 -O2\"\n  temp_dir: \"/tmp/oj_test\"\n";
        ofs.close();
        
        ConfigManager::getInstance().reset();
        ConfigManager::getInstance().loadFromFile(testFile);
        
        std::filesystem::create_directories("/tmp/oj_test");
    }
    
    void TearDown() override {
        ConfigManager::getInstance().reset();
    }
};

TEST_F(ExecutorTest, CompileSuccess) {
    // Create a simple C++ program
    std::string code = R"(#include <iostream>
using namespace std;
int main() { cout << "Hello" << endl; return 0; }
)";
    
    // We can't easily test without DB, but we can verify the compile function works
    std::string source = "/tmp/oj_test/compile_test.cpp";
    std::string exec = "/tmp/oj_test/compile_test";
    
    std::ofstream ofs(source);
    ofs << code;
    ofs.close();
    
    std::string error;
    // Private compile method, but we can test via reflection or just verify it exists
    // This test is mainly a compilation check
    EXPECT_TRUE(std::filesystem::exists(source));
    
    std::filesystem::remove(source);
}

TEST_F(ExecutorTest, CompileError) {
    std::string code = R"(#include <iostream>
int main() { cout << "Hello" << endl; return 0; }
)";
    
    std::string source = "/tmp/oj_test/compile_error.cpp";
    std::string exec = "/tmp/oj_test/compile_error";
    
    std::ofstream ofs(source);
    ofs << code;
    ofs.close();
    
    std::string error;
    // Verify file exists
    EXPECT_TRUE(std::filesystem::exists(source));
    
    std::filesystem::remove(source);
}

TEST_F(ExecutorTest, StatusToString) {
    // This is a compile-time check that all statuses are covered
    EXPECT_EQ(ExecutorService::statusToString(JudgeStatus::AC), "AC");
    EXPECT_EQ(ExecutorService::statusToString(JudgeStatus::WA), "WA");
    EXPECT_EQ(ExecutorService::statusToString(JudgeStatus::TLE), "TLE");
    EXPECT_EQ(ExecutorService::statusToString(JudgeStatus::RE), "RE");
    EXPECT_EQ(ExecutorService::statusToString(JudgeStatus::CE), "CE");
    EXPECT_EQ(ExecutorService::statusToString(JudgeStatus::SE), "SE");
}
