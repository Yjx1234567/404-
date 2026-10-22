#pragma once

#include <httplib.h>
#include <string>

namespace oj {

class Server {
public:
    static Server& getInstance();
    
    bool start(const std::string& host, int port);
    void stop();
    bool isRunning() const;
    
private:
    Server();
    ~Server();
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
    
    httplib::Server server_;
    bool running_;
};

} // namespace oj
