#pragma once

#include <string>

namespace oj {

class User {
public:
    User();
    User(int id, const std::string& username, const std::string& password,
         const std::string& role, const std::string& created_at);
    
    int getId() const { return id_; }
    void setId(int id) { id_ = id; }
    
    const std::string& getUsername() const { return username_; }
    void setUsername(const std::string& username) { username_ = username; }
    
    const std::string& getPassword() const { return password_; }
    void setPassword(const std::string& password) { password_ = password; }
    
    const std::string& getRole() const { return role_; }
    void setRole(const std::string& role) { role_ = role; }
    
    const std::string& getCreatedAt() const { return created_at_; }
    void setCreatedAt(const std::string& t) { created_at_ = t; }
    
    bool saveToDB();
    bool loadFromDB(int id);
    bool loadFromDBByUsername(const std::string& username);
    bool deleteFromDB();
    
    static User getByUsername(const std::string& username);
    static bool usernameExists(const std::string& username);
    
private:
    int id_;
    std::string username_;
    std::string password_;
    std::string role_;
    std::string created_at_;
};

} // namespace oj
