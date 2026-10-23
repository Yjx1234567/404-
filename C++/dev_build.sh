#!/usr/bin/env bash
# ============================================================================
# 开发用增量编译脚本（区别于 setup.sh 的完整部署）
# 用法：bash dev_build.sh [--test]
#   同步源码到 ~/oj_platform → 确认 MySQL 就绪 → 增量编译 → （可选）跑 ctest
# ============================================================================
set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
SYNC_DIR="$HOME/oj_platform"

echo "[INFO] 源码: $PROJECT_DIR"
echo "[INFO] 构建: $SYNC_DIR"

# ---- 同步源码（镜像 setup.sh 的同步范围）----
for item in src config public database third_party tests tools; do
    if [ -d "$PROJECT_DIR/$item" ]; then
        mkdir -p "$SYNC_DIR/$item"
        cp -rf "$PROJECT_DIR/$item/." "$SYNC_DIR/$item/"
    fi
done
for item in CMakeLists.txt README.md API.md SPEC.md; do
    [ -f "$PROJECT_DIR/$item" ] && cp -f "$PROJECT_DIR/$item" "$SYNC_DIR/"
done

cd "$SYNC_DIR"

# ---- MySQL 就绪 ----
if ! sudo service mysql start >/dev/null 2>&1; then
    sudo systemctl start mysql >/dev/null 2>&1 || true
fi
for i in $(seq 1 30); do
    sudo mysqladmin ping >/dev/null 2>&1 && break
    sleep 1
done

CURRENT_USER="$(whoami)"
sudo mysql -e "CREATE USER IF NOT EXISTS '${CURRENT_USER}'@'localhost' IDENTIFIED WITH auth_socket;" 2>/dev/null || true
sudo mysql -e "GRANT ALL ON *.* TO '${CURRENT_USER}'@'localhost'; FLUSH PRIVILEGES;"
mysql -u "${CURRENT_USER}" < database/init.sql

# ---- 配置文件用户名对齐当前 Linux 用户（与 setup.sh 行为一致）----
sed -i "/^[[:space:]]*username:/s|.*|  username: \"${CURRENT_USER}\"|" config/config.yaml

# ---- 增量编译 ----
mkdir -p build
cd build
if [ ! -f Makefile ]; then
    cmake .. -DCMAKE_BUILD_TYPE=Release > /dev/null
fi
make -j"$(nproc)" 2>&1 | grep -E "error|Error|warning|Built target" | tail -20

if [ ! -f oj_server ]; then
    echo "[ERROR] 编译失败"
    exit 1
fi
echo "[INFO] 编译成功: $SYNC_DIR/build/oj_server"

# ---- 可选：跑测试 ----
if [ "$1" == "--test" ]; then
    cd "$SYNC_DIR/build"
    ctest --output-on-failure 2>&1 | tail -20
fi
