#!/usr/bin/env bash
# ============================================================================
# 自动换源脚本（WSL Ubuntu 一键把默认国际源换成清华源）
# 兼容两种格式：
#   1. 老式：/etc/apt/sources.list
#   2. 新式（Ubuntu 24.04 默认）：/etc/apt/sources.list.d/ubuntu.sources
#   3. /etc/apt/sources.list.d/*.list
# 用法：bash fix_sources.sh
# 幂等：重复运行也安全（sed 替换回相同字符串）
# ============================================================================
set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'
info() { echo -e "${GREEN}[INFO]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
err()  { echo -e "${RED}[ERROR]${NC} $1"; }

# --- 1. 检测 Ubuntu 代号（focal / jammy / noble / ...） ---
. /etc/os-release 2>/dev/null || {
    err "无法读取 /etc/os-release，可能不是 Ubuntu 系统？"
    exit 1
}
CODENAME="${UBUNTU_CODENAME:-$VERSION_CODENAME}"
info "检测到系统: ${PRETTY_NAME:-Ubuntu}  代号=${CODENAME}"

# --- 2. 选择清华源 ---
TSINGHUA="mirrors.tuna.tsinghua.edu.cn"
ALIYUN="mirrors.aliyun.com"
# 优先清华，失败时回退阿里云
PRIMARY="$TSINGHUA"

backup_and_replace() {
    local f="$1"
    [ -f "$f" ] || return 0
    # 跳过已备份过的文件
    case "$f" in
        *.bak.*) return 0 ;;
    esac
    # 备份
    if [ ! -f "${f}.bak.oj" ]; then
        sudo cp "$f" "${f}.bak.oj"
        info "已备份: $f -> ${f}.bak.oj"
    fi
    # 替换 archive.ubuntu.com / security.ubuntu.com / ports.ubuntu.com
    sudo sed -i \
        -e "s|//archive.ubuntu.com|//${PRIMARY}|g" \
        -e "s|//security.ubuntu.com|//${PRIMARY}|g" \
        -e "s|//ports.ubuntu.com|//${PRIMARY}|g" \
        "$f"
    info "已替换: $f"
}

# --- 3. 处理所有可能的源文件 ---
# 收集候选文件（包括 sources.list 旧格式 + 新版 ubuntu.sources + sources.list.d/*.list *.sources）
CANDIDATES=()
[ -f /etc/apt/sources.list ] && CANDIDATES+=("/etc/apt/sources.list")
[ -f /etc/apt/sources.list.d/ubuntu.sources ] && CANDIDATES+=("/etc/apt/sources.list.d/ubuntu.sources")
if [ -d /etc/apt/sources.list.d ]; then
    while IFS= read -r f; do
        case "$f" in
            *.bak.*) continue ;;
        esac
        CANDIDATES+=("$f")
    done < <(find /etc/apt/sources.list.d -maxdepth 1 -type f \( -name "*.list" -o -name "*.sources" \) 2>/dev/null)
fi

if [ ${#CANDIDATES[@]} -eq 0 ]; then
    err "未找到任何 apt 源文件，结构异常。退出。"
    exit 1
fi

info "找到 ${#CANDIDATES[@]} 个源文件，开始处理..."
for f in "${CANDIDATES[@]}"; do
    # 跳过已知非 ubuntu 源（docker、vscode 等），保险起见只改包含 ubuntu.com 的文件
    if grep -qE "(archive|security|ports)\.ubuntu\.com" "$f" 2>/dev/null; then
        backup_and_replace "$f"
    else
        info "跳过非 Ubuntu 源: $f"
    fi
done

# --- 4. 检查替换结果 ---
info "--- 替换后的源（前 5 行/文件）---"
for f in "${CANDIDATES[@]}"; do
    case "$f" in
        *.bak.*) continue ;;
    esac
    if grep -q "${PRIMARY}" "$f" 2>/dev/null; then
        echo "  [OK] $f"
        head -3 "$f" | sed 's/^/      /'
    fi
done

# --- 5. apt-get update ---
info "运行 apt-get update..."
if sudo apt-get update 2>&1 | tail -5; then
    info "apt-get update 成功"
else
    err "apt-get update 失败，可能是网络问题或清华源暂不可用"
    err "请尝试以下任一方案后重新运行本脚本："
    err "  方案 A：换阿里源：执行 sed -i 's|${PRIMARY}|${ALIYUN}|g' /etc/apt/sources.list* /etc/apt/sources.list.d/*.sources 2>/dev/null"
    err "  方案 B：恢复默认源：sudo cp /etc/apt/sources.list.bak.oj /etc/apt/sources.list && sudo cp /etc/apt/sources.list.d/ubuntu.sources.bak.oj /etc/apt/sources.list.d/ubuntu.sources"
    err "  方案 C：使用代理后重试。"
    exit 1
fi

info "换源完成！"
