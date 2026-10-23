#pragma once

#include "model/user.h"
#include "model/session.h"
#include <string>
#include <optional>

namespace oj {

class AuthService {
public:
    // Register new user
    static bool registerUser(const std::string& username, const std::string& password, int& out_user_id);
    
    // Login user, return session token
    static std::optional<Session> loginUser(const std::string& username, const std::string& password);
    
    // Logout user by token
    static bool logoutUser(const std::string& token);
    
    // Get user by session token
    static std::optional<User> getUserBySession(const std::string& token);
    
    // Check if user is admin
    static bool isAdmin(const std::string& token);
    
    // Validate username format (3-64 chars)
    static bool validateUsername(const std::string& username);
    
    // Validate password (at least 6 chars)
    static bool validatePassword(const std::string& password);
};

} // namespace oj
