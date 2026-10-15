#pragma once

#include <string>
#include <utility>

namespace oj {

// Auth handlers: return {status_code, json_body}
std::pair<int, std::string> handleRegister(const std::string& body);
std::pair<int, std::string> handleLogin(const std::string& body);
std::pair<int, std::string> handleLogout(const std::string& session_token);

} // namespace oj
