// Run with: node --test tests/frontend/problem_list.test.js
// DOM-contract and workflow checks for the real script; no browser rendering.
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const source = fs.readFileSync(path.join(__dirname, '../../public/js/problem.js'), 'utf8');
const markup = fs.readFileSync(path.join(__dirname, '../../public/problem_list.html'), 'utf8');
const sampleProblem = { id: 7, title: 'A + B', difficulty: 'Easy', created_at: '2026-10-22 10:00:00' };
const flush = () => new Promise(resolve => setImmediate(resolve));

function attributes(text) {
    return Object.fromEntries([...text.matchAll(/([\w-]+)="([^"]*)"/g)].map(match => [match[1], match[2]]));
}

async function createPage(options = {}) {
    const elements = new Map();
    class Element {
        constructor(id = '', attrs = {}) {
            this.id = id;
            this.value = '';
            this.className = attrs.class || '';
            this.attributes = { ...attrs };
            this.dataset = attrs['data-filter'] ? { filter: attrs['data-filter'] } : {};
            this.disabled = false;
            this.listeners = new Map();
            this._html = '';
            this.classList = {
                contains: name => this.className.split(/\s+/).includes(name),
                add: name => {
                    if (!this.classList.contains(name)) this.className = (this.className + ' ' + name).trim();
                },
                remove: name => { this.className = this.className.split(/\s+/).filter(item => item !== name).join(' '); }
            };
        }
        set innerHTML(value) {
            this._html = String(value);
            if (this.id !== 'pagination') return;
            // Replace generated pagination controls as innerHTML would in a DOM.
            elements.delete('prevPage');
            elements.delete('nextPage');
            for (const match of this._html.matchAll(/<button\b([^>]*)>/g)) {
                const attrs = attributes(match[1]);
                const button = new Element(attrs.id, attrs);
                button.disabled = /\bdisabled\b/.test(match[1]);
                elements.set(button.id, button);
            }
        }
        get innerHTML() { return this._html; }
        setAttribute(name, value) { this.attributes[name] = String(value); }
        addEventListener(name, handler) { this.listeners.set(name, handler); }
        async emit(name, event = {}) {
            const handler = this.listeners.get(name);
            if (handler) await handler(event);
            await flush();
        }
    }

    for (const id of ['problemList', 'pagination', 'searchInput']) elements.set(id, new Element(id));
    const filters = [...markup.matchAll(/<button\b([^>]*)>/g)]
        .map(match => attributes(match[1]))
        .filter(attrs => (attrs.class || '').split(/\s+/).includes('filter-btn'))
        .map(attrs => new Element('', attrs));
    const requests = [];
    const responses = [...(options.responses || [])];
    const defaultResponse = options.response || { status: 200, data: { problems: [sampleProblem], total: 1 } };
    const timers = new Map();
    let nextTimer = 0;
    let now = 0;
    const context = vm.createContext({
        appPageUrl: value => value,
        document: {
            getElementById: id => elements.get(id) || null,
            querySelectorAll: selector => selector === '.filter-btn' ? filters : []
        },
        async getProblems(params) {
            requests.push({ ...params });
            const response = responses.length ? responses.shift() : defaultResponse;
            if (response instanceof Error) throw response;
            return response;
        },
        setTimeout(callback, delay) {
            const id = ++nextTimer;
            timers.set(id, { callback, due: now + delay });
            return id;
        },
        clearTimeout: id => timers.delete(id)
    });
    await vm.runInContext(source, context, { filename: 'problem.js' });

    return {
        elements, filters, requests, responses, timers,
        filter(value) { return filters.find(button => button.dataset.filter === value); },
        async advance(milliseconds) {
            const target = now + milliseconds;
            for (;;) {
                const next = [...timers].filter(([, timer]) => timer.due <= target)
                    .sort((a, b) => a[1].due - b[1].due)[0];
                if (!next) break;
                now = next[1].due;
                timers.delete(next[0]);
                await next[1].callback();
                await flush();
            }
            now = target;
        },
        async reload() { await vm.runInContext('loadProblems()', context); }
    };
}

test('initial load renders four columns and the original problem route using server pagination', async () => {
    const page = await createPage();
    assert.deepEqual(page.requests, [{ page: 1, page_size: 20 }]);
    const html = page.elements.get('problemList').innerHTML;
    assert.match(html, /href="problem\.html\?id=7" class="problem-card"/);
    const columns = [...html.matchAll(/<span class="([^"]+)"/g)].map(match => match[1]);
    assert.deepEqual(columns, ['problem-number', 'problem-card-title', 'difficulty-tag easy', 'problem-card-meta']);
    assert.match(html, />#7<\/span>/);
    assert.match(html, />A \+ B<\/span>/);
    assert.match(html, />简单<\/span>/);
    assert.match(html, />2026-10-22 10:00:00<\/span>/);
    assert.equal(page.elements.get('pagination').innerHTML, '');
    assert.equal(page.filter('all').attributes['aria-pressed'], 'true');
});

test('long titles remain complete while text and route parameters are escaped', async () => {
    const longTitle = '需要完整保留的长题目标题'.repeat(35);
    const page = await createPage({ response: { status: 200, data: { total: 1, problems: [{
        id: '7 &/#?"', title: longTitle + ' <img src=x> & "quoted" \'single\'',
        difficulty: 'Hard', created_at: '<time> & "date"'
    }] } } });
    const html = page.elements.get('problemList').innerHTML;
    assert.ok(html.includes(longTitle));
    assert.ok(html.includes('href="problem.html?id=' + encodeURIComponent('7 &/#?"') + '"'));
    assert.match(html, /#7 &amp;\/#\?&quot;/);
    assert.match(html, /&lt;img src=x&gt; &amp; &quot;quoted&quot; &#39;single&#39;/);
    assert.match(html, /&lt;time&gt; &amp; &quot;date&quot;/);
    assert.match(html, /difficulty-tag hard">困难/);
    assert.doesNotMatch(html, /<img|<time>/);
});

test('difficulty selection resets pagination and keeps active and aria-pressed states aligned', async () => {
    const page = await createPage({ response: { status: 200, data: { problems: [sampleProblem], total: 41 } } });
    await page.elements.get('nextPage').emit('click');
    assert.equal(page.requests.at(-1).page, 2);
    await page.filter('Medium').emit('click');
    assert.deepEqual(page.requests.at(-1), { page: 1, page_size: 20, difficulty: 'Medium' });
    for (const button of page.filters) {
        const selected = button.dataset.filter === 'Medium';
        assert.equal(button.classList.contains('active'), selected);
        assert.equal(button.attributes['aria-pressed'], String(selected));
    }
    await page.filter('all').emit('click');
    assert.deepEqual(page.requests.at(-1), { page: 1, page_size: 20 });
    assert.equal(page.filter('all').attributes['aria-pressed'], 'true');
    assert.equal(page.filter('Medium').attributes['aria-pressed'], 'false');
});

test('typing is debounced for 400 ms and rapid input only requests the latest trimmed title', async () => {
    const page = await createPage({ response: { status: 200, data: { problems: [sampleProblem], total: 41 } } });
    await page.elements.get('nextPage').emit('click');
    const search = page.elements.get('searchInput');
    search.value = 'A';
    await search.emit('input');
    await page.advance(250);
    assert.equal(page.requests.length, 2);
    search.value = '  A + B  ';
    await search.emit('input');
    assert.equal(page.timers.size, 1);
    await page.advance(399);
    assert.equal(page.requests.length, 2);
    await page.advance(1);
    assert.deepEqual(page.requests.at(-1), { page: 1, page_size: 20, keyword: 'A + B' });
    assert.equal(page.requests.length, 3);
    assert.equal(page.timers.size, 0);
});

test('Enter searches immediately, cancels pending debounce, and clearing whitespace removes the keyword', async () => {
    const page = await createPage();
    const search = page.elements.get('searchInput');
    search.value = '  二分查找  ';
    await search.emit('input');
    await search.emit('keydown', { key: 'Escape' });
    assert.equal(page.requests.length, 1);
    await search.emit('keydown', { key: 'Enter' });
    assert.deepEqual(page.requests.at(-1), { page: 1, page_size: 20, keyword: '二分查找' });
    assert.equal(page.timers.size, 0);
    await page.advance(1000);
    assert.equal(page.requests.length, 2);
    search.value = ' \t ';
    await search.emit('input');
    await page.advance(400);
    assert.deepEqual(page.requests.at(-1), { page: 1, page_size: 20 });
});

test('keyword and difficulty filters remain combined when moving between pages', async () => {
    const page = await createPage({ response: { status: 200, data: { problems: [sampleProblem], total: 41 } } });
    await page.filter('Hard').emit('click');
    const search = page.elements.get('searchInput');
    search.value = '图';
    await search.emit('keydown', { key: 'Enter' });
    await page.elements.get('nextPage').emit('click');
    assert.deepEqual(page.requests.at(-1), { page: 2, page_size: 20, difficulty: 'Hard', keyword: '图' });
});

test('empty data and HTTP or invalid-response failures render their respective states', async () => {
    const empty = await createPage({ response: { status: 200, data: { problems: [], total: 0 } } });
    assert.match(empty.elements.get('problemList').innerHTML, /class="empty-state">暂无题目/);
    assert.equal(empty.elements.get('pagination').innerHTML, '');
    for (const [response, expected] of [
        [{ status: 401, data: { error: 'unauthorized' } }, /登录状态已失效.*重新登录/],
        [{ status: 403, data: {} }, /没有查看题库的权限/],
        [{ status: 404, data: {} }, /题库接口不存在.*最新版本/],
        [{ status: 500, data: { error: 'internal server error' } }, /HTTP 500.*服务器内部错误/],
        [{ status: 0, data: {} }, /无法连接题库服务.*启动 OJ 服务/],
        [{ status: 200, data: {} }, /题库数据格式不正确/],
        [{ status: 200, data: { problems: null } }, /题库数据格式不正确/],
        [{ status: 200, data: { problems: [null] } }, /题库数据格式不正确/]
    ]) {
        const failed = await createPage({ response });
        const html = failed.elements.get('problemList').innerHTML;
        assert.match(html, /class="empty-state" role="alert"/);
        assert.match(html, expected);
        assert.doesNotMatch(html, /加载中|problem-card/);
        assert.equal(failed.elements.get('pagination').innerHTML, '');
    }
});

test('first and last pages disable the appropriate controls and never request out-of-range pages', async () => {
    const page = await createPage({ response: { status: 200, data: { problems: [sampleProblem], total: 41 } } });
    assert.match(page.elements.get('pagination').innerHTML, /第 1 \/ 3 页（共 41 题）/);
    assert.equal(page.elements.get('prevPage').disabled, true);
    assert.equal(page.elements.get('nextPage').disabled, false);
    // Dispatching directly also exercises the script's boundary guard.
    await page.elements.get('prevPage').emit('click');
    assert.equal(page.requests.length, 1);
    await page.elements.get('nextPage').emit('click');
    assert.equal(page.requests.at(-1).page, 2);
    assert.equal(page.elements.get('prevPage').disabled, false);
    assert.equal(page.elements.get('nextPage').disabled, false);
    await page.elements.get('nextPage').emit('click');
    assert.equal(page.requests.at(-1).page, 3);
    assert.match(page.elements.get('pagination').innerHTML, /第 3 \/ 3 页（共 41 题）/);
    assert.equal(page.elements.get('prevPage').disabled, false);
    assert.equal(page.elements.get('nextPage').disabled, true);
    await page.elements.get('nextPage').emit('click');
    assert.equal(page.requests.length, 3);
    await page.elements.get('prevPage').emit('click');
    assert.equal(page.requests.at(-1).page, 2);
});

test('pagination disappears at the one-page boundary and appears when total exceeds 20', async () => {
    const page = await createPage({ response: { status: 200, data: { problems: [sampleProblem], total: 20 } } });
    assert.equal(page.elements.get('pagination').innerHTML, '');
    page.responses.push({ status: 200, data: { problems: [sampleProblem], total: 21 } });
    await page.reload();
    assert.match(page.elements.get('pagination').innerHTML, /第 1 \/ 2 页（共 21 题）/);
    page.responses.push({ status: 200, data: { problems: [], total: 0 } });
    await page.reload();
    assert.equal(page.elements.get('pagination').innerHTML, '');
    assert.equal(page.elements.has('prevPage'), false);
    assert.equal(page.elements.has('nextPage'), false);
});

test('a failed reload clears old pagination and safely displays diagnostic text', async () => {
    const page = await createPage({ response: { status: 200, data: { problems: [sampleProblem], total: 41 } } });
    assert.ok(page.elements.has('nextPage'));
    page.responses.push({ status: 503, data: { error: '<img src=x onerror="alert(1)"> & 数据库不可用' } });
    await page.reload();
    const html = page.elements.get('problemList').innerHTML;
    assert.match(html, /HTTP 503/);
    assert.match(html, /&lt;img src=x onerror=&quot;alert\(1\)&quot;&gt; &amp; 数据库不可用/);
    assert.doesNotMatch(html, /<img/);
    assert.equal(page.elements.get('pagination').innerHTML, '');
    assert.equal(page.elements.has('nextPage'), false);
});

test('a rejected request stops loading with a connection message and allows a later reload', async () => {
    const page = await createPage({ responses: [new Error('offline')] });
    assert.match(page.elements.get('problemList').innerHTML, /无法连接题库服务/);
    assert.doesNotMatch(page.elements.get('problemList').innerHTML, /加载中/);
    await page.reload();
    assert.match(page.elements.get('problemList').innerHTML, /problem-card-title">A \+ B/);
});
