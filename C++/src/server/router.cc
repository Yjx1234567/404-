#include "server/router.h"
#include "server/http_policy.h"
#include "handler/problem_handler.h"
#include "handler/auth_handler.h"
#include "handler/admin_handler.h"
#include "handler/submission_handler.h"
#include "handler/class_handler.h"
#include "service/auth_service.h"
#include "utils/config.h"
#include "utils/logger.h"
#include <json/json.h>
#include <sys/stat.h>

namespace oj {

// httplib 的 query params（multimap）转 std::map（同 key 取第一个）
static std::map<std::string, std::string> toQueryMap(const httplib::Request& req) {
    std::map<std::string, std::string> m;
    for (const auto& kv : req.params) {
        if (m.find(kv.first) == m.end()) m[kv.first] = kv.second;
    }
    return m;
}

// 解析 :id 路径参数；非法或非正数返回 false
static bool pathId(const httplib::Request& req, int& id) {
    try {
        id = std::stoi(req.path_params.at("id"));
        return id > 0;
    } catch (...) {
        return false;
    }
}

Router& Router::getInstance() {
    static Router instance;
    return instance;
}

std::string Router::extractSessionToken(const httplib::Request& req) {
    return http_policy::sessionToken(req);
}

bool Router::isLoggedIn(const httplib::Request& req) {
    std::string token = extractSessionToken(req);
    if (token.empty()) return false;
    auto user = AuthService::getUserBySession(token);
    return user.has_value();
}

bool Router::isAdmin(const httplib::Request& req) {
    std::string token = extractSessionToken(req);
    if (token.empty()) return false;
    return AuthService::isAdmin(token);
}

void Router::setJsonResponse(std::string& res, const std::string& body, int status) {
    // This is used by lambda captures
}

void Router::setupRoutes(httplib::Server& server) {
    // 静态资源目录：优先读配置，依次尝试「配置目录 → ./public → ../public」，
    // 覆盖从项目根目录或 build/ 目录启动两种场景
    // （修复原版硬编码 "public" 导致从 build 目录启动时前端 404 的问题）
    std::string public_dir;
    struct stat dir_stat;
    std::string configured = ConfigManager::getInstance().getString("server.public_dir", "public");
    for (const std::string& candidate : {configured, std::string("public"), std::string("../public")}) {
        if (!candidate.empty() && stat(candidate.c_str(), &dir_stat) == 0 && S_ISDIR(dir_stat.st_mode)) {
            public_dir = candidate;
            break;
        }
    }
    if (public_dir.empty()) {
        Logger::getInstance().warning("Static public dir not found, static files will 404 (run from project root)");
        public_dir = "public";
    }
    server.set_base_dir(public_dir);
    server.set_post_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        http_policy::applyCachePolicy(req, res);
    });

    // 异常防护包装器：任何 handler 抛异常（如 DB 连接超时）都返回 500，
    // 避免异常穿透到 httplib 工作线程导致整个服务进程崩溃
    auto guard = [](auto&& fn, httplib::Response& res) {
        try {
            fn();
        } catch (const std::exception& e) {
            Logger::getInstance().error(std::string("Handler exception: ") + e.what());
            res.status = 500;
            res.set_content("{\"error\":\"internal server error\"}", "application/json");
        } catch (...) {
            Logger::getInstance().error("Handler exception: unknown");
            res.status = 500;
            res.set_content("{\"error\":\"internal server error\"}", "application/json");
        }
    };

    // 当前登录用户（未登录返回 id=0）
    auto getUser = [this](const httplib::Request& req) -> User {
        std::string token = extractSessionToken(req);
        auto user = AuthService::getUserBySession(token);
        if (user.has_value()) return *user;
        User empty;
        return empty;
    };

    // Problem list (requires login)
    server.Get("/api/problems", [this, guard, getUser](const httplib::Request& req, httplib::Response& res) {
        guard([&]() {
            if (!isLoggedIn(req)) {
                res.status = 401;
                res.set_content("{\"error\":\"unauthorized\"}", "application/json");
                return;
            }
            auto [status, body] = handleProblemList(toQueryMap(req));
            res.status = status;
            res.set_content(body, "application/json");
        }, res);
    });

    // Problem detail (requires login)
    server.Get("/api/problems/:id", [this, guard](const httplib::Request& req, httplib::Response& res) {
        guard([&]() {
            if (!isLoggedIn(req)) {
                res.status = 401;
                res.set_content("{\"error\":\"unauthorized\"}", "application/json");
                return;
            }
            int id = 0;
            if (!pathId(req, id)) {
                res.status = 400;
                res.set_content("{\"error\":\"invalid problem id\"}", "application/json");
                return;
            }
            auto [status, body] = handleProblemDetail(id);
            res.status = status;
            res.set_content(body, "application/json");
        }, res);
    });

    // Tags list (requires login)
    server.Get("/api/tags", [this, guard](const httplib::Request& req, httplib::Response& res) {
        guard([&]() {
            if (!isLoggedIn(req)) {
                res.status = 401;
                res.set_content("{\"error\":\"unauthorized\"}", "application/json");
                return;
            }
            auto [status, body] = handleTagsList();
            res.status = status;
            res.set_content(body, "application/json");
        }, res);
    });

    // Submit code（异步判题：落库 + 入队后立即返回）
    server.Post("/api/submit", [this, guard, getUser](const httplib::Request& req, httplib::Response& res) {
        guard([&]() {
            User user = getUser(req);
            if (user.getId() == 0) {
                res.status = 401;
                res.set_content("{\"error\":\"unauthorized\"}", "application/json");
                return;
            }
            auto [status, body] = handleSubmit(req.body, user.getId());
            res.status = status;
            res.set_content(body, "application/json");
        }, res);
    });

    // My submissions list
    server.Get("/api/submissions", [this, guard, getUser](const httplib::Request& req, httplib::Response& res) {
        guard([&]() {
            User user = getUser(req);
            if (user.getId() == 0) {
                res.status = 401;
                res.set_content("{\"error\":\"unauthorized\"}", "application/json");
                return;
            }
            bool admin = (user.getRole() == "admin");
            auto [status, body] = handleSubmissionList(toQueryMap(req), user.getId(), admin);
            res.status = status;
            res.set_content(body, "application/json");
        }, res);
    });

    // Submission detail
    server.Get("/api/submissions/:id", [this, guard, getUser](const httplib::Request& req, httplib::Response& res) {
        guard([&]() {
            User user = getUser(req);
            if (user.getId() == 0) {
                res.status = 401;
                res.set_content("{\"error\":\"unauthorized\"}", "application/json");
                return;
            }
            int id = 0;
            if (!pathId(req, id)) {
                res.status = 400;
                res.set_content("{\"error\":\"invalid submission id\"}", "application/json");
                return;
            }
            bool admin = (user.getRole() == "admin");
            auto [status, body] = handleSubmissionDetail(id, user.getId(), admin);
            res.status = status;
            res.set_content(body, "application/json");
        }, res);
    });

    // Personal stats
    server.Get("/api/users/me/stats", [this, guard, getUser](const httplib::Request& req, httplib::Response& res) {
        guard([&]() {
            User user = getUser(req);
            if (user.getId() == 0) {
                res.status = 401;
                res.set_content("{\"error\":\"unauthorized\"}", "application/json");
                return;
            }
            auto [status, body] = handleUserStats(user.getId());
            res.status = status;
            res.set_content(body, "application/json");
        }, res);
    });

    // Leaderboard
    server.Get("/api/leaderboard", [this, guard](const httplib::Request& req, httplib::Response& res) {
        guard([&]() {
            if (!isLoggedIn(req)) {
                res.status = 401;
                res.set_content("{\"error\":\"unauthorized\"}", "application/json");
                return;
            }
            auto [status, body] = handleLeaderboard();
            res.status = status;
            res.set_content(body, "application/json");
        }, res);
    });

    // Register
    server.Post("/api/register", [guard](const httplib::Request& req, httplib::Response& res) {
        guard([&]() {
            auto [status, body] = handleRegister(req.body);
            res.status = status;
            res.set_content(body, "application/json");
        }, res);
    });

    // Login
    server.Post("/api/login", [guard](const httplib::Request& req, httplib::Response& res) {
        guard([&]() {
            auto [status, body] = handleLogin(req.body);
            Json::Value root;
            Json::Reader reader;
            if (status == 200 && reader.parse(body, root) && root["session_id"].isString()) {
                http_policy::setSessionCookie(res, root["session_id"].asString());
            }
            res.status = status;
            res.set_content(body, "application/json");
        }, res);
    });

    // Logout
    server.Post("/api/logout", [this, guard](const httplib::Request& req, httplib::Response& res) {
        guard([&]() {
            std::string token = extractSessionToken(req);
            auto [status, body] = handleLogout(token);
            res.status = status;
            res.set_content(body, "application/json");
        }, res);
        // Clear the browser cookie even if the session is missing, expired or
        // its deletion failed; the existing HTTP status/body remain intact.
        http_policy::expireSessionCookie(res);
    });

    // ===== 班级/作业 =====

    server.Post("/api/classes", [guard, getUser](const httplib::Request& req, httplib::Response& res) {
        guard([&]() {
            User user = getUser(req);
            if (user.getId() == 0) {
                res.status = 401;
                res.set_content("{\"error\":\"unauthorized\"}", "application/json");
                return;
            }
            auto [status, body] = handleCreateClass(req.body, user.getId());
            res.status = status;
            res.set_content(body, "application/json");
        }, res);
    });

    server.Post("/api/classes/join", [guard, getUser](const httplib::Request& req, httplib::Response& res) {
        guard([&]() {
            User user = getUser(req);
            if (user.getId() == 0) {
                res.status = 401;
                res.set_content("{\"error\":\"unauthorized\"}", "application/json");
                return;
            }
            auto [status, body] = handleJoinClass(req.body, user.getId());
            res.status = status;
            res.set_content(body, "application/json");
        }, res);
    });

    server.Get("/api/classes/mine", [guard, getUser](const httplib::Request& req, httplib::Response& res) {
        guard([&]() {
            User user = getUser(req);
            if (user.getId() == 0) {
                res.status = 401;
                res.set_content("{\"error\":\"unauthorized\"}", "application/json");
                return;
            }
            auto [status, body] = handleMyClasses(user.getId());
            res.status = status;
            res.set_content(body, "application/json");
        }, res);
    });

    server.Get("/api/classes/:id", [guard, getUser](const httplib::Request& req, httplib::Response& res) {
        guard([&]() {
            User user = getUser(req);
            if (user.getId() == 0) {
                res.status = 401;
                res.set_content("{\"error\":\"unauthorized\"}", "application/json");
                return;
            }
            int id = 0;
            if (!pathId(req, id)) {
                res.status = 400;
                res.set_content("{\"error\":\"invalid class id\"}", "application/json");
                return;
            }
            bool admin = (user.getRole() == "admin");
            auto [status, body] = handleClassDetail(id, user.getId(), admin);
            res.status = status;
            res.set_content(body, "application/json");
        }, res);
    });

    server.Delete("/api/classes/:id", [guard, getUser](const httplib::Request& req, httplib::Response& res) {
        guard([&]() {
            User user = getUser(req);
            if (user.getId() == 0) {
                res.status = 401;
                res.set_content("{\"error\":\"unauthorized\"}", "application/json");
                return;
            }
            int id = 0;
            if (!pathId(req, id)) {
                res.status = 400;
                res.set_content("{\"error\":\"invalid class id\"}", "application/json");
                return;
            }
            bool admin = (user.getRole() == "admin");
            auto [status, body] = handleDeleteClass(id, user.getId(), admin);
            res.status = status;
            res.set_content(body, "application/json");
        }, res);
    });

    server.Post("/api/classes/:id/assignments", [guard, getUser](const httplib::Request& req, httplib::Response& res) {
        guard([&]() {
            User user = getUser(req);
            if (user.getId() == 0) {
                res.status = 401;
                res.set_content("{\"error\":\"unauthorized\"}", "application/json");
                return;
            }
            int id = 0;
            if (!pathId(req, id)) {
                res.status = 400;
                res.set_content("{\"error\":\"invalid class id\"}", "application/json");
                return;
            }
            bool admin = (user.getRole() == "admin");
            auto [status, body] = handleCreateAssignment(id, req.body, user.getId(), admin);
            res.status = status;
            res.set_content(body, "application/json");
        }, res);
    });

    server.Get("/api/classes/:id/assignments", [guard, getUser](const httplib::Request& req, httplib::Response& res) {
        guard([&]() {
            User user = getUser(req);
            if (user.getId() == 0) {
                res.status = 401;
                res.set_content("{\"error\":\"unauthorized\"}", "application/json");
                return;
            }
            int id = 0;
            if (!pathId(req, id)) {
                res.status = 400;
                res.set_content("{\"error\":\"invalid class id\"}", "application/json");
                return;
            }
            bool admin = (user.getRole() == "admin");
            auto [status, body] = handleListAssignments(id, user.getId());
            (void)admin;
            res.status = status;
            res.set_content(body, "application/json");
        }, res);
    });

    server.Get("/api/assignments/:id", [guard, getUser](const httplib::Request& req, httplib::Response& res) {
        guard([&]() {
            User user = getUser(req);
            if (user.getId() == 0) {
                res.status = 401;
                res.set_content("{\"error\":\"unauthorized\"}", "application/json");
                return;
            }
            int id = 0;
            if (!pathId(req, id)) {
                res.status = 400;
                res.set_content("{\"error\":\"invalid assignment id\"}", "application/json");
                return;
            }
            bool admin = (user.getRole() == "admin");
            auto [status, body] = handleAssignmentDetail(id, user.getId(), admin);
            res.status = status;
            res.set_content(body, "application/json");
        }, res);
    });

    // ===== Admin =====

    // Admin: Create problem
    server.Post("/api/admin/problems", [this, guard](const httplib::Request& req, httplib::Response& res) {
        guard([&]() {
            if (!isAdmin(req)) {
                res.status = 403;
                res.set_content("{\"error\":\"forbidden\"}", "application/json");
                return;
            }
            auto [status, body] = handleCreateProblem(req.body);
            res.status = status;
            res.set_content(body, "application/json");
        }, res);
    });

    // Admin: Problem detail (含 spj_code/tags，编辑表单用)
    server.Get("/api/admin/problems/:id", [this, guard](const httplib::Request& req, httplib::Response& res) {
        guard([&]() {
            if (!isAdmin(req)) {
                res.status = 403;
                res.set_content("{\"error\":\"forbidden\"}", "application/json");
                return;
            }
            int id = 0;
            if (!pathId(req, id)) {
                res.status = 400;
                res.set_content("{\"error\":\"invalid problem id\"}", "application/json");
                return;
            }
            auto [status, body] = handleGetProblemAdmin(id);
            res.status = status;
            res.set_content(body, "application/json");
        }, res);
    });

    // Admin: Update problem
    server.Put("/api/admin/problems/:id", [this, guard](const httplib::Request& req, httplib::Response& res) {
        guard([&]() {
            if (!isAdmin(req)) {
                res.status = 403;
                res.set_content("{\"error\":\"forbidden\"}", "application/json");
                return;
            }
            int id = 0;
            if (!pathId(req, id)) {
                res.status = 400;
                res.set_content("{\"error\":\"invalid problem id\"}", "application/json");
                return;
            }
            auto [status, body] = handleUpdateProblem(id, req.body);
            res.status = status;
            res.set_content(body, "application/json");
        }, res);
    });

    // Admin: Delete problem
    server.Delete("/api/admin/problems/:id", [this, guard](const httplib::Request& req, httplib::Response& res) {
        guard([&]() {
            if (!isAdmin(req)) {
                res.status = 403;
                res.set_content("{\"error\":\"forbidden\"}", "application/json");
                return;
            }
            int id = 0;
            if (!pathId(req, id)) {
                res.status = 400;
                res.set_content("{\"error\":\"invalid problem id\"}", "application/json");
                return;
            }
            auto [status, body] = handleDeleteProblem(id);
            res.status = status;
            res.set_content(body, "application/json");
        }, res);
    });
}

} // namespace oj
