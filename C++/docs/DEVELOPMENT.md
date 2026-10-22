# 开发指南（DEVELOPMENT）

> 本文档面向**二次开发者**（也是写给未来的自己）：讲清楚系统架构、一次提交的完整生命周期、每个目录的职责、如何新增功能、以及前人踩过的坑。
> 配套阅读：[PROGRESS.md](PROGRESS.md)（当前进度与待办路线图）、[../API.md](../API.md)（接口文档）、[../README.md](../README.md)（部署手册）。

---

## 1. 技术栈与总体架构

| 层级 | 技术选型 | 说明 |
|------|----------|------|
| 后端语言 | C++17 | `set(CMAKE_CXX_STANDARD 17)` |
| HTTP 框架 | cpp-httplib（header-only，`third_party/`） | 多线程模式，`thread_pool_size` 可配 |
| 数据库 | MySQL 8.0（mysqlclient C API） | 12 张表，InnoDB，utf8mb4 |
| 密码哈希 | Botan-2 的 bcrypt | `generate_bcrypt` / `check_bcrypt`，**Botan-3 已删除此 API，只能用 Botan-2** |
| 配置/JSON | yaml-cpp / jsoncpp | |
| 前端 | 原生 HTML/CSS/JS + Ace Editor（已本地化到 `public/vendor/ace/`） | 无构建步骤，改完刷新即生效 |
| 构建 | CMake ≥3.10 | 单一 `CMakeLists.txt` |
| 测试 | GTest（6 个单测套件）+ Python 冒烟脚本 | |

### 分层架构（自上而下）

```
浏览器 (public/*.html + js/*.js)
   │  fetch /api/*（Cookie 携带 session_id）
   ▼
server/router.cc ── 所有路由注册 + 登录态/管理员校验 + guard 异常防护
   │
   ▼
handler/ ── 请求体 JSON 解析、参数校验、组装响应 JSON（不含业务细节）
   │
   ▼
service/ ── 业务逻辑：判题队列、沙箱执行、认证、题库查询、限流
   │
   ▼
model/ ── Active Record 风格数据模型（每个类自带 saveToDB/getByXxx 等 SQL 方法）
   │
   ▼
db/connection_pool ── 单例连接池 + escapeSql() 防 SQL 注入
   ▼
MySQL (oj_system)
```

**分层约定**：router 只做"路由 + 权限 + 调 handler"；handler 只做"解析 + 校验 + 调 service/model + 拼响应"；SQL 全部收在 model 层；跨 model 的业务逻辑放 service 层。新增代码请遵守这个分层，否则会退化成面条代码。

---

## 2. 目录结构详解

```
├── src/
│   ├── main.cc                  # 入口：配置→日志→连接池→判题队列→HTTP 服务；SIGINT/SIGTERM 优雅退出
│   ├── server/
│   │   ├── server.{h,cc}        # httplib::Server 单例封装（start/stop）
│   │   └── router.{h,cc}        # ★ 所有 API 路由注册（约 20 个端点）
│   ├── handler/                 # 一个业务域一个文件
│   │   ├── auth_handler         #   注册/登录/登出
│   │   ├── problem_handler      #   题目列表/详情/标签/个人统计/排行榜
│   │   ├── submission_handler   #   提交/提交列表/提交详情
│   │   ├── admin_handler        #   管理员：题目增删改查（含 SPJ/标签/用例整体替换）
│   │   └── class_handler        #   班级 CRUD + 加入码 + 作业 + 完成矩阵（最大的 handler，562 行）
│   ├── service/
│   │   ├── executor_service     # ★ 判题核心：编译、沙箱运行、逐用例比对、SPJ 特判（674 行）
│   │   ├── judge_queue          # ★ 异步判题队列（生产者-消费者，2 worker，重启自动恢复 PENDING）
│   │   ├── auth_service         #   会话校验、密码校验、token 生成
│   │   ├── problem_service      #   题库查询（分页/难度/关键词/标签）
│   │   └── rate_limiter         #   每用户每分钟提交次数限制（429）
│   ├── model/                   # problem / test_case / submission / user / session
│   ├── db/connection_pool       # 连接池（10 连接，取连接 30s 超时）+ escapeSql()
│   └── utils/                   # config(yaml) / logger / bcrypt(Botan-2)
├── public/                      # 前端（无构建，直接静态服务）
│   ├── index/login/register/problem_list/problem/submissions/profile/leaderboard/admin.html
│   ├── js/api.js                #   ★ 统一 fetch 封装（所有页面都 import 它）
│   └── vendor/ace/              #   Ace 编辑器本地化，无 CDN 依赖
├── database/init.sql            # 建库建表（12 表，幂等 IF NOT EXISTS）
├── config/config.yaml           # 运行配置（端口/DB/判题限制/限流/降权用户等）
├── tests/
│   ├── unit/                    # 6 个 GTest 套件（已接入 CMake）
│   ├── integration/api_test.cc  # ⚠️ 未接入 CMake，内容已过期（引用了已删除的 submit_handler.cc），建议删除或重写
│   └── python/                  # smoke_test.sh 端到端冒烟 + seed_problems.sh 造数 + check_filter.sh
├── tools/                       # create_admin_user（建管理员）、db_reset_tool（重置题库）
├── third_party/                 # cpp-httplib 头文件
├── setup.sh                     # 完整部署（WSL 内跑：换源→装依赖→同步→建库→编译→起服务）
├── dev_build.sh                 # ★ 日常开发用：同步源码→增量编译→(可选)跑测试
├── fix_sources.sh               # apt 换清华源（兼容新旧 sources 格式）
└── 0/1/2/3-*.bat                # Windows 双击部署四件套
```

---

## 3. 一次代码提交的完整生命周期（核心流程）

理解这条链路 = 理解整个系统：

```
1. problem_detail.js：编辑器取代码 → POST /api/submit {code, problem_id}
2. router.cc:166     ：guard 包裹 → getUser() 校验登录（未登录 401）
3. submission_handler：校验参数（code ≤64KB、题目存在）→ rate_limiter 检查（超限 429）
4. submission_handler：Submission::saveToDB() 落库 status=PENDING
                       → JudgeQueue::enqueue() → 立即返回 {submission_id, "PENDING"}
5. 前端开始轮询 GET /api/submissions/:id（每 1~2 秒）
6. judge_queue.cc worker 线程：取出任务 → 调 ExecutorService::execute()
7. executor_service.cc：
   a. 写源码到 /tmp/oj_temp/xxx_{pid}_{原子序号}.cpp（防并发撞名）
   b. compile()：std::system 调 g++，stderr 重定向到文件，15s 编译超时 → 失败即 CE
   c. 若有 spj_code：按源码哈希缓存编译 SPJ 可执行文件
   d. 逐用例 runTestCase()：
      fork() → 子进程 applySandbox()（见下）→ 重定向 stdin/stdout 到临时文件
      → exec 用户程序 → 父进程 wait4 计时（超 timeout 杀进程 → TLE）
      → 读输出文件与 expected 逐行比对（或跑 SPJ）→ AC/WA/RE/MLE
   e. 汇总为 JudgeResult{status, passed/total, details[]}
8. worker 把结果写回 submissions 表（status/passed/total/time_ms/result JSON/error_info）
9. 前端轮询到终态 → 渲染结果面板
```

**服务重启恢复**：`JudgeQueue::start()` 会扫描 submissions 表中遗留的 PENDING/JUDGING 记录重新入队，不会丢提交。

### 沙箱安全机制（executor_service.cc applySandbox，约 320 行起）

用户代码是不可信输入，子进程在 exec 前做了：

| 措施 | 目的 |
|------|------|
| `unshare(CLONE_NEWNET)` | 禁止联网 |
| `setuid(nobody)`（仅 root 运行生效，配置 `judge_user`） | 降权，避免用户代码以服务权限运行 |
| `RLIMIT_CPU = timeout` | CPU 时间硬限制 → TLE |
| `RLIMIT_AS = max_memory_mb` | 地址空间限制 → MLE |
| `RLIMIT_STACK / FSIZE / NOFILE / CORE=0` | 栈/输出文件/句柄数/无 core |
| `RLIMIT_NPROC = 64` | 限进程数（注意：不能设 1，否则用户代码用 std::thread 直接 RE） |
| 临时文件名 `xxx_{pid}_{原子序号}` | 并发安全 |

> 局限性（如实说明）：未做 chroot/seccomp，文件系统可读；生产级 OJ 需补 seccomp 白名单。毕设场景够用，论文里可作为"安全性讨论"一章。

---

## 4. 数据库 Schema（12 表）

```
users(id, username, password[bcrypt], role[user|admin], created_at)
sessions(id, user_id→users, token, expires_at)               # 会话，登录后 7 天有效
problems(id, title, difficulty[Easy|Medium|Hard], content, template, spj_code, created_at)
test_cases(id, problem_id→problems, input, expected, position)
tags(id, name) + problem_tags(problem_id, tag_id)            # 多对多
submissions(id, user_id, problem_id, code, status[PENDING|JUDGING|AC|WA|TLE|MLE|RE|CE|SE],
            passed, total, time_ms, error_info, result[JSON], created_at)
classes(id, name, join_code, teacher_id→users)
class_members(class_id, user_id, joined_at)
assignments(id, class_id→classes, title, description, due_at)
assignment_problems(assignment_id, problem_id, position)
```

所有外键 `ON DELETE CASCADE`，删题目/班级自动清理关联数据。改表直接改 `database/init.sql` 后重跑（开发库可用 `db_reset_tool` 重置，**会清掉全部题目和会话**，谨慎用）。

---

## 5. 开发环境与日常工作流

### 环境约束（重要，别绕路）

- **只能在 WSL Ubuntu 22.04 编译运行**（fork/unistd.h + Botan-2 依赖）。Windows 本机 MinGW/MSVC 编不过。
- **必须 Ubuntu 22.04**：24.04 软件源没有 libbotan-2-dev（只有 3.x，API 不兼容）。
- **不能直接在 /mnt/d 上编译**：drvfs 挂载点无执行权限，CMake 报 `Operation not permitted`。所以构建目录固定在 WSL 本地盘 `~/oj_platform`。

### 日常改代码循环

```
1. Windows 侧用任意编辑器改 D:\...\OJ 平台\ 下的源码（这是"真正的"源码）
2. WSL 里：cd /mnt/d/projects/cpp-oj-platform && bash dev_build.sh --test
   （自动：同步源码到 ~/oj_platform → 增量 make → ctest）
3. 重启服务（必须从项目根目录跑，否则找不到 config/ 和 public/）：
   cd ~/oj_platform && ./build/oj_server        # 前台跑，Ctrl+C 停
4. 浏览器 http://localhost:8080 验证
5. 想跑端到端冒烟：bash tests/python/smoke_test.sh（会先重置数据库，见脚本头部）
```

改**前端**（public/）不需要编译：dev_build.sh 同步后刷新浏览器即可（httplib 对静态文件无缓存策略）。

### 数据库操作速查

```bash
sudo service mysql start                                    # WSL 每次开机后
mysql oj_system -e "SELECT id,username,role FROM users;"    # 直查
mysql oj_system < database/init.sql                          # 幂等补建表
./build/create_admin_user                                    # 重建 admin/admin123
```

MySQL 用户用的是 Linux 用户名 + auth_socket 认证（配置里 username: "your_username"，密码留空）。

---

## 6. 如何新增一个 API 接口（Step by Step）

以"收藏题目"为例，改动点固定是这 5 处：

1. **`database/init.sql`**：加表 `favorites(user_id, problem_id)`（改完重跑 init.sql）
2. **`src/model/`**：新建 `favorite.h/cc`（参考 `test_case.h/cc`，最简单的 model 长这样）：
   - 成员变量 + getter/setter
   - `static Favorite getByUserAndProblem(...)` / `bool saveToDB()` / `bool deleteFromDB()`
   - **所有 SQL 字符串拼接必须过 `escapeSql()`**（connection_pool.h 提供）
3. **`src/handler/`**：在对应 handler 加 `std::pair<int, std::string> handleAddFavorite(const std::string& body, int user_id)`
   - 返回 `{http_status, json_body}`，这是全项目的 handler 统一签名
   - jsoncpp 解析请求体：`Json::Reader().parse(body, root)`
4. **`src/server/router.cc`**：照抄现有路由模板：
   ```cpp
   server.Post("/api/favorites", [this, guard, getUser](const httplib::Request& req, httplib::Response& res) {
       guard([&]() {
           User user = getUser(req);
           if (user.getId() == 0) { res.status = 401; res.set_content("{\"error\":\"unauthorized\"}", "application/json"); return; }
           auto [status, body] = handleAddFavorite(req.body, user.getId());
           res.status = status;
           res.set_content(body, "application/json");
       }, res);
   });
   ```
5. **`CMakeLists.txt`**：`set(SOURCES ...)` 里加 `src/model/favorite.cc`（**新文件必加，忘加就是链接错误 undefined reference**）

前端：`public/js/api.js` 加 fetch 封装 → 页面 js 调用 → 需要新页面就复制一个现有 html 改。

**注意 guard 的坑**：lambda 一律**按值捕获** `[this, guard, getUser]`。httplib 路由的生命周期长于 setupRoutes 栈帧，按引用捕获局部变量 = 悬垂引用（历史上踩过）。

---

## 7. 测试体系

| 层级 | 位置 | 跑法 | 覆盖 |
|------|------|------|------|
| 单元 | tests/unit/*.cc | `ctest --output-on-failure` | config 解析 / logger / 连接池 / problem CRUD / 判题执行器 / 注册登录 |
| 端到端 | tests/python/smoke_test.sh | 服务启动后跑 | 认证、六种判题结果、统计、排行榜、标签、班级作业、限流，约 30 断言 |
| 造数 | tests/python/seed_problems.sh | 手动 | 批量造练习题 |

单测里需要连 DB 的（problem/executor/auth/connection_pool）依赖本地 MySQL 已启动。冒烟测试**会重置数据库**，跑之前想清楚。

---

## 8. 历史踩坑记录（改动相关代码前必读）

这些坑都已经修好，但**重新引入的成本极高**，改到相关区域时保持警惕：

1. **SQL 注入**：曾经全部 model 用裸字符串拼 SQL。现在所有用户输入进 SQL 前必须 `escapeSql()`（新增 model 时最容易犯）。
2. **判题管道死锁**：runTestCase 曾用 pipe 收 stdout，用户程序输出 >64KB 时 write 阻塞 → 整个判题线程卡死。现改为**临时文件重定向** stdin/stdout/stderr。别改回 pipe。
3. **RLIMIT_NPROC=1**：曾把用户进程数限到 1，导致用户代码里 `std::thread` 直接 RE。现放宽到 64。
4. **popen 读不到编译错误**：compile() 曾用 popen，stderr 混不进管道且 pclose 返回值判断错误。现用 `std::system` + stderr 重定向到文件。
5. **handler 异常打崩服务**：DB 超时抛异常会穿透 httplib 工作线程。现在所有路由都套 `guard`（见 router.cc:94）。
6. **Botan 版本**：代码用的 `generate_bcrypt/check_bcrypt` 是 Botan-2 API，3.x 已删除。升级 Botan = 重写 bcrypt.cc。
7. **drvfs noexec**：见第 5 节，任何 CMake 操作都别在 /mnt/d 下做。
8. **bat 编码**：Windows bat 里写中文必乱码（cmd 按 GBK 逐行切 UTF-8 字节）。bat 保持纯 ASCII，中文路径用 bash glob 运行时展开。
9. **cp 同步嵌套**：同步源码用 `cp -rf src/./ dest/` 而非 `cp -rf src dest/`，后者重复运行会嵌套成 `dest/src/src`。
10. **Ace CDN**：编辑器曾依赖 CDN，网络差就白屏。已本地化到 public/vendor/ace/，并有 textarea 降级。

---

## 9. 配置项速查（config/config.yaml）

| 键 | 默认 | 说明 |
|----|------|------|
| `server.host/port` | 0.0.0.0 / 8080 | |
| `server.thread_pool_size` | 4 | httplib 工作线程 |
| `server.public_dir` | ./public | 找不到时自动回退 ./public → ../public |
| `database.username/password` | your_username / 空 | auth_socket 认证，换 Linux 用户要改 |
| `execution.timeout_seconds` | 5 | 单用例 CPU 上限 |
| `execution.max_memory_mb` | 256 | RLIMIT_AS |
| `execution.compile_command` | g++ -std=c++17 -O2 | |
| `execution.compile_timeout_seconds` | 15 | 防编译炸弹 |
| `execution.judge_workers` | 2 | 判题并发数 |
| `execution.rate_limit_per_minute` | 10 | 每用户每分钟提交上限，0=不限 |
| `execution.judge_user` | nobody | 降权目标（root 运行时生效） |
| `logging.*` | INFO / server.log / true | |
