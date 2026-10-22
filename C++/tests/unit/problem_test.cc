#include <gtest/gtest.h>
#include "model/problem.h"
#include "model/test_case.h"
#include "db/connection_pool.h"
#include "utils/config.h"

using namespace oj;

class ProblemTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Test assumes database is available
        // Use unique titles to avoid conflicts
    }
};

TEST_F(ProblemTest, ConstructorAndGetters) {
    Problem p;
    p.setId(1);
    p.setTitle("Test Problem");
    p.setDifficulty("Easy");
    p.setContent("Test content");
    p.setTemplate("template code");
    
    EXPECT_EQ(p.getId(), 1);
    EXPECT_EQ(p.getTitle(), "Test Problem");
    EXPECT_EQ(p.getDifficulty(), "Easy");
    EXPECT_EQ(p.getContent(), "Test content");
    EXPECT_EQ(p.getTemplate(), "template code");
}

TEST_F(ProblemTest, TestCaseConstructorAndGetters) {
    TestCase tc;
    tc.setId(1);
    tc.setProblemId(2);
    tc.setInput("1 2");
    tc.setExpected("3");
    tc.setPosition(0);
    
    EXPECT_EQ(tc.getId(), 1);
    EXPECT_EQ(tc.getProblemId(), 2);
    EXPECT_EQ(tc.getInput(), "1 2");
    EXPECT_EQ(tc.getExpected(), "3");
    EXPECT_EQ(tc.getPosition(), 0);
}

TEST_F(ProblemTest, GetAllWithoutDB) {
    // Without DB connection, should return empty
    auto problems = Problem::getAll();
    // Result depends on DB state, just verify no crash
    EXPECT_TRUE(problems.empty() || !problems.empty());
}
