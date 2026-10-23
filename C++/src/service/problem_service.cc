#include "service/problem_service.h"
#include "db/connection_pool.h"
#include "utils/logger.h"
#include <mysql/mysql.h>
#include <algorithm>

namespace oj {

std::vector<Problem> ProblemService::listProblems() {
    return Problem::getAll();
}

std::vector<Problem> ProblemService::listProblemsByDifficulty(const std::string& difficulty) {
    return Problem::getByDifficulty(difficulty);
}

std::vector<Problem> ProblemService::searchProblems(const ProblemQuery& query, int& total) {
    return Problem::search(query, total);
}

Problem ProblemService::getProblem(int id) {
    Problem p;
    p.loadFromDB(id);
    return p;
}

bool ProblemService::createProblem(const std::string& title, const std::string& difficulty,
                                   const std::string& content, const std::string& template_code,
                                   const std::string& spj_code, const std::vector<std::string>& tags,
                                   const std::vector<TestCase>& test_cases, int& out_id) {
    if (!validateDifficulty(difficulty)) {
        return false;
    }

    Problem p;
    p.setTitle(title);
    p.setDifficulty(difficulty);
    p.setContent(content);
    p.setTemplate(template_code);
    p.setSpjCode(spj_code);
    p.setTags(tags);

    if (!p.saveToDB()) {
        return false;
    }

    out_id = p.getId();

    for (const auto& tc : test_cases) {
        TestCase newTc = tc;
        newTc.setProblemId(out_id);
        if (!newTc.saveToDB()) {
            Logger::getInstance().warning("Failed to save test case for problem " + std::to_string(out_id));
        }
    }

    return true;
}

bool ProblemService::deleteProblem(int id) {
    Problem p;
    if (!p.loadFromDB(id)) {
        return false;
    }
    return p.deleteFromDB();
}

bool ProblemService::validateDifficulty(const std::string& difficulty) {
    return difficulty == "Easy" || difficulty == "Medium" || difficulty == "Hard";
}

} // namespace oj
