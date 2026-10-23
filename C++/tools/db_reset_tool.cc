#include "model/problem.h"
#include "model/test_case.h"
#include "model/user.h"
#include "db/connection_pool.h"
#include "utils/config.h"
#include "utils/logger.h"
#include <iostream>
#include <vector>
#include <tuple>

using namespace oj;

int main(int argc, char* argv[]) {
    std::string config_path = (argc > 1) ? argv[1] : "config/config.yaml";
    
    if (!ConfigManager::getInstance().loadFromFile(config_path)) {
        std::cerr << "Failed to load config from " << config_path << std::endl;
        return 1;
    }
    
    Logger::getInstance().setLevel(LogLevel::INFO);
    Logger::getInstance().setConsole(true);
    
    std::string db_host = ConfigManager::getInstance().getString("database.host", "localhost");
    int db_port = ConfigManager::getInstance().getInt("database.port", 3306);
    std::string db_user = ConfigManager::getInstance().getString("database.username", "root");
    std::string db_pass = ConfigManager::getInstance().getString("database.password", "");
    std::string db_name = ConfigManager::getInstance().getString("database.database", "oj_system");
    int max_conn = ConfigManager::getInstance().getInt("database.max_connections", 10);
    
    if (!ConnectionPool::getInstance().init(db_host, db_port, db_user, db_pass, db_name, max_conn)) {
        std::cerr << "Failed to connect to database" << std::endl;
        return 1;
    }
    
    std::cout << "WARNING: This will delete ALL existing problems and test cases!" << std::endl;
    std::cout << "Are you sure? (yes/no): ";
    std::string confirm;
    std::cin >> confirm;
    if (confirm != "yes") {
        std::cout << "Cancelled." << std::endl;
        return 0;
    }
    
    // Delete all problems (cascade deletes test cases)
    auto conn = ConnectionPool::getInstance().getConnection();
    if (conn) {
        mysql_query(conn, "DELETE FROM problems");
        mysql_query(conn, "DELETE FROM sessions");
        ConnectionPool::getInstance().releaseConnection(conn);
    }
    
    // Create test problems
    std::vector<std::tuple<std::string, std::string, std::string, std::string, std::vector<std::pair<std::string, std::string>>>> problems = {
        {
            "两数相加",
            "Easy",
            "给定两个整数 a 和 b，返回它们的和。\n\n### 输入格式\n一行，包含两个整数 a 和 b\n\n### 输出格式\n一个整数，表示 a + b 的和",
            "#include <iostream>\nusing namespace std;\n\nint main() {\n    int a, b;\n    cin >> a >> b;\n    cout << a + b << endl;\n    return 0;\n}",
            {
                {"1 2", "3"},
                {"100 200", "300"},
                {"-5 10", "5"}
            }
        },
        {
            "判断奇偶",
            "Easy",
            "给定一个整数，判断它是奇数还是偶数。\n\n### 输入格式\n一个整数 n\n\n### 输出格式\n如果 n 是偶数，输出 `Even`；如果是奇数，输出 `Odd`",
            "#include <iostream>\nusing namespace std;\n\nint main() {\n    int n;\n    cin >> n;\n    if (n % 2 == 0) {\n        cout << \"Even\" << endl;\n    } else {\n        cout << \"Odd\" << endl;\n    }\n    return 0;\n}",
            {
                {"4", "Even"},
                {"7", "Odd"},
                {"0", "Even"}
            }
        },
        {
            "求最大值",
            "Medium",
            "给定三个整数，找出其中的最大值。\n\n### 输入格式\n一行，包含三个整数 a, b, c\n\n### 输出格式\n一个整数，表示三个数中的最大值",
            "#include <iostream>\nusing namespace std;\n\nint main() {\n    int a, b, c;\n    cin >> a >> b >> c;\n    int max = a;\n    if (b > max) max = b;\n    if (c > max) max = c;\n    cout << max << endl;\n    return 0;\n}",
            {
                {"1 2 3", "3"},
                {"-5 -2 -10", "-2"},
                {"7 7 7", "7"}
            }
        },
        {
            "斐波那契数列",
            "Hard",
            "求斐波那契数列的第 n 项。\n\n### 输入格式\n一个整数 n\n\n### 输出格式\n第 n 个斐波那契数\n\n### 说明\nF(1) = 1, F(2) = 1, F(n) = F(n-1) + F(n-2)",
            "#include <iostream>\nusing namespace std;\n\nint main() {\n    int n;\n    cin >> n;\n    if (n <= 2) {\n        cout << 1 << endl;\n        return 0;\n    }\n    long long a = 1, b = 1, c;\n    for (int i = 3; i <= n; i++) {\n        c = a + b;\n        a = b;\n        b = c;\n    }\n    cout << b << endl;\n    return 0;\n}",
            {
                {"1", "1"},
                {"5", "5"},
                {"10", "55"}
            }
        }
    };
    
    for (const auto& p : problems) {
        Problem problem;
        problem.setTitle(std::get<0>(p));
        problem.setDifficulty(std::get<1>(p));
        problem.setContent(std::get<2>(p));
        problem.setTemplate(std::get<3>(p));
        
        if (!problem.saveToDB()) {
            std::cerr << "Failed to create problem: " << std::get<0>(p) << std::endl;
            continue;
        }
        
        int position = 0;
        for (const auto& tc : std::get<4>(p)) {
            TestCase testcase;
            testcase.setProblemId(problem.getId());
            testcase.setInput(tc.first);
            testcase.setExpected(tc.second);
            testcase.setPosition(position++);
            testcase.saveToDB();
        }
        
        std::cout << "Created problem: " << problem.getId() << " - " << problem.getTitle() << std::endl;
    }
    
    std::cout << "Database reset complete." << std::endl;
    return 0;
}
