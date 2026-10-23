// Run with: node --test tests/frontend/api.test.js
// Exercise the real API wrapper with mocked HTTP responses; no browser required.
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const source = fs.readFileSync(path.join(__dirname, '../../public/js/api.js'), 'utf8');
const snapshot = value => JSON.parse(JSON.stringify(value));

function createApi(options = {}) {
    const pageUrl = new URL(options.href || 'http://localhost:8080/problem_list.html');
    const initialUser = JSON.stringify({ username: 'student', role: 'user' });
    const storage = new Map([['user', initialUser], ['unrelated', 'preserve']]);
    const removals = [];
    const redirects = [];
    const requests = [];
    const responses = [...(options.responses || [])];
    const context = vm.createContext({
        URL,
        document: { querySelector: () => options.metaVersion ? { content: options.metaVersion } : null },
        window: {
            location: {
                href: pageUrl.href,
                origin: pageUrl.origin,
                pathname: pageUrl.pathname,
                replace: target => redirects.push(target)
            }
        },
        localStorage: {
            removeItem(key) {
                removals.push(key);
                if (options.storageError) throw new Error('Storage is disabled');
                storage.delete(key);
            }
        },
        async fetch(url, requestOptions) {
            requests.push({ url, options: snapshot(requestOptions) });
            if (options.fetchError) throw new TypeError('Failed to fetch');
            const response = responses.length ? responses.shift() : (options.response || { status: 200, data: { message: 'ok' } });
            return {
                status: response.status,
                async json() {
                    if (response.jsonError) throw new SyntaxError('Unexpected token <');
                    return response.data;
                }
            };
        }
    });
    vm.runInContext(source, context, { filename: 'api.js' });
    return { context, requests, storage, initialUser, removals, redirects };
}

test('JSON success preserves data and merges custom headers with same-origin credentials', async () => {
    const expected = { problems: [{ id: 7, title: 'A + B' }], total: 1 };
    const api = createApi({ response: { status: 200, data: expected } });
    const result = await api.context.request('/api/problems', {
        method: 'POST', headers: { 'X-Request-Id': 'sample' }, credentials: 'omit', body: '{"sample":true}'
    });
    assert.deepEqual(snapshot(result), { status: 200, data: expected });
    assert.deepEqual(api.requests, [{ url: '/api/problems', options: {
        method: 'POST', headers: { 'Content-Type': 'application/json', 'X-Request-Id': 'sample' },
        credentials: 'same-origin', body: '{"sample":true}'
    } }]);
    assert.deepEqual(api.removals, []);
    assert.deepEqual(api.redirects, []);
});

test('query parameters are encoded and empty values are skipped without losing zero', async () => {
    const api = createApi();
    await api.context.getProblems({ page: 2, page_size: 20, keyword: '图 & A/B', difficulty: 'Hard', empty: '', missing: undefined, absent: null, offset: 0 });
    const target = new URL(api.requests[0].url, 'http://localhost:8080');
    assert.equal(target.pathname, '/api/problems');
    assert.deepEqual(Object.fromEntries(target.searchParams), {
        page: '2', page_size: '20', keyword: '图 & A/B', difficulty: 'Hard', offset: '0'
    });
    assert.match(api.requests[0].url, /%E5%9B%BE/);
    assert.match(api.requests[0].url, /%26/);
    assert.match(api.requests[0].url, /%2F/);
    assert.equal(api.requests[0].options.credentials, 'same-origin');
});

test('login and registration keep POST payloads and successful login data intact', async () => {
    const data = { message: 'login successful', username: 'student', role: 'user', session_id: 'server-cookie-token' };
    const api = createApi({ responses: [{ status: 200, data }, { status: 201, data: { message: 'user registered successfully' } }] });
    assert.deepEqual(snapshot(await api.context.loginUser('student', 'p@ss"word')), { status: 200, data });
    assert.equal(api.requests[0].url, '/api/login');
    assert.equal(api.requests[0].options.method, 'POST');
    assert.deepEqual(JSON.parse(api.requests[0].options.body), { username: 'student', password: 'p@ss"word' });
    assert.equal((await api.context.registerUser('new student', 'secret')).status, 201);
    assert.equal(api.requests[1].url, '/api/register');
    assert.equal(api.requests[1].options.method, 'POST');
    assert.deepEqual(JSON.parse(api.requests[1].options.body), { username: 'new student', password: 'secret' });
    assert.deepEqual(api.removals, []);
    assert.deepEqual(api.redirects, []);
});

test('network failures resolve to status zero with an actionable Chinese error', async () => {
    const api = createApi({ fetchError: true });
    const result = await api.context.getProblems();
    assert.equal(result.status, 0);
    assert.match(result.data.error, /无法连接服务器/);
    assert.match(result.data.error, /启动 OJ 服务/);
    assert.equal(api.storage.get('user'), api.initialUser);
    assert.deepEqual(api.redirects, []);
});

test('non-JSON and non-object success responses resolve to 502 while 204 is supported', async () => {
    for (const response of [
        { status: 200, jsonError: true },
        { status: 201, data: null },
        { status: 200, data: [] },
        { status: 200, data: 'unexpected text' },
        { status: 200, data: true }
    ]) {
        const api = createApi({ response });
        const result = await api.context.getProblems();
        assert.equal(result.status, 502);
        assert.match(result.data.error, /服务器返回了非预期的数据/);
        assert.deepEqual(api.removals, []);
        assert.deepEqual(api.redirects, []);
    }
    const noContent = createApi({ response: { status: 204, jsonError: true } });
    assert.deepEqual(snapshot(await noContent.context.logoutUser()), { status: 204, data: {} });
});

test('HTTP errors retain their status and always supply a string error without exposing response HTML', async () => {
    for (const response of [
        { status: 404, jsonError: true },
        { status: 500, data: {} },
        { status: 503, data: { error: { internal: '<html>proxy</html>' } } },
        { status: 429, data: { error: '' } }
    ]) {
        const api = createApi({ response });
        const result = await api.context.getProblems();
        assert.equal(result.status, response.status);
        assert.equal(typeof result.data.error, 'string');
        assert.match(result.data.error, /[\u4e00-\u9fff]/);
        assert.doesNotMatch(result.data.error, /<html>/);
        assert.deepEqual(api.redirects, []);
    }
    const explicit = createApi({ response: { status: 400, data: { error: 'invalid json', detail: 'request body' } } });
    assert.deepEqual(snapshot(await explicit.context.getProblems()), { status: 400, data: { error: 'invalid json', detail: 'request body' } });
});

test('protected API 401 clears only the user cache and redirects to login once for concurrent failures', async () => {
    const api = createApi({ response: { status: 401, data: { error: 'unauthorized' } } });
    const results = await Promise.all([api.context.getProblems(), api.context.getLeaderboard()]);
    for (const result of results) {
        assert.equal(result.status, 401);
        assert.match(result.data.error, /登录状态已失效，请重新登录/);
    }
    assert.equal(api.storage.has('user'), false);
    assert.equal(api.storage.get('unrelated'), 'preserve');
    assert.ok(api.removals.length >= 1);
    assert.ok(api.removals.every(key => key === 'user'));
    assert.deepEqual(api.redirects, ['login.html']);
});

test('protected non-JSON 401 and unavailable local storage still return 401 and navigate to login', async () => {
    for (const response of [{ status: 401, jsonError: true }, { status: 401, data: { error: 'unauthorized' } }]) {
        const api = createApi({ response, storageError: true });
        const result = await api.context.request('http://localhost:8080/api/submissions/7?view=detail');
        assert.equal(result.status, 401);
        assert.match(result.data.error, /登录状态已失效/);
        assert.deepEqual(api.removals, ['user']);
        assert.deepEqual(api.redirects, ['login.html']);
    }
});

test('login and registration 401 responses never clear local identity or force a redirect', async () => {
    for (const url of ['/api/login', '/api/register', '/api/login?attempt=2', '/api/register?ref=home', '/api/login/']) {
        for (const response of [{ status: 401, data: { error: 'invalid username or password' } }, { status: 401, jsonError: true }]) {
            const api = createApi({ response });
            const result = await api.context.request(url, { method: 'POST' });
            assert.equal(result.status, 401);
            assert.equal(typeof result.data.error, 'string');
            assert.equal(api.storage.get('user'), api.initialUser);
            assert.deepEqual(api.removals, []);
            assert.deepEqual(api.redirects, []);
        }
    }
});

test('cross-origin and non-API 401 responses and protected 403 responses preserve login state', async () => {
    for (const url of ['https://external.example/api/problems', '//external.example/api/problems', '/health', '/apiary/problems']) {
        const api = createApi({ response: { status: 401, data: { error: 'unauthorized' } } });
        assert.equal((await api.context.request(url)).status, 401);
        assert.equal(api.storage.get('user'), api.initialUser);
        assert.deepEqual(api.removals, []);
        assert.deepEqual(api.redirects, []);
    }
    const forbidden = createApi({ response: { status: 403, data: {} } });
    const result = await forbidden.context.getProblems();
    assert.equal(result.status, 403);
    assert.match(result.data.error, /没有.*权限/);
    assert.equal(forbidden.storage.get('user'), forbidden.initialUser);
    assert.deepEqual(forbidden.removals, []);
    assert.deepEqual(forbidden.redirects, []);
});

test('a protected 401 received while already on login clears stale identity without navigating again', async () => {
    const api = createApi({ href: 'http://localhost:8080/login.html', response: { status: 401, data: { error: 'unauthorized' } } });
    assert.equal((await api.context.getProblems()).status, 401);
    assert.equal(api.storage.has('user'), false);
    assert.deepEqual(api.redirects, []);
});
