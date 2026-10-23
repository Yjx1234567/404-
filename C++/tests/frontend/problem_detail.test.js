// Run with: node --test tests/frontend/problem_detail.test.js
// These are DOM-contract and workflow checks; they do not render a browser.
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const source = fs.readFileSync(path.join(__dirname, '../../public/js/problem_detail.js'), 'utf8');
const encode = text => String(text).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');

async function createPage(options = {}) {
    const elements = new Map();
    class Element {
        constructor(id = '') {
            this.id = id;
            this.className = '';
            this.value = '';
            this.disabled = false;
            this.attributes = {};
            this.listeners = new Map();
            this.children = [];
            this._text = '';
            this._html = null;
            this.classList = {
                contains: name => this.className.split(/\s+/).includes(name),
                add: name => { if (!this.classList.contains(name)) this.className = (this.className + ' ' + name).trim(); },
                remove: name => { this.className = this.className.split(/\s+/).filter(item => item !== name).join(' '); }
            };
        }
        set textContent(value) { this._text = String(value); this._html = null; }
        get textContent() { return this._text; }
        set innerHTML(value) { this._html = String(value); this._text = ''; }
        get innerHTML() { return this._html === null ? encode(this._text) : this._html; }
        setAttribute(name, value) { this.attributes[name] = String(value); }
        appendChild(child) { this.children.push(child); if (child.id) elements.set(child.id, child); }
        addEventListener(name, handler) { this.listeners.set(name, handler); }
        async emit(name, event = {}) { if (this.listeners.has(name)) await this.listeners.get(name)(event); }
    }
    for (const id of ['problemTitle', 'problemDifficulty', 'problemContent', 'testCases', 'editor', 'runBtn', 'resultPanel', 'resultStatus', 'resultMessage', 'resultDetails']) {
        elements.set(id, new Element(id));
    }
    elements.get('resultPanel').className = 'result-panel hidden';
    const document = {
        getElementById: id => elements.get(id) || null,
        createElement: () => new Element()
    };
    const timers = new Map();
    let timerId = 0;
    const storage = new Map(Object.entries(options.storage || {}));
    const alerts = [];
    const requests = { problems: [], submits: [], submissions: [] };
    const resizeObservers = [];
    const windowListeners = new Map();
    const editor = {
        value: '', resizeCalls: 0, change: null,
        setTheme(value) { this.theme = value; },
        setFontSize(value) { this.fontSize = value; },
        setValue(value) { this.value = value; },
        getValue() { return this.value; },
        resize() { this.resizeCalls++; },
        session: {
            setMode(value) { editor.mode = value; },
            on(name, handler) { if (name === 'change') editor.change = handler; }
        }
    };
    const ace = { edit: () => editor };
    const window = {
        location: { search: options.search === undefined ? '?id=7' : options.search },
        addEventListener: (name, handler) => windowListeners.set(name, handler)
    };
    if (options.ace !== false) window.ace = ace;
    if (options.aceFailed) window.__aceFailed = true;
    const defaultProblem = { id: 7, title: 'A + B', difficulty: 'Easy', content: '计算两个整数的和。', template: 'int main() { return 0; }', test_cases: [{ input: '1 2', expected: '3' }] };
    const submissionResponses = [...(options.submissionResponses || [])];
    const context = vm.createContext({
        window, document, ace, URLSearchParams,
        ResizeObserver: options.resizeObserver === false ? undefined : class {
            constructor(callback) { this.callback = callback; resizeObservers.push(this); }
            observe(element) { this.element = element; }
        },
        localStorage: {
            getItem(key) { if (options.storageReadFails) throw new Error('Storage unavailable'); return storage.get(key) ?? null; },
            setItem: (key, value) => storage.set(key, value),
            removeItem: key => storage.delete(key)
        },
        setTimeout(callback, delay) { const id = ++timerId; timers.set(id, { callback, delay }); return id; },
        clearTimeout: id => timers.delete(id),
        alert: message => alerts.push(message),
        async getProblem(id) {
            requests.problems.push(id);
            if (options.problemError) throw new Error('offline');
            return options.problemResponse || { status: 200, data: defaultProblem };
        },
        async submitCode(code, id) {
            requests.submits.push({ code, id });
            if (options.submitError) throw new Error('offline');
            return options.submitResponse || { status: 200, data: { submission_id: 42, status: 'PENDING' } };
        },
        async getSubmission(id) {
            requests.submissions.push(id);
            const next = submissionResponses.shift();
            if (next instanceof Error) throw next;
            return next || { status: 200, data: { status: 'AC', passed: 1, total: 1, message: '通过', details: [] } };
        }
    });
    await vm.runInContext(source, context, { filename: 'problem_detail.js' });
    return {
        elements, editor, storage, alerts, requests, resizeObservers, windowListeners, timers, context,
        async tick(delay) {
            const item = [...timers].find(([, timer]) => timer.delay === delay);
            assert.ok(item, `Expected a ${delay} ms timer`);
            timers.delete(item[0]);
            await item[1].callback();
        },
        result(data) { context.__result = data; vm.runInContext('showResult(__result)', context); }
    };
}

test('Ace restores the per-problem draft and resizes for viewport and container changes', async () => {
    const page = await createPage({ storage: { oj_draft_problem_7: 'draft code' } });
    assert.equal(page.editor.value, 'draft code');
    assert.equal(page.editor.theme, 'ace/theme/monokai');
    assert.equal(page.editor.mode, 'ace/mode/c_cpp');
    assert.equal(page.editor.resizeCalls, 1);
    page.windowListeners.get('resize')();
    page.resizeObservers[0].callback();
    assert.equal(page.editor.resizeCalls, 3);
    assert.equal(page.resizeObservers[0].element.id, 'editor');
});

test('Ace still resizes on window changes when ResizeObserver is unavailable', async () => {
    const page = await createPage({ resizeObserver: false });
    assert.equal(page.resizeObservers.length, 0);
    page.windowListeners.get('resize')();
    assert.equal(page.editor.resizeCalls, 2);
});

test('textarea fallback keeps its CSS and accessibility contract and saves a draft', async () => {
    const page = await createPage({ ace: false });
    const fallback = page.elements.get('editorFallback');
    assert.equal(fallback.className, 'editor-fallback');
    assert.equal(fallback.attributes['aria-label'], 'C++ 代码编辑器');
    assert.equal(fallback.attributes.spellcheck, 'false');
    assert.equal(fallback.value, 'int main() { return 0; }');
    fallback.value = 'int main() { return 1; }';
    await fallback.emit('input');
    await page.tick(500);
    assert.equal(page.storage.get('oj_draft_problem_7'), fallback.value);
});

test('a failed Ace load and unavailable draft storage still allow code entry', async () => {
    const page = await createPage({ aceFailed: true, storageReadFails: true });
    assert.equal(page.elements.get('editorFallback').value, 'int main() { return 0; }');
    assert.equal(page.editor.resizeCalls, 0);
});

test('an invalid or missing problem leaves the editor inactive', async () => {
    const missing = await createPage({ search: '' });
    assert.equal(missing.elements.get('problemTitle').textContent, '无效的题目 ID');
    assert.equal(missing.requests.problems.length, 0);
    await missing.elements.get('runBtn').emit('click');
    assert.equal(missing.requests.submits.length, 0);
    const failed = await createPage({ problemResponse: { status: 404, data: { error: '题目不存在' } } });
    assert.equal(failed.elements.get('problemTitle').textContent, '题目不存在');
});

test('a rejected problem request replaces the loading state and leaves submission inactive', async () => {
    const page = await createPage({ problemError: true });
    assert.equal(page.elements.get('problemTitle').textContent, '题目加载失败，请稍后重试');
    assert.equal(page.editor.resizeCalls, 0);
    await page.elements.get('runBtn').emit('click');
    assert.equal(page.requests.submits.length, 0);
});

test('whitespace-only code is rejected without changing busy state or submitting', async () => {
    const page = await createPage();
    page.editor.value = ' \n\t ';
    await page.elements.get('runBtn').emit('click');
    assert.equal(page.requests.submits.length, 0);
    assert.equal(page.elements.get('runBtn').disabled, false);
    assert.deepEqual(page.alerts, ['请输入代码']);
});

test('submission failure and thrown network errors restore the run action', async () => {
    for (const options of [{ submitResponse: { status: 429, data: { error: '提交过于频繁' } } }, { submitError: true }]) {
        const page = await createPage(options);
        await page.elements.get('runBtn').emit('click');
        assert.equal(page.elements.get('runBtn').disabled, false);
        assert.equal(page.elements.get('runBtn').textContent, '运行代码');
        assert.equal(page.elements.get('runBtn').attributes['aria-busy'], 'false');
        assert.equal(page.elements.get('resultPanel').attributes['aria-busy'], 'false');
        assert.equal(page.alerts.length, 1);
        assert.equal(page.timers.size, 0);
    }
});

test('PENDING and JUDGING keep the action busy until an AC result clears the draft', async () => {
    const page = await createPage({
        storage: { oj_draft_problem_7: 'int main() {}' },
        submissionResponses: [
            { status: 200, data: { status: 'PENDING' } },
            { status: 200, data: { status: 'JUDGING' } },
            { status: 200, data: { status: 'AC', passed: 1, total: 1, message: '通过', details: [] } }
        ]
    });
    page.elements.get('resultDetails').innerHTML = 'old error';
    await page.elements.get('runBtn').emit('click');
    assert.deepEqual(page.requests.submits, [{ code: 'int main() {}', id: 7 }]);
    assert.equal(page.elements.get('runBtn').disabled, true);
    assert.equal(page.elements.get('runBtn').attributes['aria-busy'], 'true');
    assert.equal(page.elements.get('resultDetails').innerHTML, '');
    assert.equal(page.elements.get('resultPanel').classList.contains('hidden'), true);
    await page.elements.get('runBtn').emit('click');
    assert.equal(page.requests.submits.length, 1);
    await page.tick(250);
    assert.equal(page.elements.get('runBtn').textContent, '排队中...');
    await page.tick(250);
    assert.equal(page.elements.get('runBtn').textContent, '判题中...');
    await page.tick(250);
    assert.equal(page.elements.get('runBtn').disabled, false);
    assert.equal(page.elements.get('resultStatus').textContent, 'AC');
    assert.equal(page.elements.get('resultPanel').classList.contains('hidden'), false);
    assert.equal(page.storage.has('oj_draft_problem_7'), false);
    assert.deepEqual(page.requests.submissions, [42, 42, 42]);
});

test('a terminal result cancels a pending autosave so a cleared draft is not recreated', async () => {
    const page = await createPage();
    page.editor.change();
    await page.elements.get('runBtn').emit('click');
    await page.tick(250);
    assert.equal([...page.timers.values()].some(timer => timer.delay === 500), false);
    assert.equal(page.storage.has('oj_draft_problem_7'), false);
});

test('edits made while judging remain saved whether autosave runs before the result or not', async () => {
    for (const saveBeforeResult of [false, true]) {
        const page = await createPage();
        page.editor.value = 'submitted A';
        await page.elements.get('runBtn').emit('click');
        page.editor.value = 'unsent B';
        page.editor.change();
        if (saveBeforeResult) await page.tick(500);
        await page.tick(250);
        assert.equal(page.storage.get('oj_draft_problem_7'), 'unsent B');
        assert.equal([...page.timers.values()].some(timer => timer.delay === 500), false);
    }
});

test('changing code and then restoring the submitted version allows its draft to be cleared', async () => {
    const page = await createPage();
    page.editor.value = 'submitted A';
    await page.elements.get('runBtn').emit('click');
    page.editor.value = 'unsent B';
    page.editor.change();
    await page.tick(500);
    page.editor.value = 'submitted A';
    page.editor.change();
    await page.tick(250);
    assert.equal(page.storage.has('oj_draft_problem_7'), false);
    assert.equal([...page.timers.values()].some(timer => timer.delay === 500), false);
});

test('poll errors and timeout restore the action while preserving the draft', async () => {
    for (const response of [{ status: 500, data: { error: '服务异常' } }, new Error('offline')]) {
        const page = await createPage({ storage: { oj_draft_problem_7: 'saved' }, submissionResponses: [response] });
        await page.elements.get('runBtn').emit('click');
        await page.tick(250);
        assert.equal(page.elements.get('runBtn').disabled, false);
        assert.equal(page.storage.get('oj_draft_problem_7'), 'saved');
        assert.equal(page.timers.size, 0);
        assert.equal(page.alerts.length, 1);
    }
    const timeout = await createPage({ submissionResponses: [{ status: 200, data: { status: 'JUDGING' } }] });
    vm.runInContext('pollSubmission(42, 120)', timeout.context);
    await timeout.tick(250);
    assert.equal(timeout.elements.get('runBtn').disabled, false);
    assert.match(timeout.alerts[0], /判题超时/);
});

test('WA output and compiler errors are escaped, and a later result clears old details', async () => {
    const page = await createPage();
    page.result({ status: 'WA', passed: 0, total: 1, message: '答案错误', details: [{ case: 1, status: 'WA', elapsed_ms: 2, expected: '<script>expected</script>', actual: '<img src=x>', error: '<error>' }] });
    const detail = page.elements.get('resultDetails').innerHTML;
    assert.match(detail, /&lt;script&gt;expected&lt;\/script&gt;/);
    assert.match(detail, /&lt;img src=x&gt;/);
    assert.match(detail, /result-error result-case-error/);
    assert.doesNotMatch(detail, /<script>|<img/);
    page.result({ status: 'CE', passed: 0, total: 1, message: '编译失败', error_info: '<fatal>' });
    assert.match(page.elements.get('resultDetails').innerHTML, /&lt;fatal&gt;/);
    page.result({ status: 'AC', passed: 1, total: 1, message: '通过' });
    assert.equal(page.elements.get('resultDetails').innerHTML, '');
});
