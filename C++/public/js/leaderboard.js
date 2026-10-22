// 排行榜：按 AC 题数排名（后端已排序，取前 50）
// 数据来源：GET /api/leaderboard

function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

async function loadLeaderboard() {
    const el = document.getElementById('leaderboardList');
    const { status, data } = await getLeaderboard();

    if (status !== 200 || !data.leaderboard) {
        el.innerHTML = '<div class="empty-state">加载失败</div>';
        return;
    }

    if (data.leaderboard.length === 0) {
        el.innerHTML = '<div class="empty-state">暂无数据</div>';
        return;
    }

    // 高亮当前登录用户
    const me = checkAuth();
    const myName = me ? me.username : '';

    el.innerHTML = data.leaderboard.map(entry => {
        const isMe = entry.username === myName;
        const medal = entry.rank <= 3 ? `<span class="medal medal-${entry.rank}">${entry.rank}</span>`
                                      : `<span class="rank-num">${entry.rank}</span>`;
        return `
            <div class="leaderboard-item${isMe ? ' leaderboard-me' : ''}">
                ${medal}
                <span class="leaderboard-name">${escapeHtml(entry.username)}${isMe ? ' <span class="admin-badge">我</span>' : ''}</span>
                <div class="leaderboard-stats">
                    <span class="leaderboard-solved">${entry.solved} 题</span>
                    <span class="leaderboard-subs">${entry.submissions} 次提交</span>
                </div>
            </div>
        `;
    }).join('');
}

loadLeaderboard();
