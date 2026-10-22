# 部署指南

本指南介绍如何在单机 Linux 环境下完整部署 C++ 在线评测系统。

## 环境要求
- Linux 系统（Ubuntu 20.04+ 或类似发行版）
- GCC/G++ 9+
- CMake 3.10+
- MySQL 8.0

## 步骤 1：安装系统依赖
```bash
sudo apt update
sudo apt install -y cmake g++ make mysql-server libmysqlclient-dev libyaml-cpp-dev libbotan-2-dev libjsoncpp-dev libgtest-dev
```
验证安装：
```bash
g++ --version    # 应显示 9.x 或更高
cmake --version  # 应显示 3.10 或更高
mysql --version  # 应显示 8.0 或更高
```

## 步骤 2：配置 MySQL
### 启动 MySQL 服务
```bash
sudo systemctl enable mysql
sudo systemctl start mysql
```

### 创建数据库用户
将 `your_username` 替换为你的 Linux 用户名：
```bash
sudo mysql -e "CREATE USER IF NOT EXISTS 'your_username'@'localhost' IDENTIFIED WITH auth_socket;"
sudo mysql -e "GRANT ALL ON *.* TO 'your_username'@'localhost';"
```

### 初始化数据库
```bash
mysql -u your_username < /path/to/cpp-oj-vibecoding-teach/database/init.sql
```
验证数据库创建：
```bash
mysql -u your_username -e "USE oj_system; SHOW TABLES;"
```
预期输出：
```
+---------------+
| Tables        |
+---------------+
| problems      |
| test_cases    |
| users         |
| sessions      |
+---------------+
```

## 步骤 3：编译项目
```bash
cd /path/to/cpp-oj-vibecoding-teach
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```
编译完成后可执行文件位于 `build/oj_server`。

## 步骤 4：配置
编辑 `config/config.yaml` 匹配你的环境：
```yaml
server:
  host: 0.0.0.0      # 监听所有网络接口
  port: 8080         # HTTP 端口
  thread_pool_size: 4
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

## 步骤 5：创建管理员账号
```bash
./build/create_admin_user
```
按照提示创建管理员账户。

## 步骤 6：运行服务
### 基本运行
```bash
./build/oj_server
```
服务将在 `http://0.0.0.0:8080` 监听请求。

### 生产环境运行（后台）
使用 systemd 服务或 nohup：
```bash
nohup ./build/oj_server > server.log 2>&1 &
```

### 验证服务运行
```bash
curl http://localhost:8080/api/problems
```
应返回 `{"problems":[]}`

## 步骤 7：防火墙配置（如需要）
```bash
sudo ufw allow 8080/tcp
```

## 部署目录结构建议
推荐部署目录结构：
```
/opt/oj-system/
├── config/
│   └── config.yaml
├── public/
│   └── (前端文件)
├── build/
│   └── oj_server
├── database/
│   └── init.sql
└── logs/
```
相应更新 `config/config.yaml` 中的路径。

## Systemd 服务（可选）
创建 `/etc/systemd/system/oj-server.service`：
```ini
[Unit]
Description=C++ OJ Server
After=network.target mysql.service

[Service]
Type=simple
User=your_username
WorkingDirectory=/path/to/cpp-oj-vibecoding-teach
ExecStart=/path/to/cpp-oj-vibecoding-teach/build/oj_server
Restart=on-failure

[Install]
WantedBy=multi-user.target
```
启用并启动服务：
```bash
sudo systemctl daemon-reload
sudo systemctl enable oj-server
sudo systemctl start oj-server
```
查看服务状态：
```bash
sudo systemctl status oj-server
```

## 故障排查
### MySQL 连接失败
检查 MySQL 是否运行：
```bash
sudo systemctl status mysql
```
测试连接：
```bash
mysql -u username -p
```

### 端口被占用
```bash
lsof -i :8080
kill -9 <PID>
```

### 编译失败
确认所有依赖已安装：
```bash
dpkg -l | grep -E "cmake|g++|mysql|yaml|botan|jsoncpp|gtest"
```

### auth_socket 权限问题
```bash
sudo mysql -e "ALTER USER 'username'@'localhost' IDENTIFIED WITH mysql_native_password BY 'password';"
sudo mysql -e "GRANT ALL ON *.* TO 'username'@'localhost';"
```
然后在 `config.yaml` 中配置密码。

## 安全说明
- 本系统仅使用进程级隔离
- 生产环境建议升级为容器级代码执行隔离
- 代码执行默认超时时间为 5 秒
- 内存限制为 256MB

## 日常维护
### 查看日志
```bash
tail -f server.log
```

### 数据库备份
```bash
mysqldump -u username oj_system > backup_$(date +%Y%m%d).sql
```

### 重启服务
```bash
pkill oj_server
./build/oj_server
```
