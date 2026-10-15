#include "server/server.h"
#include "service/judge_queue.h"
#include "db/connection_pool.h"
#include "utils/config.h"
#include "utils/logger.h"
#include <iostream>
#include <csignal>

using namespace oj;

static Server* g_server = nullptr;

void signal_handler(int signal) {
    if (g_server) {
        g_server->stop();
    }
}

int main(int argc, char* argv[]) {
    std::string config_path = "config/config.yaml";
    if (argc > 1) {
        config_path = argv[1];
    }
    
    // Load configuration
    if (!ConfigManager::getInstance().loadFromFile(config_path)) {
        std::cerr << "Failed to load configuration from " << config_path << std::endl;
        return 1;
    }
    
    // Setup logger
    std::string log_level = ConfigManager::getInstance().getString("logging.level", "INFO");
    if (log_level == "DEBUG") {
        Logger::getInstance().setLevel(LogLevel::DEBUG);
    } else if (log_level == "WARNING") {
        Logger::getInstance().setLevel(LogLevel::WARNING);
    } else if (log_level == "ERROR") {
        Logger::getInstance().setLevel(LogLevel::ERROR);
    } else {
        Logger::getInstance().setLevel(LogLevel::INFO);
    }
    
    std::string log_file = ConfigManager::getInstance().getString("logging.file", "server.log");
    Logger::getInstance().setFile(log_file);
    Logger::getInstance().setConsole(ConfigManager::getInstance().getBool("logging.console", true));
    
    // Initialize database connection pool
    std::string db_host = ConfigManager::getInstance().getString("database.host", "localhost");
    int db_port = ConfigManager::getInstance().getInt("database.port", 3306);
    std::string db_user = ConfigManager::getInstance().getString("database.username", "root");
    std::string db_pass = ConfigManager::getInstance().getString("database.password", "");
    std::string db_name = ConfigManager::getInstance().getString("database.database", "oj_system");
    int max_conn = ConfigManager::getInstance().getInt("database.max_connections", 10);
    
    if (!ConnectionPool::getInstance().init(db_host, db_port, db_user, db_pass, db_name, max_conn)) {
        Logger::getInstance().error("Failed to initialize database connection pool");
        return 1;
    }
    
    Logger::getInstance().info("Database connection pool initialized");

    // 启动异步判题队列（同时恢复上次未判完的提交）
    int judge_workers = ConfigManager::getInstance().getInt("execution.judge_workers", 2);
    JudgeQueue::getInstance().start(judge_workers);

    // Start server
    std::string host = ConfigManager::getInstance().getString("server.host", "0.0.0.0");
    int port = ConfigManager::getInstance().getInt("server.port", 8080);

    Server& server = Server::getInstance();
    g_server = &server;

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    if (!server.start(host, port)) {
        Logger::getInstance().error("Failed to start server");
        JudgeQueue::getInstance().stop();
        return 1;
    }

    JudgeQueue::getInstance().stop();
    Logger::getInstance().info("Server stopped gracefully");
    return 0;
}
