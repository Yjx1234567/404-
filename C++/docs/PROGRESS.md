# 项目进度（PROGRESS）

> **用途**：下次回来继续开发时，先读这一份就能接上进度。每次做完一批改动，更新一次"快照"和"已完成"。
> 最后更新：2026-10-22（前端和一键启动；后端历史测试时间见下方）

---

## 0. 2026-10-22 更新

- 已完成 9 页浅色样式升级；实际 WSL 服务现已同步新前端，在线 CSS/JS 摘要与 Windows 源码一致。
- 日常只需双击根目录 `启动OJ.bat`，自动启动数据库、同步文件、按需增量构建、校验并打开登录页；服务后台运行，可关闭启动窗口。`停止OJ.bat` 仅停止本项目服务。
- VS Code 可用默认构建任务“启动 CodeOJ”（Ctrl+Shift+B），或任务列表中的“停止 CodeOJ”。
- 修复本地缓存与会话失效时的登录循环；受保护 API 的 401 回登录页，网络/格式错误给出具体提示。
- 前端 46 项、启动引擎 19 项逻辑测试通过；数据库迁移模拟通过；真实 Windows/WSL 启动、重复复用、停止重启、登录题库查询通过。此次未运行浏览器或截图检查，也未重跑后端 GTest/判题端到端测试。
- 原配置哈希不变；100 道题、2 个账号、1 条提交、294 个用例保留。详见 `design/2026-10-22-one-click-start.md`。

## 1. 2026-09-03 后端历史快照（本次未重新验证这些测试）

| 项 | 状态 |
|----|------|
| 编译 | ✅ WSL Ubuntu-22.04，`~/oj_platform/build/`，增量编译无错误 |
| 单元测试 | ✅ ctest 6/6 通过（config/logger/connection_pool/problem/executor/auth） |
| 端到端 | ✅ smoke_test.sh 曾全绿（2026-09-03 凌晨日志：AC/WA/CE/TLE/MLE/RE 各状态均验证过）；数据库当天重置过一次 |
| 数据库 | 已重置为干净库，仅 admin 用户；演示题库需用 `db_reset_tool` 或 `seed_problems.sh` 重建 |
| 服务 | `./build/oj_server`（从 `~/oj_platform` 根目录启动），端口 8080，管理员 admin/admin123 |
| 源码一致性 | Windows（真实源码）↔ WSL（构建副本）已同步一致 |
| 毕设定性 | 项目主体已完成，处于**功能扩展 + 论文撰写**阶段 |

## 2. 已实现功能模块（12 个，均可写入开题报告）

| # | 模块 | 主要代码位置 | 备注 |
|---|------|--------------|------|
| 1 | 用户注册/登录/登出（bcrypt + Session/Cookie） | auth_handler / auth_service / model/user / model/session | 会话 7 天有效 |
| 2 | 题库管理：增删改查 + 难度 + 标签 + 关键词搜索 + 分页 | admin_handler / problem_handler / problem_service | 管理员可编辑，用例/标签整体替换 |
| 3 | 在线代码编辑器（Ace 本地化 + textarea 降级） | public/js/problem_detail.js, public/vendor/ace/ | |
| 4 | 异步判题队列（生产者-消费者，重启恢复） | service/judge_queue | 提交立即返回，前端轮询 |
| 5 | 判题沙箱（fork + rlimit 六项 + 禁网 + 降权） | service/executor_service 的 applySandbox | |
| 6 | 多结果判定：AC/WA/TLE/MLE/RE/CE/SE | executor_service | TLE 用 wait4 计时，MLE 用 RLIMIT_AS |
| 7 | Special Judge 特判（源码哈希缓存编译） | executor_service 的 prepareSpj/runSpj | 支持非唯一解题目 |
| 8 | 提交记录 + 逐用例详情查询（本人/管理员可见） | submission_handler | 含代码回显、error_info |
| 9 | 个人统计 + 排行榜 | problem_handler（stats/leaderboard） | 按难度统计、AC 数排名 |
| 10 | 班级 + 作业 + 成员完成矩阵 | class_handler（562 行） | 加入码进班，教师看全班进度 |
| 11 | 提交频率限制（防刷题） | service/rate_limiter | 默认 10 次/分钟，429 |
| 12 | 数据库连接池 + SQL 注入防护 | db/connection_pool | escapeSql 全覆盖 |

**关键技术点（≥5，写论文/开题用）**：①C++17 多线程（互斥锁+条件变量的判题队列）；②进程级沙箱（fork/exec+setrlimit+unshare+setuid）；③MySQL C API 连接池；④bcrypt 密码安全存储；⑤cpp-httplib 轻量 HTTP 服务；⑥RESTful API + 前后端分离；⑦CMake + GTest 工程化；⑧特判机制。

## 3. 待实现功能（按优先级排序，均为中等难度）

> 选取原则：贴合教学场景、答辩有亮点、工作量可控。完成后在本表打勾并移入第 2 节。

| # | 功能 | 设计要点 | 预估工作量 |
|---|------|----------|-----------|
| 1 | **比赛模式** | 新表 contests + contest_problems + contest_submissions；限时；比赛专用排行榜（AC 数+罚时）；管理员创建比赛页 | 3-5 天，最大亮点 |
| 2 | **ECharts 数据可视化** | 引入 echarts.min.js（本地化）；班级正确率柱状图、个人提交趋势折线图、难度分布饼图 | 1-2 天，出效果最快 |
| 3 | **代码相似度查重** | 作业详情页对全班提交做相似度检测（先简单的 token 序列 + 最长公共子串/LCS，或 SimHash）；超阈值标红 | 2-3 天，答辩常问 |
| 4 | **SSE 判题进度实时推送** | 替代前端轮询：httplib 支持 chunked 响应；判题完成后向等待的连接推送终态 | 1-2 天 |
| 5 | **AI 错误反馈（Vibe Coding 呼应点）** | 判为 WA/CE 时把代码+用例发给大模型 API，生成讲解；结果存表异步返回 | 1-2 天（调 API 而已） |
| 6 | 修改密码 + 管理员用户管理 | users 表更新接口 + admin 页用户列表（封禁/重置密码） | 1 天 |
| 7 | 题目批量导入导出 | JSON/CSV 格式，seed_problems.sh 已有雏形可搬进后端 | 1 天 |
| 8 | 错题收藏 / 讨论区 | 第 6 节 DEVELOPMENT.md 的示例即是收藏功能的模板 | 各 1 天 |

**建议实现顺序**：2（可视化，半天见效果）→ 6（补完整性）→ 1（比赛，核心亮点）→ 3（查重）→ 4/5（锦上添花）。

## 4. 已知问题与技术债

| # | 问题 | 位置 | 建议 |
|---|------|------|------|
| 1 | 死代码 `submit_handler.{h,cc}`（重构残留，未进 CMake，无引用） | **仅 WSL 侧** `~/oj_platform/src/handler/` | 删除；Windows 侧已无此文件 |
| 2 | `tests/integration/api_test.cc` 未接入 CMake，且内容过期（断言已删除的 submit_handler.cc 存在） | tests/integration/ | 删除或重写为真集成测试 |
| 3 | 判题结果靠前端轮询（1-2s 一次） | problem_detail.js | 由待办 #4（SSE）解决 |
| 4 | 无修改密码、无用户管理界面 | admin.html 只有题目管理 | 待办 #6 |
| 5 | 沙箱未做 chroot/seccomp，用户代码可读文件系统 | executor_service | 毕设可不修，论文里如实讨论即可 |
| 6 | server.log 无轮转，长期运行会膨胀 | utils/logger | 低优先级，加个简单按大小截断 |
| 7 | sessions 过期记录不主动清理 | model/session | 低优先级，登录时顺手清或定时清 |
| 8 | `tests/python/check_filter.sh`、`seed_problems.sh` 刚从 WSL 拷回，未纳入文档说明 | tests/python/ | 已在本档第 7 节说明 |

## 5. 环境备忘（换机器/重装时看这里）

- WSL 发行版：**Ubuntu-22.04**（不是默认的 Ubuntu！bat 脚本都已指向它）
- 构建目录：WSL 内 `~/oj_platform`（ext4 本地盘，勿在 /mnt/d 编译）
- MySQL：`sudo service mysql start`（WSL 每次开机后），用户为当前 Linux 用户名 + auth_socket
- 部署脚本：`setup.sh`（全量）/ `dev_build.sh --test`（日常）/ 根目录 4 个 bat（Windows 双击）
- 换源脚本：`fix_sources.sh`（清华源，兼容新旧 sources 格式）
- 服务必须**从项目根目录**启动（`cd ~/oj_platform && ./build/oj_server`），否则找不到 config/ 和 public/

## 6. 更新本文档的约定

- 每完成一个待办功能：第 3 节该行标记 ✅ 并把模块信息并入第 2 节表格；
- 每次收工：更新第 1 节快照的日期与各项状态；
- 新发现的坑：写进 [DEVELOPMENT.md](DEVELOPMENT.md) 第 8 节，别只记在脑子里。
