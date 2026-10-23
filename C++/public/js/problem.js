// 题目列表：服务端分页 + 难度筛选 + 标题关键词搜索
// （题量大时不再一次拉全量，翻页/筛选都走 GET /api/problems 查询参数）

const PAGE_SIZE = 20;

let state = {
    page: 1,
    difficulty: 'all',
    keyword: '',
    total: 0
};

let searchTimer = null;

function escapeProblemText(value) {
    const entities = { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' };
    return String(value ?? '').replace(/[&<>"']/g, char => entities[char]);
}

function problemLoadFailure(status, data) {
    if (status === 0) return '无法连接题库服务，请确认已启动 OJ 服务后刷新重试。';
    if (status === 401) return '登录状态已失效，请重新登录后查看题库。';
    if (status === 403) return '当前账号没有查看题库的权限，请联系管理员。';
    if (status === 404) return '题库接口不存在，请确认已启动最新版本的 OJ 服务。';
    const reason = data && typeof data.error === 'string' ? data.error.trim() : '';
    const knownErrors = { 'internal server error': '服务器内部错误', 'invalid json': '请求数据格式不正确' };
    const detail = Object.prototype.hasOwnProperty.call(knownErrors, reason) ? knownErrors[reason] : reason;
    if (status >= 500) return `题库服务暂时不可用（HTTP ${status}）${detail ? '：' + detail : '，请稍后重试或重新启动 OJ 服务。'}`;
    if (status === 200) return '服务器返回的题库数据格式不正确，请重新启动 OJ 服务后重试。';
    return `题库加载失败（HTTP ${status}）${detail ? '：' + detail : '，请重试。'}`;
}

function showProblemLoadFailure(status, data) {
    document.getElementById('problemList').innerHTML =
        `<div class="empty-state" role="alert">${escapeProblemText(problemLoadFailure(status, data))}</div>`;
    document.getElementById('pagination').innerHTML = '';
}

async function loadProblems() {
    const list = document.getElementById('problemList');
    list.innerHTML = '<div class="loading">加载中...</div>';

    const params = { page: state.page, page_size: PAGE_SIZE };
    if (state.difficulty !== 'all') params.difficulty = state.difficulty;
    if (state.keyword) params.keyword = state.keyword;

    try {
        const { status, data } = await getProblems(params);
        if (status !== 200 || !data || !Array.isArray(data.problems) ||
            !data.problems.every(problem => problem && typeof problem === 'object')) {
            showProblemLoadFailure(status, data);
            return;
        }

        const total = Number(data.total);
        state.total = Number.isFinite(total) && total >= 0 ? Math.floor(total) : data.problems.length;
        renderProblems(data.problems);
        renderPagination();
    } catch (error) {
        showProblemLoadFailure(0);
    }
}

function renderProblems(problems) {
    const list = document.getElementById('problemList');

    if (problems.length === 0) {
        list.innerHTML = '<div class="empty-state">暂无题目</div>';
        return;
    }

    const difficultyLabels = { Easy: '简单', Medium: '中等', Hard: '困难' };
    list.innerHTML = problems.map(p => {
        const difficultyClass = Object.prototype.hasOwnProperty.call(difficultyLabels, p.difficulty)
            ? p.difficulty.toLowerCase() : '';
        return `
            <a href="${escapeProblemText(appPageUrl('problem.html?id=' + encodeURIComponent(p.id)))}" class="problem-card">
                <span class="problem-number">#${escapeProblemText(p.id)}</span>
                <span class="problem-card-title">${escapeProblemText(p.title)}</span>
                <span class="difficulty-tag ${difficultyClass}">${escapeProblemText(difficultyLabels[p.difficulty] || p.difficulty)}</span>
                <span class="problem-card-meta">${escapeProblemText(p.created_at)}</span>
            </a>
        `;
    }).join('');
}

function renderPagination() {
    const el = document.getElementById('pagination');
    const totalPages = Math.max(1, Math.ceil(state.total / PAGE_SIZE));
    if (totalPages <= 1) {
        el.innerHTML = '';
        return;
    }
    el.innerHTML = `
        <button class="page-btn" id="prevPage" ${state.page <= 1 ? 'disabled' : ''}>&laquo; 上一页</button>
        <span class="page-info">第 ${state.page} / ${totalPages} 页（共 ${state.total} 题）</span>
        <button class="page-btn" id="nextPage" ${state.page >= totalPages ? 'disabled' : ''}>下一页 &raquo;</button>
    `;
    document.getElementById('prevPage').addEventListener('click', () => {
        if (state.page > 1) { state.page--; loadProblems(); }
    });
    document.getElementById('nextPage').addEventListener('click', () => {
        if (state.page < totalPages) { state.page++; loadProblems(); }
    });
}

// 难度筛选按钮：切换后从第一页重新查询
const filterButtons = document.querySelectorAll('.filter-btn');
filterButtons.forEach(btn => {
    btn.addEventListener('click', () => {
        filterButtons.forEach(b => {
            b.classList.remove('active');
            b.setAttribute('aria-pressed', 'false');
        });
        btn.classList.add('active');
        btn.setAttribute('aria-pressed', 'true');
        state.difficulty = btn.dataset.filter;
        state.page = 1;
        loadProblems();
    });
});

// 标题搜索：输入防抖 400ms 后自动查询，回车立即查
const searchInput = document.getElementById('searchInput');
searchInput.addEventListener('input', () => {
    if (searchTimer) clearTimeout(searchTimer);
    searchTimer = setTimeout(() => {
        state.keyword = searchInput.value.trim();
        state.page = 1;
        loadProblems();
    }, 400);
});
searchInput.addEventListener('keydown', (e) => {
    if (e.key === 'Enter') {
        if (searchTimer) clearTimeout(searchTimer);
        state.keyword = searchInput.value.trim();
        state.page = 1;
        loadProblems();
    }
});

loadProblems();
