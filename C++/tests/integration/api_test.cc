#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

TEST(Integration, ProjectStructure) {
    // Verify project structure exists
    EXPECT_TRUE(std::filesystem::exists("CMakeLists.txt"));
    EXPECT_TRUE(std::filesystem::exists("config/config.yaml"));
    EXPECT_TRUE(std::filesystem::exists("database/init.sql"));
    EXPECT_TRUE(std::filesystem::exists("src/main.cc"));
    EXPECT_TRUE(std::filesystem::exists("src/server/server.h"));
    EXPECT_TRUE(std::filesystem::exists("src/server/router.cc"));
    EXPECT_TRUE(std::filesystem::exists("src/handler/auth_handler.cc"));
    EXPECT_TRUE(std::filesystem::exists("src/handler/problem_handler.cc"));
    EXPECT_TRUE(std::filesystem::exists("src/handler/submit_handler.cc"));
    EXPECT_TRUE(std::filesystem::exists("src/handler/admin_handler.cc"));
    EXPECT_TRUE(std::filesystem::exists("src/service/problem_service.cc"));
    EXPECT_TRUE(std::filesystem::exists("src/service/executor_service.cc"));
    EXPECT_TRUE(std::filesystem::exists("src/service/auth_service.cc"));
    EXPECT_TRUE(std::filesystem::exists("src/model/problem.cc"));
    EXPECT_TRUE(std::filesystem::exists("src/model/test_case.cc"));
    EXPECT_TRUE(std::filesystem::exists("src/model/user.cc"));
    EXPECT_TRUE(std::filesystem::exists("src/model/session.cc"));
    EXPECT_TRUE(std::filesystem::exists("src/db/connection_pool.cc"));
    EXPECT_TRUE(std::filesystem::exists("src/utils/logger.cc"));
    EXPECT_TRUE(std::filesystem::exists("src/utils/config.cc"));
    EXPECT_TRUE(std::filesystem::exists("src/utils/bcrypt.cc"));
    EXPECT_TRUE(std::filesystem::exists("public/index.html"));
    EXPECT_TRUE(std::filesystem::exists("public/login.html"));
    EXPECT_TRUE(std::filesystem::exists("public/register.html"));
    EXPECT_TRUE(std::filesystem::exists("public/problem_list.html"));
    EXPECT_TRUE(std::filesystem::exists("public/problem.html"));
    EXPECT_TRUE(std::filesystem::exists("public/admin.html"));
    EXPECT_TRUE(std::filesystem::exists("public/css/style.css"));
    EXPECT_TRUE(std::filesystem::exists("public/js/api.js"));
    EXPECT_TRUE(std::filesystem::exists("public/js/auth.js"));
    EXPECT_TRUE(std::filesystem::exists("public/js/problem.js"));
    EXPECT_TRUE(std::filesystem::exists("public/js/problem_detail.js"));
    EXPECT_TRUE(std::filesystem::exists("public/js/admin.js"));
    EXPECT_TRUE(std::filesystem::exists("tools/create_admin_user.cc"));
    EXPECT_TRUE(std::filesystem::exists("tools/db_reset_tool.cc"));
}

TEST(Integration, ConfigFileValid) {
    EXPECT_TRUE(std::filesystem::exists("config/config.yaml"));
    std::ifstream file("config/config.yaml");
    EXPECT_TRUE(file.is_open());
    
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    
    EXPECT_NE(content.find("server:"), std::string::npos);
    EXPECT_NE(content.find("database:"), std::string::npos);
    EXPECT_NE(content.find("execution:"), std::string::npos);
    EXPECT_NE(content.find("logging:"), std::string::npos);
}

TEST(Integration, DatabaseScriptValid) {
    EXPECT_TRUE(std::filesystem::exists("database/init.sql"));
    std::ifstream file("database/init.sql");
    EXPECT_TRUE(file.is_open());
    
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    
    EXPECT_NE(content.find("CREATE TABLE IF NOT EXISTS problems"), std::string::npos);
    EXPECT_NE(content.find("CREATE TABLE IF NOT EXISTS test_cases"), std::string::npos);
    EXPECT_NE(content.find("CREATE TABLE IF NOT EXISTS users"), std::string::npos);
    EXPECT_NE(content.find("CREATE TABLE IF NOT EXISTS sessions"), std::string::npos);
}
