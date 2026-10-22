// 提交记录页：我的提交历史 + 判题结果详情
// URL 参数：?problem_id=1 只看某题的提交（从题目页跳转时携带）

const urlParams = new URLSearchParams(window.location.search);
let state = {
    page: 1,
    pageSize: 20,
    problemId: urlParams.get('problem_id') || '',
    total: 0
};

let refreshTimer = null;

function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

function statusClass(status) {
    const known = ['AC', 'WA', 'TLE', 'MLE', 'RE', 'CE', 'SE', 'PENDING', 'JUDGING'];
    return known.includes(status) ? status : 'SE';
}

function isJudging(status) {
    return status === 'PENDING' || status === 'JUDGING';
}

function statusText(status) {
    if (status === 'PENDING') return '排队中';
    if (status === 'JUDGING') return '判题中';
    return status;
}

async function loadSubmissions() {
    const list = document.getElementById('submissionList');
    list.innerHTML = '<div class="loading">加载中...</div>';

    const params = { page: state.page, page_size: state.pageSize };
    if (state.problemId) params.problem_id = state.problemId;

    const { status, data } = await getSubmissions(params);
    if (status !== 200 || !data.submissions) {
        list.innerHTML = '<div class="empty-state">加载失败</div>';
        return;
    }

    state.total = data.total;
    renderSubmissions(data.submissions);
    renderPagination();
    scheduleRefresh(data.submissions);
}

function renderSubmissions(submissions) {
    const list = document.getElementById('submissionList');

    if (submissions.length === 0) {
        list.innerHTML = '<div class="empty-state">暂无提交记录</div>';
        return;
    }

    list.innerHTML = submissions.map(s => `
        <div class="submission-item" data-id="${s.id}">
            <div class="submission-item-main">
                <span class="submission-id">#${s.id}</span>
                <a href="${escapeHtml(appPageUrl('problem.html?id=' + encodeURIComponent(s.problem_id)))}" class="submission-problem">${escapeHtml(s.problem_title || ('题目 ' + s.problem_id))}</a>
            </div>
            <div class="submission-item-meta">
                <span class="status-tag ${statusClass(s.status)}">${statusText(s.status)}</span>
                <span class="submission-passed">${s.passed}/${s.total}</span>
                <span class="submission-time">${s.time_ms}ms</span>
                <span class="submission-date">${s.created_at}</span>
                <button class="btn-view" onclick="viewDetail(${s.id})">查看</button>
            </div>
        </div>
    `).join('');
}

// 列表里还有排队/判题中的记录时，定时刷新当前页
function scheduleRefresh(submissions) {
    if (refreshTimer) {
        clearTimeout(refreshTimer);
        refreshTimer = null;
    }
    if (!submissions.some(s => isJudging(s.status))) return;
    refreshTimer = setTimeout(() => loadSubmissions(), 1200);
}

function renderPagination() {
    const el = document.getElementById('pagination');
    const totalPages = Math.max(1, Math.ceil(state.total / state.pageSize));
    if (totalPages <= 1) {
        el.innerHTML = '';
        return;
    }
    el.innerHTML = `
        <button class="page-btn" id="prevPage" ${state.page <= 1 ? 'disabled' : ''}>&laquo; 上一页</button>
        <span class="page-info">第 ${state.page} / ${totalPages} 页（共 ${state.total} 条）</span>
        <button class="page-btn" id="nextPage" ${state.page >= totalPages ? 'disabled' : ''}>下一页 &raquo;</button>
    `;
    document.getElementById('prevPage').addEventListener('click', () => {
        if (state.page > 1) { state.page--; loadSubmissions(); }
    });
    document.getElementById('nextPage').addEventListener('click', () => {
        const totalPages = Math.max(1, Math.ceil(state.total / state.pageSize));
        if (state.page < totalPages) { state.page++; loadSubmissions(); }
    });
}

async function viewDetail(id) {
    const panel = document.getElementById('detailPanel');
    const { status, data } = await getSubmission(id);
    if (status !== 200 || data.error) {
        alert(data.error || '加载失败');
        return;
    }

    const statusEl = document.getElementById('detailStatus');
    statusEl.textContent = statusText(data.status);
    statusEl.className = `result-status ${statusClass(data.status)}`;
    document.getElementById('detailMessage').textContent =
        `${data.message} · ${data.passed}/${data.total} 通过 · ${data.time_ms}ms · 提交于 ${data.created_at}`;

    document.getElementById('detailMeta').textContent = '';

    // 编译错误 / 系统错误信息
    const errEl = document.getElementById('detailError');
    if (data.error_info && data.status !== 'AC') {
        errEl.innerHTML = `<div class="result-case-value result-error">${escapeHtml(data.error_info)}</div>`;
    } else {
        errEl.innerHTML = '';
    }

    // 逐用例结果
    const cases = data.details || [];
    document.getElementById('detailCases').innerHTML = cases.map(d => `
        <div class="result-case">
            <div class="result-case-header">
                <span class="result-case-status ${d.status === 'AC' ? 'pass' : 'fail'}">
                    ${d.status === 'AC' ? '✓' : '✗'} 用例 ${d.case} · ${d.status || ''}
                </span>
                <span class="result-case-meta">用时 ${d.elapsed_ms}ms</span>
            </div>
            ${d.status === 'WA' ? `
            <div class="result-case-row">
                <div>
                    <div class="result-case-label">期望输出</div>
                    <div class="result-case-value">${escapeHtml(d.expected || '')}</div>
                </div>
                <div>
                    <div class="result-case-label">实际输出</div>
                    <div class="result-case-value">${escapeHtml(d.actual || '')}</div>
                </div>
            </div>
            ` : ''}
            ${d.error ? `<div class="result-case-value result-error result-case-error">${escapeHtml(d.error)}</div>` : ''}
        </div>
    `).join('');

    document.getElementById('detailCode').textContent = data.code || '';

    // 参考答案：仅在判题未通过且题目配置了参考答案时展示
    const solWrap = document.getElementById('detailSolutionWrap');
    if (data.status && data.status !== 'AC' && data.solution) {
        document.getElementById('detailSolution').textContent = data.solution;
        solWrap.classList.remove('hidden');
    } else {
        solWrap.classList.add('hidden');
    }

    panel.classList.remove('hidden');
    const reducedMotion = window.matchMedia && window.matchMedia('(prefers-reduced-motion: reduce)').matches;
    panel.scrollIntoView({ behavior: reducedMotion ? 'auto' : 'smooth', block: 'start' });
}

// ===== 事件绑定 =====

document.getElementById('filterBtn').addEventListener('click', () => {
    const v = document.getElementById('problemFilter').value.trim();
    state.problemId = v && parseInt(v, 10) > 0 ? v : '';
    state.page = 1;
    loadSubmissions();
});

document.getElementById('clearFilterBtn').addEventListener('click', () => {
    document.getElementById('problemFilter').value = '';
    state.problemId = '';
    state.page = 1;
    loadSubmissions();
});

document.getElementById('problemFilter').addEventListener('keydown', (e) => {
    if (e.key === 'Enter') document.getElementById('filterBtn').click();
});

document.getElementById('detailCloseBtn').addEventListener('click', () => {
    document.getElementById('detailPanel').classList.add('hidden');
});

if (state.problemId) {
    document.getElementById('problemFilter').value = state.problemId;
}

loadSubmissions();
