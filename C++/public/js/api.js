const BASE_URL = '/api';

let sessionRedirectStarted = false;

// 页面、静态资源分别带版本；所有动态页面导航都沿用当前发布的页面版本。
function appPageUrl(path) {
    const original = String(path);
    try {
        const current = new URL(window.location.href);
        const target = new URL(original, current);
        const referencePath = original.split(/[?#]/, 1)[0];
        if (!referencePath || target.origin !== current.origin ||
            !/^https?:$/.test(target.protocol) || !target.pathname.toLowerCase().endsWith('.html')) return original;

        const marker = document.querySelector('meta[name="app-version"]');
        const markedVersion = marker ? marker.content : '';
        const queryVersion = current.searchParams.get('v') || '';
        const validVersion = /^[0-9a-f]{16}$/i;
        const version = validVersion.test(markedVersion) ? markedVersion :
            (validVersion.test(queryVersion) ? queryVersion : '');
        if (!version) return original;

        target.searchParams.set('v', version);
        return referencePath + target.search + target.hash;
    } catch (error) {
        return original;
    }
}

function requestFailureMessage(status) {
    if (status === 0) return '无法连接服务器，请确认已启动 OJ 服务后刷新重试。';
    if (status === 401) return '登录状态已失效，请重新登录。';
    if (status === 403) return '当前账号没有执行此操作的权限。';
    if (status === 404) return '接口不存在，请确认已启动最新版本的 OJ 服务。';
    if (status >= 500) return `服务器暂时无法完成请求（HTTP ${status}），请稍后重试。`;
    return `请求未能完成（HTTP ${status}），请重试。`;
}

function isProtectedApiRequest(url) {
    try {
        const target = new URL(url, window.location.href);
        const pathname = target.pathname.replace(/\/+$/, '');
        return target.origin === window.location.origin && pathname.startsWith('/api/') &&
            pathname !== '/api/login' && pathname !== '/api/register';
    } catch (error) {
        return false;
    }
}

function expireLocalSession() {
    try { localStorage.removeItem('user'); } catch (error) { /* 存储被禁用也继续回到登录页。 */ }
    if (sessionRedirectStarted || window.location.pathname.endsWith('/login.html')) return;
    sessionRedirectStarted = true;
    // replace 避免浏览器“返回”后再次落在失效的受保护页面。
    window.location.replace(appPageUrl('login.html'));
}

async function request(url, options = {}) {
    let response;
    try {
        response = await fetch(url, {
            ...options,
            credentials: 'same-origin',
            headers: { 'Content-Type': 'application/json', ...options.headers }
        });
    } catch (error) {
        return { status: 0, data: { error: requestFailureMessage(0) } };
    }

    let data;
    try {
        data = response.status === 204 ? {} : await response.json();
        if (!data || typeof data !== 'object' || Array.isArray(data)) throw new Error('Invalid JSON response');
    } catch (error) {
        const status = response.status >= 400 ? response.status : 502;
        data = {
            error: response.status >= 400 ? requestFailureMessage(status) :
                '服务器返回了非预期的数据，请确认已启动最新版本的 OJ 服务后重试。'
        };
        if (response.status === 401 && isProtectedApiRequest(url)) expireLocalSession();
        return { status, data };
    }

    if (response.status === 401 && isProtectedApiRequest(url)) {
        expireLocalSession();
        data = { ...data, error: requestFailureMessage(401) };
    } else if (response.status >= 400 && (typeof data.error !== 'string' || !data.error.trim())) {
        data = { ...data, error: requestFailureMessage(response.status) };
    }
    return { status: response.status, data };
}

// 把 {key: value} 对象拼成查询串（跳过空值）
function buildQuery(params = {}) {
    const pairs = Object.entries(params)
        .filter(([, v]) => v !== undefined && v !== null && v !== '')
        .map(([k, v]) => `${encodeURIComponent(k)}=${encodeURIComponent(v)}`);
    return pairs.length ? `?${pairs.join('&')}` : '';
}

async function getProblems(params = {}) {
    return request(`${BASE_URL}/problems${buildQuery(params)}`);
}

async function getProblem(id) {
    return request(`${BASE_URL}/problems/${id}`);
}

async function getTags() {
    return request(`${BASE_URL}/tags`);
}

async function submitCode(code, problemId) {
    return request(`${BASE_URL}/submit`, {
        method: 'POST',
        body: JSON.stringify({ code, problem_id: problemId })
    });
}

// 提交详情（含判题结果）；轮询时反复调用直到状态离开 PENDING/JUDGING
async function getSubmission(id) {
    return request(`${BASE_URL}/submissions/${id}`);
}

// 我的提交历史；参数：{problem_id, page, page_size, user_id(admin)}
async function getSubmissions(params = {}) {
    return request(`${BASE_URL}/submissions${buildQuery(params)}`);
}

async function getMyStats() {
    return request(`${BASE_URL}/users/me/stats`);
}

async function getLeaderboard() {
    return request(`${BASE_URL}/leaderboard`);
}

async function registerUser(username, password) {
    return request(`${BASE_URL}/register`, {
        method: 'POST',
        body: JSON.stringify({ username, password })
    });
}

async function loginUser(username, password) {
    return request(`${BASE_URL}/login`, {
        method: 'POST',
        body: JSON.stringify({ username, password })
    });
}

async function logoutUser() {
    return request(`${BASE_URL}/logout`, { method: 'POST' });
}

async function createProblem(problem) {
    return request(`${BASE_URL}/admin/problems`, {
        method: 'POST',
        body: JSON.stringify(problem)
    });
}

async function getProblemAdmin(id) {
    return request(`${BASE_URL}/admin/problems/${id}`);
}

async function updateProblem(id, problem) {
    return request(`${BASE_URL}/admin/problems/${id}`, {
        method: 'PUT',
        body: JSON.stringify(problem)
    });
}

async function deleteProblem(id) {
    return request(`${BASE_URL}/admin/problems/${id}`, {
        method: 'DELETE'
    });
}
