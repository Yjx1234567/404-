#include "handler/admin_handler.h"
#include "service/problem_service.h"
#include "model/problem.h"
#include "model/test_case.h"
#include "utils/logger.h"
#include <json/json.h>
#include <sstream>

namespace oj {

namespace {

// 从请求 JSON 中提取题目字段（create/update 共用），校验失败时返回 false 并写入 error
bool parseProblemBody(const Json::Value& request, Json::Value& error_root,
                      std::string& title, std::string& difficulty, std::string& content,
                      std::string& template_code, std::string& spj_code,
                      std::vector<TestCase>& test_cases, std::vector<std::string>& tags) {
    Json::StreamWriterBuilder builder;

    if (!request.isMember("title") || request["title"].asString().empty()) {
        error_root["error"] = "title is required";
        return false;
    }
    if (!request.isMember("difficulty") || request["difficulty"].asString().empty()) {
        error_root["error"] = "difficulty is required";
        return false;
    }
    if (!request.isMember("content") || request["content"].asString().empty()) {
        error_root["error"] = "content is required";
        return false;
    }

    title = request["title"].asString();
    difficulty = request["difficulty"].asString();
    content = request["content"].asString();
    template_code = request.isMember("template") ? request["template"].asString() : "";
    spj_code = request.isMember("spj_code") ? request["spj_code"].asString() : "";

    if (!ProblemService::validateDifficulty(difficulty)) {
        error_root["error"] = "invalid difficulty, must be Easy, Medium, or Hard";
        return false;
    }

    if (request.isMember("test_cases") && request["test_cases"].isArray()) {
        for (const auto& tc : request["test_cases"]) {
            if (!tc.isMember("input") || !tc.isMember("expected")) {
                continue;
            }
            TestCase tcase;
            tcase.setInput(tc["input"].asString());
            tcase.setExpected(tc["expected"].asString());
            tcase.setPosition(test_cases.size());
            test_cases.push_back(tcase);
        }
    }

    if (request.isMember("tags") && request["tags"].isArray()) {
        for (const auto& t : request["tags"]) {
            if (t.isString() && !t.asString().empty()) {
                tags.push_back(t.asString());
            }
        }
    }

    return true;
}

// 保存测试用例（update 场景先清空旧的）
bool saveTestCases(int problem_id, const std::vector<TestCase>& test_cases) {
    for (const auto& tc : test_cases) {
        TestCase newTc = tc;
        newTc.setProblemId(problem_id);
        if (!newTc.saveToDB()) {
            Logger::getInstance().warning("Failed to save test case for problem " + std::to_string(problem_id));
            return false;
        }
    }
    return true;
}

} // namespace

std::pair<int, std::string> handleCreateProblem(const std::string& body) {
    Json::Value root;
    Json::Reader reader;
    Json::Value request;

    if (!reader.parse(body, request)) {
        root["error"] = "invalid json";
        Json::StreamWriterBuilder builder;
        return {400, Json::writeString(builder, root)};
    }

    std::string title, difficulty, content, template_code, spj_code;
    std::vector<TestCase> test_cases;
    std::vector<std::string> tags;

    if (!parseProblemBody(request, root, title, difficulty, content, template_code,
                          spj_code, test_cases, tags)) {
        Json::StreamWriterBuilder builder;
        return {400, Json::writeString(builder, root)};
    }

    int out_id = 0;
    if (!ProblemService::createProblem(title, difficulty, content, template_code,
                                       spj_code, tags, test_cases, out_id)) {
        root["error"] = "failed to save problem";
        Json::StreamWriterBuilder builder;
        return {500, Json::writeString(builder, root)};
    }

    root.clear();
    root["id"] = out_id;
    root["message"] = "problem created successfully";
    Json::StreamWriterBuilder builder;
    return {201, Json::writeString(builder, root)};
}

std::pair<int, std::string> handleUpdateProblem(int id, const std::string& body) {
    Json::Value root;
    Json::Reader reader;
    Json::Value request;

    if (id <= 0) {
        root["error"] = "invalid problem id";
        Json::StreamWriterBuilder builder;
        return {400, Json::writeString(builder, root)};
    }
    if (!reader.parse(body, request)) {
        root["error"] = "invalid json";
        Json::StreamWriterBuilder builder;
        return {400, Json::writeString(builder, root)};
    }

    Problem existing;
    if (!existing.loadFromDB(id)) {
        root["error"] = "problem not found";
        Json::StreamWriterBuilder builder;
        return {404, Json::writeString(builder, root)};
    }

    std::string title, difficulty, content, template_code, spj_code;
    std::vector<TestCase> test_cases;
    std::vector<std::string> tags;

    if (!parseProblemBody(request, root, title, difficulty, content, template_code,
                          spj_code, test_cases, tags)) {
        Json::StreamWriterBuilder builder;
        return {400, Json::writeString(builder, root)};
    }

    existing.setTitle(title);
    existing.setDifficulty(difficulty);
    existing.setContent(content);
    existing.setTemplate(template_code);
    existing.setSpjCode(spj_code);
    existing.setTags(tags);

    if (!existing.updateToDB()) {
        root["error"] = "failed to update problem";
        Json::StreamWriterBuilder builder;
        return {500, Json::writeString(builder, root)};
    }

    // test_cases 提供时整体替换
    if (request.isMember("test_cases") && request["test_cases"].isArray()) {
        TestCase::deleteByProblemId(id);
        saveTestCases(id, test_cases);
    }

    root.clear();
    root["id"] = id;
    root["message"] = "problem updated successfully";
    Json::StreamWriterBuilder builder;
    return {200, Json::writeString(builder, root)};
}

std::pair<int, std::string> handleGetProblemAdmin(int id) {
    Json::Value root;

    if (id <= 0) {
        root["error"] = "invalid problem id";
        Json::StreamWriterBuilder builder;
        return {400, Json::writeString(builder, root)};
    }

    Problem p;
    if (!p.loadFromDB(id)) {
        root["error"] = "problem not found";
        Json::StreamWriterBuilder builder;
        return {404, Json::writeString(builder, root)};
    }

    root["id"] = p.getId();
    root["title"] = p.getTitle();
    root["difficulty"] = p.getDifficulty();
    root["content"] = p.getContent();
    root["template"] = p.getTemplate();
    root["spj_code"] = p.getSpjCode();
    root["created_at"] = p.getCreatedAt();

    Json::Value tags(Json::arrayValue);
    for (const auto& t : p.getTags()) {
        tags.append(t);
    }
    root["tags"] = tags;

    Json::Value test_cases(Json::arrayValue);
    auto tcs = p.getTestCases();
    for (const auto& tc : tcs) {
        Json::Value t;
        t["id"] = tc.getId();
        t["input"] = tc.getInput();
        t["expected"] = tc.getExpected();
        t["position"] = tc.getPosition();
        test_cases.append(t);
    }
    root["test_cases"] = test_cases;

    Json::StreamWriterBuilder builder;
    return {200, Json::writeString(builder, root)};
}

std::pair<int, std::string> handleDeleteProblem(int id) {
    Json::Value root;

    if (id <= 0) {
        root["error"] = "invalid problem id";
        Json::StreamWriterBuilder builder;
        return {400, Json::writeString(builder, root)};
    }

    if (!ProblemService::deleteProblem(id)) {
        root["error"] = "failed to delete problem";
        Json::StreamWriterBuilder builder;
        return {500, Json::writeString(builder, root)};
    }

    root["message"] = "problem deleted successfully";
    Json::StreamWriterBuilder builder;
    return {200, Json::writeString(builder, root)};
}

} // namespace oj
