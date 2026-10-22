// Run with: node --test tests/frontend/auth.test.js
// Checks login state and form contracts without browser automation.
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const source = fs.readFileSync(path.join(__dirname, '../../public/js/auth.js'), 'utf8');
const encode = value => String(value).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
const cachedUser = JSON.stringify({ username: 'student', role: 'user' });

function createPage(options = {}) {
    const elements = new Map();
    class Element {
        constructor(id = '') {
            this.id = id;
            this.value = '';
            this.className = '';
            this.listeners = new Map();
            this.children = [];
            this._text = '';
            this.classList = {
                contains: name => this.className.split(/\s+/).includes(name),
                toggle: (name, force) => {
                    const selected = force === undefined ? !this.classList.contains(name) : force;
                    this.className = this.className.split(/\s+/).filter(item => item && item !== name).join(' ');
                    if (selected) this.className = (this.className + ' ' + name).trim();
                }
            };
        }
        set textContent(value) { this._text = String(value); this.children = []; }
        get textContent() { return this._text + this.children.map(child => child.textContent).join(''); }
        get innerHTML() { return encode(this._text) + this.children.map(child => child.innerHTML).join(''); }
        appendChild(child) { this.children.push(child); }
        addEventListener(name, callback) { this.listeners.set(name, callback); }
        async emit(name, event = {}) {
            const callback = this.listeners.get(name);
            if (callback) await callback({ preventDefault() {}, ...event });
        }
    }
    const pathname = options.pathname || '/login.html';
    const ids = ['username', 'password', 'confirmPassword', 'loginError', 'registerError'];
    if (pathname.endsWith('/login.html')) ids.push('loginForm');
    if (pathname.endsWith('/register.html')) ids.push('registerForm');
    if (options.header) ids.push('userInfo', 'adminLink', 'logoutBtn');
    for (const id of ids) elements.set(id, new Element(id));
    if (elements.has('adminLink')) elements.get('adminLink').className = 'admin-link hidden';
    const storage = new Map();
    if (options.storedUser !== undefined) storage.set('user', options.storedUser);
    const requests = { login: [], register: [], logout: 0 };
    const alerts = [];
    const window = { location: { pathname, href: 'http://localhost:8080' + pathname } };
    const context = vm.createContext({
        window,
        appPageUrl: value => value,
        document: { getElementById: id => elements.get(id) || null, createElement: () => new Element() },
        localStorage: {
            getItem(key) { if (options.readFails) throw new Error('storage unavailable'); return storage.get(key) ?? null; },
            setItem(key, value) { if (options.writeFails) throw new Error('storage unavailable'); storage.set(key, value); },
            removeItem(key) { if (options.removeFails) throw new Error('storage unavailable'); storage.delete(key); }
        },
        async loginUser(username, password) {
            requests.login.push({ username, password });
            if (options.loginError) throw new Error('offline');
            return options.loginResponse || { status: 200, data: { message: 'login successful', username: 'student', role: 'user', session_id: 'private-session' } };
        },
        async registerUser(username, password) {
            requests.register.push({ username, password });
            if (options.registerError) throw new Error('offline');
            return options.registerResponse || { status: 201, data: { message: 'user registered successfully' } };
        },
        async logoutUser() { requests.logout++; if (options.logoutError) throw new Error('offline'); },
        alert: message => alerts.push(message)
    });
    vm.runInContext(source, context, { filename: 'auth.js' });
    return { elements, storage, requests, alerts, window, context };
}

test('a local cache never redirects login or register pages before a successful server login', () => {
    for (const pathname of ['/login.html', '/register.html']) {
        const page = createPage({ pathname, storedUser: cachedUser });
        assert.equal(page.window.location.href, 'http://localhost:8080' + pathname);
        assert.equal(page.storage.get('user'), cachedUser);
        assert.equal(page.requests.login.length, 0);
    }
});

test('missing, corrupt, or invalid cached users cannot enter protected pages', () => {
    for (const storedUser of [undefined, '{', 'null', '[]', '{}', JSON.stringify({ username: '', role: 'user' }), JSON.stringify({ username: 'x', role: 'invalid' })]) {
        const page = createPage({ pathname: '/problem_list.html', storedUser });
        assert.equal(page.window.location.href, 'login.html');
        assert.equal(page.storage.has('user'), false);
    }
    const unavailable = createPage({ pathname: '/problem_list.html', readFails: true, removeFails: true });
    assert.equal(unavailable.window.location.href, 'login.html');
});

test('public pages stay accessible without a cache, including malformed cached data', () => {
    for (const pathname of ['/', '/index.html', '/login.html', '/register.html']) {
        const page = createPage({ pathname, storedUser: '{' });
        assert.equal(page.window.location.href, 'http://localhost:8080' + pathname);
    }
});

test('header user information is text and admin visibility follows the cached role', () => {
    const admin = createPage({ pathname: '/admin.html', header: true, storedUser: JSON.stringify({ username: '<img src=x>', role: 'admin' }) });
    assert.equal(admin.elements.get('adminLink').classList.contains('hidden'), false);
    assert.equal(admin.elements.get('userInfo').children[0].className, 'admin-badge');
    assert.match(admin.elements.get('userInfo').innerHTML, /&lt;img src=x&gt;/);
    assert.doesNotMatch(admin.elements.get('userInfo').innerHTML, /<img/);
    const student = createPage({ pathname: '/problem_list.html', header: true, storedUser: cachedUser });
    assert.equal(student.elements.get('adminLink').classList.contains('hidden'), true);
    assert.equal(student.elements.get('userInfo').textContent, 'student');
});

test('successful login trims the username, preserves password bytes, and keeps the problem-list route', async () => {
    const page = createPage();
    page.elements.get('username').value = '  student  ';
    page.elements.get('password').value = ' secret ';
    await page.elements.get('loginForm').emit('submit');
    assert.deepEqual(page.requests.login, [{ username: 'student', password: ' secret ' }]);
    assert.deepEqual(JSON.parse(page.storage.get('user')), { username: 'student', role: 'user' });
    assert.equal(page.window.location.href, 'problem_list.html');
    assert.equal(page.elements.get('loginError').textContent, '');
});

test('wrong credentials stay on login and preserve any cache without a redirect loop', async () => {
    const page = createPage({ storedUser: cachedUser, loginResponse: { status: 401, data: { error: 'invalid username or password' } } });
    await page.elements.get('loginForm').emit('submit');
    assert.match(page.elements.get('loginError').textContent, /用户名或密码错误/);
    assert.equal(page.window.location.href, 'http://localhost:8080/login.html');
    assert.equal(page.storage.get('user'), cachedUser);
});

test('login request failures and disabled storage produce actionable text without navigating', async () => {
    for (const options of [
        { loginResponse: { status: 0, data: { error: '无法连接服务器，请启动 OJ 服务。' } } },
        { loginError: true },
        { writeFails: true }
    ]) {
        const page = createPage(options);
        await page.elements.get('loginForm').emit('submit');
        assert.match(page.elements.get('loginError').textContent, /无法连接服务器|本地存储/);
        assert.equal(page.window.location.href, 'http://localhost:8080/login.html');
    }
    const escaped = createPage({ loginResponse: { status: 500, data: { error: '<img src=x onerror=alert(1)>' } } });
    await escaped.elements.get('loginForm').emit('submit');
    assert.match(escaped.elements.get('loginError').innerHTML, /&lt;img/);
    assert.doesNotMatch(escaped.elements.get('loginError').innerHTML, /<img/);
});

test('registration validates confirmation before requesting and keeps the login route after success', async () => {
    const page = createPage({ pathname: '/register.html' });
    page.elements.get('username').value = '  learner  ';
    page.elements.get('password').value = 'secret';
    page.elements.get('confirmPassword').value = 'different';
    await page.elements.get('registerForm').emit('submit');
    assert.equal(page.requests.register.length, 0);
    assert.match(page.elements.get('registerError').textContent, /两次输入的密码不一致/);
    page.elements.get('confirmPassword').value = 'secret';
    await page.elements.get('registerForm').emit('submit');
    assert.deepEqual(page.requests.register, [{ username: 'learner', password: 'secret' }]);
    assert.deepEqual(page.alerts, ['注册成功，请登录']);
    assert.equal(page.window.location.href, 'login.html');
});

test('registration network and server errors stay visible on the registration page', async () => {
    for (const options of [
        { registerError: true },
        { registerResponse: { status: 409, data: { error: '用户名已存在' } } }
    ]) {
        const page = createPage({ pathname: '/register.html', ...options });
        await page.elements.get('registerForm').emit('submit');
        assert.match(page.elements.get('registerError').textContent, /无法连接服务器|用户名已存在/);
        assert.equal(page.window.location.href, 'http://localhost:8080/register.html');
    }
});

test('logout clears the cache and returns to login even if the server request fails', async () => {
    for (const logoutError of [false, true]) {
        const page = createPage({ pathname: '/problem_list.html', storedUser: cachedUser, header: true, logoutError });
        await page.elements.get('logoutBtn').emit('click');
        assert.equal(page.storage.has('user'), false);
        assert.equal(page.requests.logout, 1);
        assert.equal(page.window.location.href, 'login.html');
    }
});
