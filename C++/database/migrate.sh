#!/usr/bin/env bash
# ============================================================================
# 存量数据库迁移脚本（幂等，可重复运行）
# 用途：老版本初始化的数据库缺少新增列/表时，执行本脚本补齐。
#       新装环境直接跑 init.sql 即可，无需本脚本。
# 用法：bash database/migrate.sh   （默认以当前用户连接本机 oj_system）
# 可选：MYSQL_USER=用户名 MYSQL_DATABASE=数据库名 bash database/migrate.sh
# 仅添加缺失列；不会删除题目、提交记录或重置用户。
# ============================================================================
set -euo pipefail

MYSQL_USER="${MYSQL_USER:-$(whoami)}"
MYSQL_DATABASE="${MYSQL_DATABASE:-oj_system}"

run_alter() {
    # $1: ALTER 语句；列已存在时（MySQL 错误 1060）视为成功跳过
    local stmt="$1"
    local out status
    if out=$(mysql --user="$MYSQL_USER" --database="$MYSQL_DATABASE" --execute="$stmt" 2>&1); then
        printf '  [OK] %s\n' "$stmt"
    else
        status=$?
        if printf '%s\n' "$out" | grep -E '^ERROR[[:space:]]+1060([[:space:]]|[(])' >/dev/null; then
            printf '  [SKIP] 列已存在: %s\n' "$stmt"
        else
            printf '  [ERROR] 迁移失败: %s\n%s\n' "$stmt" "$out" >&2
            return "$status"
        fi
    fi
}

printf '[INFO] 迁移数据库 %s 的 problems 表\n' "$MYSQL_DATABASE"
run_alter "ALTER TABLE problems ADD COLUMN spj_code MEDIUMTEXT NULL"
run_alter "ALTER TABLE problems ADD COLUMN solution MEDIUMTEXT NULL"

echo "[INFO] 迁移完成"
