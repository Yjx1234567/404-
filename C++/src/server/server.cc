#include "server/server.h"
#include "server/router.h"
#include "utils/config.h"
#include "utils/logger.h"
#include <thread>

namespace oj {

Server::Server() : running_(false) {}

Server::~Server() {
    if (running_) {
        stop();
    }
}

Server& Server::getInstance() {
    static Server instance;
    return instance;
}

bool Server::start(const std::string& host, int port) {
    if (running_) {
        return false;
    }
    
    Router::getInstance().setupRoutes(server_);
    
    Logger::getInstance().info("Server starting on " + host + ":" + std::to_string(port));
    
    running_ = true;
    if (!server_.bind_to_port(host.c_str(), port)) {
        Logger::getInstance().error("Failed to bind to port " + std::to_string(port));
        running_ = false;
        return false;
    }
    
    server_.listen_after_bind();
    return true;
}

void Server::stop() {
    if (!running_) {
        return;
    }
    server_.stop();
    running_ = false;
    Logger::getInstance().info("Server stopped");
}

bool Server::isRunning() const {
    return running_;
}

} // namespace oj
