#pragma once

#include <string>
#include <vector>
#include <memory>

namespace oj {

class TestCase {
public:
    TestCase();
    TestCase(int id, int problem_id, const std::string& input,
             const std::string& expected, int position);
    
    int getId() const { return id_; }
    void setId(int id) { id_ = id; }
    
    int getProblemId() const { return problem_id_; }
    void setProblemId(int pid) { problem_id_ = pid; }
    
    const std::string& getInput() const { return input_; }
    void setInput(const std::string& input) { input_ = input; }
    
    const std::string& getExpected() const { return expected_; }
    void setExpected(const std::string& expected) { expected_ = expected; }
    
    int getPosition() const { return position_; }
    void setPosition(int pos) { position_ = pos; }
    
    bool saveToDB();
    bool loadFromDB(int id);
    bool deleteFromDB();

    static std::vector<TestCase> getByProblemId(int problem_id);
    // 整体替换某题的用例前调用
    static bool deleteByProblemId(int problem_id);
    
private:
    int id_;
    int problem_id_;
    std::string input_;
    std::string expected_;
    int position_;
};

} // namespace oj
