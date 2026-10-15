#include "utils/bcrypt.h"
#include <botan/bcrypt.h>
#include <botan/system_rng.h>
#include <stdexcept>

namespace oj {

std::string BcryptUtil::hashPassword(const std::string& password) {
    try {
        Botan::System_RNG rng;
        // work_factor = 12, 这是 bcrypt 的默认强度
        return Botan::generate_bcrypt(password, rng, 12);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to hash password: " + std::string(e.what()));
    }
}

bool BcryptUtil::verifyPassword(const std::string& password, const std::string& hash) {
    try {
        return Botan::check_bcrypt(password, hash);
    } catch (const std::exception& e) {
        return false;
    }
}

} // namespace oj
