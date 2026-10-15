#pragma once

#include <string>

namespace oj {

// 使用 Botan-2 实现 bcrypt 密码哈希
class BcryptUtil {
public:
    // 生成 bcrypt 哈希 (包含随机 salt)
    static std::string hashPassword(const std::string& password);
    
    // 验证密码是否匹配哈希
    static bool verifyPassword(const std::string& password, const std::string& hash);
};

} // namespace oj
