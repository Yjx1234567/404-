const params = new URLSearchParams(window.location.search);
const problemId = params.get('id');

let editor = null;
let editorFallback = null;
let pollTimer = null;
let editorResizeObserver = null;

const DRAFT_KEY = 'oj_draft_problem_' + problemId;

function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

function markdownToHtml(md) {
    // Simple markdown conversion
    let html = escapeHtml(md);
    html = html.replace(/\n\n/g, '</p><p>');
    html = html.replace(/\n/g, '<br>');
    html = html.replace(/```(\w*)\n?([\s\S]*?)```/g, '<pre><code>$2</code></pre>');
    html = html.replace(/`([^`]+)`/g, '<code>$1</code>');
    html = html.replace(/###\s(.+)/g, '<h3>$1</h3>');
    html = html.replace(/##\s(.+)/g, '<h2>$1</h2>');
    html = html.replace(/#\s(.+)/g, '<h1>$1</h1>');
    return '<p>' + html + '</p>';
}

async function loadProblem() {
    if (!problemId) {
        document.getElementById('problemTitle').textContent = '无效的题目 ID';
        return;
    }

    let response;
    try {
        response = await getProblem(problemId);
    } catch (e) {
        document.getElementById('problemTitle').textContent = '题目加载失败，请稍后重试';
        return;
    }
    const { status, data } = response;
    if (status !== 200 || data.error) {
        document.getElementById('problemTitle').textContent = data.error || '题目加载失败';
        return;
    }

    document.getElementById('problemTitle').textContent = `#${data.id} ${data.title}`;
    document.getElementById('problemDifficulty').textContent = data.difficulty;
    document.getElementById('problemDifficulty').className = `difficulty-tag ${data.difficulty.toLowerCase()}`;
    document.getElementById('problemContent').innerHTML = markdownToHtml(data.content);

    const testCases = data.test_cases || [];
    document.getElementById('testCases').innerHTML = testCases.map(tc => `
        <div class="testcase-item">
            <div class="testcase-label">输入</div>
            <div class="testcase-value">${escapeHtml(tc.input)}</div>
            <div class="testcase-label">期望输出</div>
            <div class="testcase-value">${escapeHtml(tc.expected)}</div>
        </div>
    `).join('');

    // 初始化编辑器：Ace 可用则用 Ace；否则降级为 textarea（离线环境兜底）
    // 草稿优先：localStorage 里保存过未提交的代码则恢复
    let savedDraft = null;
    try { savedDraft = localStorage.getItem(DRAFT_KEY); } catch (e) { /* 隐私模式下仍可使用编辑器 */ }
    const defaultCode = savedDraft ||
        data.template ||
        '#include <iostream>\nusing namespace std;\n\nint main() {\n    \n    return 0;\n}';
    if (window.ace && !window.__aceFailed) {
        editor = ace.edit('editor');
        editor.setTheme('ace/theme/monokai');
        editor.session.setMode('ace/mode/c_cpp');
        editor.setFontSize(14);
        editor.setValue(defaultCode, -1);
        editor.session.on('change', saveDraft);
        const resizeEditor = () => editor.resize();
        window.addEventListener('resize', resizeEditor);
        if (typeof ResizeObserver !== 'undefined') {
            editorResizeObserver = new ResizeObserver(resizeEditor);
            editorResizeObserver.observe(document.getElementById('editor'));
        }
        resizeEditor();
    } else {
        const container = document.getElementById('editor');
        const fallback = document.createElement('textarea');
        fallback.id = 'editorFallback';
        fallback.className = 'editor-fallback';
        fallback.setAttribute('aria-label', 'C++ 代码编辑器');
        fallback.setAttribute('spellcheck', 'false');
        fallback.setAttribute('autocapitalize', 'off');
        container.appendChild(fallback);
        editor = {
            getValue: () => fallback.value,
            setValue: (v) => { fallback.value = v; }
        };
        fallback.value = defaultCode;
        fallback.addEventListener('input', saveDraft);
        editorFallback = fallback;
    }
}

// 代码草稿自动保存到 localStorage（按题目区分，提交成功后清除）
let draftSaveTimer = null;
function saveDraft() {
    if (!editor) return;
    if (draftSaveTimer) clearTimeout(draftSaveTimer);
    draftSaveTimer = setTimeout(() => {
        try {
            localStorage.setItem(DRAFT_KEY, editor.getValue());
        } catch (e) { /* 存储满等异常忽略 */ }
    }, 500);
}

function showResult(result) {
    const panel = document.getElementById('resultPanel');
    const status = document.getElementById('resultStatus');
    const message = document.getElementById('resultMessage');
    const details = document.getElementById('resultDetails');

    panel.classList.remove('hidden');
    panel.setAttribute('aria-busy', 'false');
    status.textContent = result.status;
    status.className = `result-status ${result.status}`;
    message.textContent = `${result.passed}/${result.total} 通过 - ${result.message}`;

    details.innerHTML = '';
    if (result.details && result.details.length > 0) {
        details.innerHTML = result.details.map(d => `
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
    } else if (result.error_info) {
        details.innerHTML = `<div class="result-case"><div class="result-case-value result-error">${escapeHtml(result.error_info)}</div></div>`;
    } else if (result.error) {
        details.innerHTML = `<div class="result-case"><div class="result-case-value result-error">${escapeHtml(result.error)}</div></div>`;
    }
}

// 异步判题：提交后轮询详情接口，直到状态离开 PENDING/JUDGING
function pollSubmission(submissionId, attempt, submittedCode) {
    const maxAttempts = 120; // 30s
    pollTimer = setTimeout(async () => {
        let response;
        try {
            response = await getSubmission(submissionId);
        } catch (e) {
            resetRunBtn();
            alert('网络异常，请稍后在提交记录页查看结果');
            return;
        }
        const { status, data } = response;
        if (status !== 200) {
            resetRunBtn();
            alert(data.error || '查询判题结果失败');
            return;
        }
        if (data.status === 'PENDING' || data.status === 'JUDGING') {
            document.getElementById('runBtn').textContent = data.status === 'PENDING' ? '排队中...' : '判题中...';
            if (attempt >= maxAttempts) {
                resetRunBtn();
                alert('判题超时，请稍后在提交记录页查看结果');
                return;
            }
            pollSubmission(submissionId, attempt + 1, submittedCode);
            return;
        }
        resetRunBtn();
        showResult(data);
        // 已提交内容可清理；判题期间继续编辑的内容仍是未提交草稿。
        const currentCode = editor.getValue();
        if (draftSaveTimer) { clearTimeout(draftSaveTimer); draftSaveTimer = null; }
        try {
            if (currentCode === submittedCode) {
                localStorage.removeItem(DRAFT_KEY);
            } else {
                localStorage.setItem(DRAFT_KEY, currentCode);
            }
        } catch (e) { /* 存储不可用不影响判题结果 */ }
    }, 250);
}

function resetRunBtn() {
    const runBtn = document.getElementById('runBtn');
    if (pollTimer) { clearTimeout(pollTimer); pollTimer = null; }
    runBtn.disabled = false;
    runBtn.setAttribute('aria-busy', 'false');
    document.getElementById('resultPanel').setAttribute('aria-busy', 'false');
    runBtn.textContent = '运行代码';
}

// Run button
const runBtn = document.getElementById('runBtn');
if (runBtn) {
    runBtn.addEventListener('click', async () => {
        if (!editor || runBtn.disabled) return;
        const code = editor.getValue();
        if (!code.trim()) {
            alert('请输入代码');
            return;
        }

        runBtn.disabled = true;
        runBtn.setAttribute('aria-busy', 'true');
        runBtn.textContent = '提交中...';
        const resultPanel = document.getElementById('resultPanel');
        resultPanel.classList.add('hidden');
        resultPanel.setAttribute('aria-busy', 'true');
        document.getElementById('resultDetails').innerHTML = '';

        let response;
        try {
            response = await submitCode(code, parseInt(problemId));
        } catch (e) {
            resetRunBtn();
            alert('网络异常，请稍后重试');
            return;
        }
        const { status, data } = response;

        if (status !== 200) {
            resetRunBtn();
            alert(data.error || '提交失败');
            return;
        }

        runBtn.textContent = '判题中...';
        // 保存草稿失败的场景已由 submit 成功覆盖：此时清掉草稿由结果回调处理
        pollSubmission(data.submission_id, 0, code);
    });
}

loadProblem();
