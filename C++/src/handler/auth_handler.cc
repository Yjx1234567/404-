#include "handler/auth_handler.h"
#include "service/auth_service.h"
#include "model/user.h"
#include "model/session.h"
#include "utils/bcrypt.h"
#include "utils/logger.h"
#include <json/json.h>
#include <sstream>

namespace oj {

std::pair<int, std::string> handleRegister(const std::string& body) {
    Json::Value root;
    Json::Reader reader;
    Json::Value request;
    
    if (!reader.parse(body, request)) {
        root["error"] = "invalid json";
        Json::StreamWriterBuilder builder;
        return {400, Json::writeString(builder, root)};
    }
    
    if (!request.isMember("username") || request["username"].asString().empty()) {
        root["error"] = "username is required";
        Json::StreamWriterBuilder builder;
        return {400, Json::writeString(builder, root)};
    }
    
    if (!request.isMember("password") || request["password"].asString().empty()) {
        root["error"] = "password is required";
        Json::StreamWriterBuilder builder;
        return {400, Json::writeString(builder, root)};
    }
    
    std::string username = request["username"].asString();
    std::string password = request["password"].asString();
    
    if (!AuthService::validateUsername(username)) {
        root["error"] = "username must be between 3 and 64 characters";
        Json::StreamWriterBuilder builder;
        return {400, Json::writeString(builder, root)};
    }
    
    if (!AuthService::validatePassword(password)) {
        root["error"] = "password must be at least 6 characters";
        Json::StreamWriterBuilder builder;
        return {400, Json::writeString(builder, root)};
    }
    
    if (User::usernameExists(username)) {
        root["error"] = "username already exists";
        Json::StreamWriterBuilder builder;
        return {409, Json::writeString(builder, root)};
    }
    
    int user_id = 0;
    if (!AuthService::registerUser(username, password, user_id)) {
        root["error"] = "failed to create user";
        Json::StreamWriterBuilder builder;
        return {500, Json::writeString(builder, root)};
    }
    
    root["message"] = "user registered successfully";
    root["id"] = user_id;
    Json::StreamWriterBuilder builder;
    return {201, Json::writeString(builder, root)};
}

std::pair<int, std::string> handleLogin(const std::string& body) {
    Json::Value root;
    Json::Reader reader;
    Json::Value request;
    
    if (!reader.parse(body, request)) {
        root["error"] = "invalid json";
        Json::StreamWriterBuilder builder;
        return {400, Json::writeString(builder, root)};
    }
    
    if (!request.isMember("username") || request["username"].asString().empty()) {
        root["error"] = "username is required";
        Json::StreamWriterBuilder builder;
        return {400, Json::writeString(builder, root)};
    }
    
    if (!request.isMember("password") || request["password"].asString().empty()) {
        root["error"] = "password is required";
        Json::StreamWriterBuilder builder;
        return {400, Json::writeString(builder, root)};
    }
    
    std::string username = request["username"].asString();
    std::string password = request["password"].asString();
    
    auto session_opt = AuthService::loginUser(username, password);
    if (!session_opt.has_value()) {
        root["error"] = "invalid username or password";
        Json::StreamWriterBuilder builder;
        return {401, Json::writeString(builder, root)};
    }
    
    // Get user info
    auto user_opt = AuthService::getUserBySession(session_opt->getToken());
    if (!user_opt.has_value()) {
        root["error"] = "invalid username or password";
        Json::StreamWriterBuilder builder;
        return {401, Json::writeString(builder, root)};
    }
    
    root["message"] = "login successful";
    root["username"] = user_opt->getUsername();
    root["role"] = user_opt->getRole();
    root["session_id"] = session_opt->getToken();
    Json::StreamWriterBuilder builder;
    return {200, Json::writeString(builder, root)};
}

std::pair<int, std::string> handleLogout(const std::string& session_token) {
    Json::Value root;
    
    if (session_token.empty()) {
        root["error"] = "not logged in";
        Json::StreamWriterBuilder builder;
        return {400, Json::writeString(builder, root)};
    }
    
    if (!AuthService::logoutUser(session_token)) {
        root["error"] = "logout failed";
        Json::StreamWriterBuilder builder;
        return {500, Json::writeString(builder, root)};
    }
    
    root["message"] = "logout successful";
    Json::StreamWriterBuilder builder;
    return {200, Json::writeString(builder, root)};
}

} // namespace oj
