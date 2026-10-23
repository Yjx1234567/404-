#include "service/auth_service.h"
#include "utils/bcrypt.h"
#include "utils/logger.h"
#include <chrono>
#include <sstream>
#include <iomanip>

namespace oj {

bool AuthService::registerUser(const std::string& username, const std::string& password, int& out_user_id) {
    if (!validateUsername(username) || !validatePassword(password)) {
        return false;
    }
    
    if (User::usernameExists(username)) {
        return false;
    }
    
    User user;
    user.setUsername(username);
    user.setPassword(BcryptUtil::hashPassword(password));
    user.setRole("user");
    
    if (!user.saveToDB()) {
        return false;
    }
    
    out_user_id = user.getId();
    return true;
}

std::optional<Session> AuthService::loginUser(const std::string& username, const std::string& password) {
    User user = User::getByUsername(username);
    if (user.getId() == 0) {
        return std::nullopt;
    }
    
    if (!BcryptUtil::verifyPassword(password, user.getPassword())) {
        return std::nullopt;
    }
    
    // Create session
    Session session;
    session.setUserId(user.getId());
    session.setToken(Session::generateToken());
    
    // Set expiry to 24 hours from now
    auto expiry = std::chrono::system_clock::now() + std::chrono::hours(24);
    auto time_t = std::chrono::system_clock::to_time_t(expiry);
    std::tm tm = *std::localtime(&time_t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    session.setExpiresAt(oss.str());
    
    if (!session.saveToDB()) {
        return std::nullopt;
    }
    
    return session;
}

bool AuthService::logoutUser(const std::string& token) {
    Session session = Session::getByToken(token);
    if (session.getId() == 0) {
        return false;
    }
    return session.deleteByToken();
}

std::optional<User> AuthService::getUserBySession(const std::string& token) {
    if (token.empty()) {
        return std::nullopt;
    }
    
    Session session = Session::getByToken(token);
    if (session.getId() == 0 || session.isExpired()) {
        return std::nullopt;
    }
    
    User user;
    if (!user.loadFromDB(session.getUserId())) {
        return std::nullopt;
    }
    
    return user;
}

bool AuthService::isAdmin(const std::string& token) {
    auto user_opt = getUserBySession(token);
    if (!user_opt.has_value()) {
        return false;
    }
    return user_opt->getRole() == "admin";
}

bool AuthService::validateUsername(const std::string& username) {
    return username.length() >= 3 && username.length() <= 64;
}

bool AuthService::validatePassword(const std::string& password) {
    return password.length() >= 6;
}

} // namespace oj
