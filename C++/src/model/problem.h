#pragma once

#include <string>
#include <vector>
#include <memory>

namespace oj {

class TestCase;

// 题目列表查询条件（分页 + 难度 + 关键词 + 标签）
struct ProblemQuery {
    int page = 1;
    int page_size = 20;
    std::string difficulty;   // 空 = 全部
    std::string keyword;      // 标题模糊搜索
    int tag_id = 0;           // 0 = 全部
};

class Problem {
public:
    Problem();
    Problem(int id, const std::string& title, const std::string& difficulty,
            const std::string& content, const std::string& template_code,
            const std::string& created_at);

    int getId() const { return id_; }
    void setId(int id) { id_ = id; }

    const std::string& getTitle() const { return title_; }
    void setTitle(const std::string& title) { title_ = title; }

    const std::string& getDifficulty() const { return difficulty_; }
    void setDifficulty(const std::string& diff) { difficulty_ = diff; }

    const std::string& getContent() const { return content_; }
    void setContent(const std::string& content) { content_ = content; }

    const std::string& getTemplate() const { return template_; }
    void setTemplate(const std::string& t) { template_ = t; }

    const std::string& getSpjCode() const { return spj_code_; }
    void setSpjCode(const std::string& s) { spj_code_ = s; }
    bool hasSpj() const { return !spj_code_.empty(); }

    // 参考答案（学生做错后展示）
    const std::string& getSolution() const { return solution_; }
    void setSolution(const std::string& s) { solution_ = s; }

    const std::string& getCreatedAt() const { return created_at_; }
    void setCreatedAt(const std::string& t) { created_at_ = t; }

    // 标签（name 列表；saveTagsToDB 按 name 建标签并重建关联）
    const std::vector<std::string>& getTags() const { return tags_; }
    void setTags(const std::vector<std::string>& tags) { tags_ = tags; }
    void loadTagsFromDB();
    bool saveTagsToDB();

    bool saveToDB();
    bool updateToDB();     // 更新题目本体字段（不含用例/标签）
    bool loadFromDB(int id);
    bool deleteFromDB();

    std::vector<TestCase> getTestCases() const;

    static std::vector<Problem> getAll();
    static std::vector<Problem> getByDifficulty(const std::string& difficulty);
    // 分页 + 筛选搜索；total 返回符合条件总数
    static std::vector<Problem> search(const ProblemQuery& query, int& total);

private:
    int id_;
    std::string title_;
    std::string difficulty_;
    std::string content_;
    std::string template_;
    std::string spj_code_;
    std::string solution_;
    std::string created_at_;
    std::vector<std::string> tags_;
};

} // namespace oj
