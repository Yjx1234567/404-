#pragma once

#include <string>
#include <functional>
#include <map>
#include <httplib.h>

namespace oj {

using Handler = std::function<std::string(const std::string& body)>;

class Router {
public:
    static Router& getInstance();
    
    void setupRoutes(httplib::Server& server);
    
private:
    Router() = default;
    
    std::string extractSessionToken(const httplib::Request& req);
    bool isLoggedIn(const httplib::Request& req);
    bool isAdmin(const httplib::Request& req);
    
    void setJsonResponse(std::string& res, const std::string& body, int status = 200);
};

} // namespace oj
