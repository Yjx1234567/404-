#!/usr/bin/env bash
# ============================================================================
# OJ 平台一键部署脚本（在 WSL / Ubuntu 22.04 中运行）
# 用法：
#   chmod +x setup.sh && ./setup.sh
#
# 流程：换源 → 装依赖 → 同步源码到 WSL 本地盘 → 配置 MySQL
#       → 初始化数据库 → 改配置 → 编译 → 创建管理员 → 启动
#
# 说明：
#   源码从 /mnt/d/... 同步到 ~/oj_platform 再编译。
#   原因：/mnt 是 Windows 磁盘(drvfs)，CMake 生成的测试可执行文件
#         在那里没有执行权限，会报 "Operation not permitted"。
# ============================================================================
set -e

# ---------- 颜色输出 ----------
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'
info()  { echo -e "${GREEN}[INFO]${NC} $1"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $1"; }
die()   { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
SYNC_DIR="$HOME/oj_platform"

info "源码目录(Windows 挂载): $PROJECT_DIR"
info "构建目录(WSL 本地磁盘): $SYNC_DIR"

# ============================================================================
# 步骤 1/6 : 安装系统依赖
# ============================================================================
info "步骤 1/6: 安装系统依赖（需要 sudo 权限，可能要等几分钟）..."

# 1.0 先换源（幂等，重复运行安全）
if [ -f "$PROJECT_DIR/fix_sources.sh" ]; then
    info "确保软件源为清华源..."
    bash "$PROJECT_DIR/fix_sources.sh" || warn "换源失败，尝试继续用默认源..."
else
    warn "未找到 fix_sources.sh，跳过换源"
fi

# 1.1 更新包列表
sudo apt-get update -qq || die "apt-get update 失败，请检查 WSL 网络连接"

# 1.2 校验 Ubuntu 版本与 botan 包
. /etc/os-release 2>/dev/null || die "无法读取 /etc/os-release"
CODENAME="${UBUNTU_CODENAME:-$VERSION_CODENAME}"
info "系统版本: ${PRETTY_NAME:-unknown}  代号=${CODENAME}"

BOTAN_PKG="libbotan-2-dev"
if ! apt-cache show "$BOTAN_PKG" >/dev/null 2>&1; then
    echo ""
    warn "============================================================"
    warn " 在当前系统中找不到 ${BOTAN_PKG}"
    warn ""
    warn " 这通常说明你装的是 Ubuntu 24.04 或更高版本，"
    warn " 那些版本的软件源里已经没有 Botan 2.x 了，"
    warn " 而本项目代码用的是 Botan 2.x 的 API。"
    warn ""
    warn " 解决办法：换成 Ubuntu 22.04"
    warn "   1) 在 Windows 上双击  0-uninstall-old-Ubuntu.bat  删除旧版"
    warn "   2) 双击  1-双击安装WSL.bat  安装 Ubuntu 22.04"
    warn "   3) 再双击  2-双击部署OJ.bat"
    warn "============================================================"
    echo ""

    # 兜底：若真有 3.x 就装上，但明确告知需要改代码，这里直接停下更稳妥
    if apt-cache show libbotan-3-dev >/dev/null 2>&1; then
        die "检测到只有 libbotan-3-dev。本项目代码需 Botan 2.x，请按上面提示换到 Ubuntu 22.04 后重试。"
    fi
    die "找不到任何 botan 开发包，请检查软件源配置。"
fi
info "将使用: ${BOTAN_PKG}"

# 1.3 安装
sudo apt-get install -y \
    cmake g++ make \
    mysql-server \
    libmysqlclient-dev \
    libyaml-cpp-dev \
    ${BOTAN_PKG} \
    libjsoncpp-dev \
    libgtest-dev \
    || die "依赖安装失败，请把上面的红色报错截图发给助手。常见原因：
   1) 网络问题导致包列表拉不全 -> 再跑一次 bash fix_sources.sh 后重试
   2) 软件源被换坏 -> 用 /etc/apt/sources.list.bak.oj 还原"

BOTAN_VER="$(dpkg-query -W -f='${Version}' ${BOTAN_PKG} 2>/dev/null || echo unknown)"
info "依赖安装完成: $(g++ --version | head -1)"
info "  ${BOTAN_PKG} 版本: ${BOTAN_VER}"

# 确认 botan 头文件确实是 2.x
if ! ls /usr/include/botan-2/botan/bcrypt.h >/dev/null 2>&1; then
    die "找不到 /usr/include/botan-2/botan/bcrypt.h，Botan 2.x 头文件缺失，请检查安装。"
fi
info "  Botan 2.x 头文件已就位"

# ============================================================================
# 步骤 2/6 : 同步源码到 WSL 本地磁盘
# ============================================================================
info "步骤 2/6: 同步源码到 WSL 本地磁盘（避免 /mnt 权限问题）..."
mkdir -p "$SYNC_DIR"

for item in src config public database third_party tests tools; do
    if [ -d "$PROJECT_DIR/$item" ]; then
        mkdir -p "$SYNC_DIR/$item"
        cp -rf "$PROJECT_DIR/$item/." "$SYNC_DIR/$item/"
        info "  同步目录: $item/"
    fi
done

for item in CMakeLists.txt README.md; do
    if [ -f "$PROJECT_DIR/$item" ]; then
        cp -f "$PROJECT_DIR/$item" "$SYNC_DIR/"
        info "  同步文件: $item"
    fi
done

cd "$SYNC_DIR"
info "已切换到构建目录: $(pwd)"

# ============================================================================
# 步骤 3/6 : 配置 MySQL 并初始化数据库
# ============================================================================
info "步骤 3/6: 配置 MySQL 并初始化数据库..."
sudo service mysql start >/dev/null 2>&1 || sudo systemctl start mysql >/dev/null 2>&1 || warn "MySQL 启动失败，请手动检查"

# 等 MySQL 就绪
for i in $(seq 1 30); do
    if sudo mysqladmin ping >/dev/null 2>&1; then break; fi
    sleep 1
done

CURRENT_USER="$(whoami)"
# auth_socket 认证：Linux 用户名即 MySQL 用户名，无需密码
sudo mysql -e "CREATE USER IF NOT EXISTS '${CURRENT_USER}'@'localhost' IDENTIFIED WITH auth_socket;" 2>/dev/null || \
    warn "MySQL 用户可能已存在"
sudo mysql -e "GRANT ALL ON *.* TO '${CURRENT_USER}'@'localhost'; FLUSH PRIVILEGES;"
info "MySQL 用户 '${CURRENT_USER}' 就绪（auth_socket 认证）"

mysql -u "${CURRENT_USER}" < database/init.sql
mysql -u "${CURRENT_USER}" -e "USE oj_system; SHOW TABLES;" | tail -n +2 | sed 's/^/      - /'

# ============================================================================
# 步骤 4/6 : 更新配置文件
# ============================================================================
info "步骤 4/6: 更新 config/config.yaml..."
sed -i "/^[[:space:]]*username:/s|.*|  username: \"${CURRENT_USER}\"|" config/config.yaml
grep "username" config/config.yaml | head -1 | sed 's/^/      /'

# ============================================================================
# 步骤 5/6 : 编译
# ============================================================================
info "步骤 5/6: 编译项目（首次约 1-3 分钟）..."
rm -rf build
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -5
make -j"$(nproc)" 2>&1 | tail -5
cd "$SYNC_DIR"

[ -f build/oj_server ] || die "编译失败：build/oj_server 未生成。请把上面的报错截图发给助手。"
info "编译成功: $SYNC_DIR/build/oj_server"

# ============================================================================
# 步骤 6/6 : 创建管理员并启动
# ============================================================================
info "步骤 6/6: 启动服务..."
echo ""
echo "=========================================="
echo "  部署已完成，即将创建管理员并启动服务。"
echo "  管理员账号: admin / admin123"
echo "=========================================="
echo ""
read -r -p "是否现在创建管理员并启动服务? [Y/n] " yn
case "$yn" in
    [nN]*) info "部署完成。以后启动请双击  3-双击启动OJ服务.bat"; exit 0 ;;
    *) ;;
esac

./build/create_admin_user || warn "管理员创建失败，请手动执行 ~/oj_platform/build/create_admin_user"
info "服务启动中... 浏览器访问 http://localhost:8080（Ctrl+C 停止）"
./build/oj_server
