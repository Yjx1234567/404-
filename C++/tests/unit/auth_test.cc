#include <gtest/gtest.h>
#include "utils/bcrypt.h"
#include "model/user.h"
#include "model/session.h"

using namespace oj;

TEST(BcryptTest, HashPassword) {
    std::string password = "test123";
    std::string hash = BcryptUtil::hashPassword(password);
    
    EXPECT_FALSE(hash.empty());
    EXPECT_TRUE(hash.length() > 0);
    // bcrypt hash starts with $2a$ or $2y$ or $2b$
    EXPECT_TRUE(hash.find("$2") == 0);
}

TEST(BcryptTest, VerifyPassword) {
    std::string password = "test123";
    std::string hash = BcryptUtil::hashPassword(password);
    
    EXPECT_TRUE(BcryptUtil::verifyPassword(password, hash));
    EXPECT_FALSE(BcryptUtil::verifyPassword("wrong", hash));
    EXPECT_FALSE(BcryptUtil::verifyPassword("", hash));
}

TEST(BcryptTest, VerifyInvalidHash) {
    EXPECT_FALSE(BcryptUtil::verifyPassword("test", "invalid"));
}

TEST(UserTest, DefaultConstructor) {
    User u;
    EXPECT_EQ(u.getId(), 0);
    EXPECT_TRUE(u.getUsername().empty());
    EXPECT_TRUE(u.getPassword().empty());
    EXPECT_TRUE(u.getRole().empty());
    EXPECT_TRUE(u.getCreatedAt().empty());
}

TEST(UserTest, ConstructorAndGetters) {
    User u(1, "testuser", "hashed_pass", "user", "2025-01-01 00:00:00");
    EXPECT_EQ(u.getId(), 1);
    EXPECT_EQ(u.getUsername(), "testuser");
    EXPECT_EQ(u.getPassword(), "hashed_pass");
    EXPECT_EQ(u.getRole(), "user");
    EXPECT_EQ(u.getCreatedAt(), "2025-01-01 00:00:00");
}

TEST(SessionTest, DefaultConstructor) {
    Session s;
    EXPECT_EQ(s.getId(), 0);
    EXPECT_EQ(s.getUserId(), 0);
    EXPECT_TRUE(s.getToken().empty());
    EXPECT_TRUE(s.isExpired());
}

TEST(SessionTest, GenerateToken) {
    std::string t1 = Session::generateToken();
    std::string t2 = Session::generateToken();
    
    EXPECT_EQ(t1.length(), 32);
    EXPECT_EQ(t2.length(), 32);
    EXPECT_NE(t1, t2);
}

TEST(SessionTest, IsExpired) {
    Session s;
    s.setExpiresAt("2020-01-01 00:00:00");
    EXPECT_TRUE(s.isExpired());
    
    // Set expiry to far future
    s.setExpiresAt("2099-12-31 23:59:59");
    EXPECT_FALSE(s.isExpired());
}
