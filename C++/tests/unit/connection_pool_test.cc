#include <gtest/gtest.h>
#include "db/connection_pool.h"
#include "utils/config.h"

using namespace oj;

class ConnectionPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        ConnectionPool::getInstance().reset();
    }
    
    void TearDown() override {
        ConnectionPool::getInstance().reset();
    }
};

TEST_F(ConnectionPoolTest, Singleton) {
    ConnectionPool& p1 = ConnectionPool::getInstance();
    ConnectionPool& p2 = ConnectionPool::getInstance();
    EXPECT_EQ(&p1, &p2);
}

TEST_F(ConnectionPoolTest, NotInitialized) {
    EXPECT_FALSE(ConnectionPool::getInstance().isInitialized());
    EXPECT_EQ(ConnectionPool::getInstance().getTotalCount(), 0);
    EXPECT_EQ(ConnectionPool::getInstance().getAvailableCount(), 0);
}

TEST_F(ConnectionPoolTest, CloseAfterInit) {
    // This test assumes a valid DB connection. Skip if not available.
    GTEST_SKIP() << "Requires database connection";
}
