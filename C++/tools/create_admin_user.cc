#include "utils/bcrypt.h"
#include "model/user.h"
#include "db/connection_pool.h"
#include "utils/config.h"
#include "utils/logger.h"
#include <iostream>
#include <cstring>

using namespace oj;

int main(int argc, char* argv[]) {
    if (argc > 1 && (std::strcmp(argv[1], "-h") == 0 || std::strcmp(argv[1], "--help") == 0)) {
        std::cout << "Usage: " << argv[0] << " [config_path]" << std::endl;
        return 0;
    }
    
    std::string config_path = (argc > 1) ? argv[1] : "config/config.yaml";
    
    if (!ConfigManager::getInstance().loadFromFile(config_path)) {
        std::cerr << "Failed to load config from " << config_path << std::endl;
        return 1;
    }
    
    Logger::getInstance().setLevel(LogLevel::INFO);
    Logger::getInstance().setConsole(true);
    
    std::string db_host = ConfigManager::getInstance().getString("database.host", "localhost");
    int db_port = ConfigManager::getInstance().getInt("database.port", 3306);
    std::string db_user = ConfigManager::getInstance().getString("database.username", "root");
    std::string db_pass = ConfigManager::getInstance().getString("database.password", "");
    std::string db_name = ConfigManager::getInstance().getString("database.database", "oj_system");
    int max_conn = ConfigManager::getInstance().getInt("database.max_connections", 10);
    
    if (!ConnectionPool::getInstance().init(db_host, db_port, db_user, db_pass, db_name, max_conn)) {
        std::cerr << "Failed to connect to database" << std::endl;
        return 1;
    }
    
    std::string admin_username = "admin";
    std::string admin_password = "admin123";
    
    // Check if admin exists
    if (User::usernameExists(admin_username)) {
        std::cout << "Admin user already exists. Do you want to reset password? (y/n): ";
        char choice;
        std::cin >> choice;
        if (choice != 'y' && choice != 'Y') {
            std::cout << "Cancelled." << std::endl;
            return 0;
        }
        
        User existing = User::getByUsername(admin_username);
        if (existing.getId() > 0) {
            existing.deleteFromDB();
        }
    }
    
    User admin;
    admin.setUsername(admin_username);
    admin.setPassword(BcryptUtil::hashPassword(admin_password));
    admin.setRole("admin");
    
    if (!admin.saveToDB()) {
        std::cerr << "Failed to create admin user" << std::endl;
        return 1;
    }
    
    std::cout << "Admin user created successfully!" << std::endl;
    std::cout << "Username: " << admin_username << std::endl;
    std::cout << "Password: " << admin_password << std::endl;
    std::cout << "Role: admin" << std::endl;
    
    return 0;
}
