#pragma once

#include "model/problem.h"
#include "model/test_case.h"
#include <vector>
#include <string>

namespace oj {

class ProblemService {
public:
    static std::vector<Problem> listProblems();
    static std::vector<Problem> listProblemsByDifficulty(const std::string& difficulty);
    // 分页 + 难度 + 关键词 + 标签筛选；total 为符合条件的总数
    static std::vector<Problem> searchProblems(const ProblemQuery& query, int& total);
    static Problem getProblem(int id);
    static bool createProblem(const std::string& title, const std::string& difficulty,
                              const std::string& content, const std::string& template_code,
                              const std::string& spj_code, const std::vector<std::string>& tags,
                              const std::vector<TestCase>& test_cases, int& out_id);
    static bool deleteProblem(int id);
    static bool validateDifficulty(const std::string& difficulty);
};

} // namespace oj
