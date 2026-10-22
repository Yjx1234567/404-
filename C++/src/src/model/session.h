#pragma once

#include <string>
#include <ctime>

namespace oj {

class Session {
public:
    Session();
    Session(int id, int user_id, const std::string& token,
            const std::string& expires_at, const std::string& created_at);
    
    int getId() const { return id_; }
    void setId(int id) { id_ = id; }
    
    int getUserId() const { return user_id_; }
    void setUserId(int uid) { user_id_ = uid; }
    
    const std::string& getToken() const { return token_; }
    void setToken(const std::string& token) { token_ = token; }
    
    const std::string& getExpiresAt() const { return expires_at_; }
    void setExpiresAt(const std::string& t) { expires_at_ = t; }
    
    const std::string& getCreatedAt() const { return created_at_; }
    void setCreatedAt(const std::string& t) { created_at_ = t; }
    
    bool isExpired() const;
    
    bool saveToDB();
    bool loadFromDB(int id);
    bool loadFromDBByToken(const std::string& token);
    bool deleteFromDB();
    bool deleteByToken();
    
    static Session getByToken(const std::string& token);
    static std::string generateToken();
    
private:
    int id_;
    int user_id_;
    std::string token_;
    std::string expires_at_;
    std::string created_at_;
};

} // namespace oj
