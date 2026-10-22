# C++ 在线评测系统

一个轻量级的 C++ 编程教育在线评测系统，支持题目管理、代码执行和结果验证。

## 功能特性
- **用户认证**：注册、登录、会话管理（Session/Cookie，bcrypt 密码哈希）
- **题库管理**：题目增删改查、难度等级、标签、关键词搜索、分页筛选
- **在线代码编辑器**：Ace Editor（本地化部署，无 CDN 依赖）
- **异步判题**：提交落库 + 判题队列（多 worker），服务重启自动恢复未完成判题
- **判题沙箱**：fork/exec 进程隔离，rlimit 限制 CPU/内存/输出，禁网 + 降权运行
- **多种结果状态**：AC（通过）、WA（答案错误）、TLE（超时）、MLE（内存超限）、RE（运行时错误）、CE（编译错误）
- **Special Judge 特判**：支持非唯一解题目（SPJ 源码按哈希缓存编译）
- **提交记录**：提交历史、逐用例判题详情、代码回显
- **个人统计与排行榜**：按难度统计做题数、全站 AC 排名
- **班级与作业**：创建班级（加入码进班）、布置作业、教师查看全班完成矩阵
- **提交频率限制**：每用户每分钟提交上限（防刷，默认 10 次/分钟）
- **管理后台**：管理员题目管理（含测试用例、标签、特判编辑）

## 技术栈

| 层级 | 技术 |
|------|------|
| 前端 | HTML、CSS、JavaScript（原生） |
| 后端 | C++17、cpp-httplib |
| 数据库 | MySQL 8.0 |
| 认证 | Session/Cookie、bcrypt（Botan-2） |
| 构建 | CMake 3.10+ |

## 快速开始

> **Windows 用户注意**：本项目使用 fork/unistd.h 等 Linux 系统调用实现判题隔离，**必须在 WSL (Ubuntu) 或 Linux 环境下编译运行**，Windows 本机（MinGW/MSVC）无法编译。
>
> **⚠️ 必须使用 Ubuntu 22.04，不能用 24.04（或更高）**
> 本项目依赖 Botan 2.x 的 `generate_bcrypt` / `check_bcrypt` 接口。Ubuntu 24.04 的软件源已移除 Botan 2.x（只有 3.x，且 3.x 删除了上述接口），会导致安装与编译失败。

### 日常启动：双击一个文件

完成首次环境部署后，双击根目录的 **`启动OJ.bat`**。它会自动定位当前项目，启动 MySQL，同步前端与必要源码，按需增量编译，检查服务，最后打开登录页。服务后台运行，可以关闭启动窗口。

- 不需要手动进入 WSL、复制 public 或反复执行 setup.sh。
- 重复双击会复用本项目健康服务；不会误杀其他占用端口的程序。
- 运行目录保留已有配置、题目、账号与提交记录；日常启动不重置管理员。
- CSS/JavaScript 引用按内容生成版本，避免更新后继续加载旧缓存。
- 需要停止时双击 **`停止OJ.bat`**，只停止本项目，不停止 MySQL。
- `2-双击部署OJ.bat`、`3-双击启动OJ服务.bat` 现在是同一个安全启动入口的兼容别名。

VS Code 打开本目录后，按 **Ctrl+Shift+B** 可运行默认任务“启动 CodeOJ”。也可以在“终端 → 运行任务”选择启动或停止。命令行只检查时可使用 `powershell -NoProfile -ExecutionPolicy Bypass -File tools/start-oj.ps1 -Check -NoBrowser`；测试启动但不打开网页可传 `-NoBrowser`。

全新机器仍需一次性安装 WSL Ubuntu-22.04 和项目依赖，见下方首次部署步骤。普通启动不会自动删除发行版、安装软件包或重置数据库。

### 方式一：命令行一键部署（WSL/Ubuntu 22.04）

```bash
cd /mnt/d/projects/cpp-oj-platform   # 替换为解压后的实际路径
bash setup.sh
```

脚本会自动完成：换清华源 → 安装依赖 → 同步源码到 `~/oj_platform` → 配置 MySQL（auth_socket）→ 初始化数据库 → 编译 → 创建管理员 → 启动服务。

> **为什么源码要复制到 `~/oj_platform` 再编译？**
> 项目位于 Windows 磁盘，WSL 通过 `/mnt/d` 挂载访问。drvfs 挂载点上 CMake 生成的临时可执行文件没有执行权限，会报 `CMake Error ... Operation not permitted`。同步到 WSL 本地 ext4 磁盘再编译可彻底规避。
> 在 Windows 上修改代码后，双击 `启动OJ.bat` 即可自动同步；前端改动无需重新编译 C++，后端变化才执行增量构建。首次部署前需要先用 `1-双击安装WSL.bat` 安装 Ubuntu-22.04。

### 方式二：手动部署

#### 1. 安装依赖（Ubuntu 22.04）

```bash
# 若在 Ubuntu 24.04 上，libbotan-2-dev 已不存在，请换用 22.04
sudo apt update && sudo apt install -y \
  cmake g++ make \
  mysql-server \
  libmysqlclient-dev \
  libyaml-cpp-dev \
  libbotan-2-dev \
  libjsoncpp-dev \
  libgtest-dev
```

> Windows/WSL 下建议先换国内源：`bash fix_sources.sh`（自动备份并切换到清华源，兼容 22.04 与 24.04 两种源格式）。

#### 2. 数据库配置
```bash
# 启动 MySQL 服务
sudo service mysql start   # WSL 用这个；原生 Ubuntu 用 sudo systemctl start mysql
# 创建数据库用户（将 'your_username' 替换为你的 Linux 用户名）
sudo mysql -e "CREATE USER IF NOT EXISTS 'your_username'@'localhost' IDENTIFIED WITH auth_socket; GRANT ALL ON *.* TO 'your_username'@'localhost';"
# 初始化数据库
mysql -u your_username < database/init.sql
```

#### 3. 编译构建
```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

#### 4. 运行服务
```bash
# 务必从项目根目录运行（否则找不到 config/ 和 public/）
cd ~/oj_platform     # 或你的项目根目录
./build/oj_server
```
服务启动后访问 `http://localhost:8080`

> 每次开机后需要重新启动 MySQL：`sudo service mysql start`

## 默认端口
| 服务 | 端口 |
|------|------|
| HTTP 服务 | 8080 |
| MySQL | 3306 |

## 创建管理员账号
首次启动服务后，使用管理员创建工具：
```bash
./build/create_admin_user
```

## 项目结构
```
cpp-oj-vibecoding-teach/
├── config/config.yaml          # 服务配置
├── database/init.sql            # 数据库初始化脚本
├── src/                        # 后端源码
│   ├── main.cc                 # 程序入口
│   ├── server/                 # HTTP 服务器和路由
│   ├── handler/                # API 处理器
│   ├── service/                # 业务逻辑
│   ├── model/                  # 数据模型
│   ├── db/                     # 数据库连接池
│   └── utils/                  # 工具类（日志、配置）
├── public/                     # 前端文件
│   ├── index.html              # 落地页
│   ├── login.html              # 登录页
│   ├── register.html           # 注册页
│   ├── problem_list.html       # 题目列表页
│   ├── problem.html            # 题目详情页和代码编辑器
│   ├── admin.html              # 管理后台
│   ├── css/style.css           # 样式文件
│   └── js/                     # 前端脚本
└── tools/                      # 管理工具
```

## 配置说明
编辑 `config/config.yaml`：
```yaml
server:
  host: 0.0.0.0      # 监听所有网络接口
  port: 8080         # HTTP 端口
  thread_pool_size: 4
  public_dir: "./public"   # 前端静态资源目录（相对启动目录；服务会自动回退 ./public → ../public）
database:
  host: localhost
  port: 3306
  username: your_username    # 你的 Linux 用户名
  password: ""               # 使用 auth_socket 认证，留空
  database: oj_system
  max_connections: 10
execution:
  timeout_seconds: 5
  max_memory_mb: 256
  compile_command: "g++ -std=c++17 -O2"
```

## 测试
运行单元测试：
```bash
cd build
ctest --output-on-failure
```
运行单个测试：
```bash
./build/config_test
./build/logger_test
./build/connection_pool_test
./build/problem_test
./build/executor_test
./build/auth_test
```

## 工具说明
| 工具 | 用途 |
|------|------|
| `create_admin_user` | 创建管理员用户 |
| `db_reset_tool` | 重置数据库 |

## API 文档
详细 API 说明请参考 [API.md](API.md)。

## 更多文档
| 文档 | 内容 |
|------|------|
| [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) | 架构详解、目录职责、判题生命周期、新增接口步骤、踩坑记录（**二次开发必读**） |
| [docs/PROGRESS.md](docs/PROGRESS.md) | 进度快照、已实现模块清单、待办路线图、技术债、环境备忘（**接续开发先读这份**） |
| [docs/运行指南.html](docs/运行指南.html) | 小白向运行手册（4 步部署 + FAQ） |
| [DEPLOY.md](DEPLOY.md) / [SPEC.md](SPEC.md) | 部署说明 / 系统规格 |

## 许可证
MIT
