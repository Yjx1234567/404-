// 个人中心：做题统计 + 难度进度 + 最近提交
// 数据来源：GET /api/users/me/stats、GET /api/problems?difficulty=X（取各难度总题数）、
//          GET /api/submissions?page_size=5

function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

function statusClass(status) {
    const known = ['AC', 'WA', 'TLE', 'MLE', 'RE', 'CE', 'SE', 'PENDING', 'JUDGING'];
    return known.includes(status) ? status : 'SE';
}

function statusText(status) {
    if (status === 'PENDING') return '排队中';
    if (status === 'JUDGING') return '判题中';
    return status;
}

async function loadProfile() {
    const user = checkAuth();
    if (!user) return;

    // 用户信息（来自登录态）
    document.getElementById('profileUsername').textContent = user.username;
    document.getElementById('profileAvatar').textContent = user.username.charAt(0).toUpperCase();
    document.getElementById('profileRole').textContent =
        user.role === 'admin' ? '管理员' : '学生';

    // 统计数据
    const { status, data } = await getMyStats();
    if (status !== 200 || data.error) {
        document.getElementById('statSolved').textContent = '-';
        return;
    }

    document.getElementById('statSolved').textContent = data.solved;
    document.getElementById('statAttempted').textContent = data.attempted;
    document.getElementById('statSubmissions').textContent = data.submissions;
    document.getElementById('statRate').textContent =
        data.submissions > 0 ? Math.round(data.solved / data.submissions * 100) + '%' : '-';

    renderDifficultyBars(data.solved_by_difficulty || {});
    loadRecentSubmissions();
}

async function renderDifficultyBars(solvedByDiff) {
    const el = document.getElementById('difficultyBars');
    const levels = ['Easy', 'Medium', 'Hard'];
    const labels = { Easy: '简单', Medium: '中等', Hard: '困难' };

    // 并行取各难度总题数，计算完成进度
    const totals = await Promise.all(levels.map(async (lv) => {
        const { status, data } = await getProblems({ difficulty: lv, page_size: 1 });
        return status === 200 ? (data.total || 0) : 0;
    }));

    el.innerHTML = levels.map((lv, i) => {
        const solved = solvedByDiff[lv] || 0;
        const total = totals[i];
        const pct = total > 0 ? Math.round(solved / total * 100) : 0;
        return `
            <div class="difficulty-bar-row">
                <span class="difficulty-bar-label">${labels[lv]}</span>
                <div class="difficulty-bar-track">
                    <div class="difficulty-bar-fill ${lv.toLowerCase()}" style="width:${pct}%"></div>
                </div>
                <span class="difficulty-bar-count">${solved} / ${total}</span>
            </div>
        `;
    }).join('');
}

async function loadRecentSubmissions() {
    const el = document.getElementById('recentSubmissions');
    const { status, data } = await getSubmissions({ page: 1, page_size: 5 });
    if (status !== 200 || !data.submissions) {
        el.innerHTML = '<div class="empty-state">加载失败</div>';
        return;
    }
    if (data.submissions.length === 0) {
        el.innerHTML = '<div class="empty-state">还没有提交记录，去做第一道题吧</div>';
        return;
    }
    el.innerHTML = data.submissions.map(s => `
        <div class="recent-sub-item">
            <a href="${escapeHtml(appPageUrl('problem.html?id=' + encodeURIComponent(s.problem_id)))}" class="recent-sub-problem">${escapeHtml(s.problem_title || ('题目 ' + s.problem_id))}</a>
            <div class="recent-sub-meta">
                <span class="status-tag ${statusClass(s.status)}">${statusText(s.status)}</span>
                <span>${s.passed}/${s.total}</span>
                <span class="recent-sub-date">${s.created_at}</span>
            </div>
        </div>
    `).join('');
}

loadProfile();
