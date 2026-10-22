// Run with: node --test tests/frontend/navigation.test.js
// Tests the actual shared navigation helper, without a browser or isolated helper stub.
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const source = fs.readFileSync(path.join(__dirname, '../../public/js/api.js'), 'utf8');
const currentRelease = '0123456789abcdef';
const previousRelease = 'fedcba9876543210';

function createNavigation(options = {}) {
    const current = new URL(options.href || 'http://localhost:8080/login.html');
    const redirects = [];
    const removals = [];
    const context = vm.createContext({
        URL,
        document: {
            querySelector(selector) {
                assert.equal(selector, 'meta[name="app-version"]');
                return options.meta === undefined ? null : { content: options.meta };
            }
        },
        window: { location: {
            href: current.href, origin: current.origin, pathname: current.pathname,
            replace: target => redirects.push(target)
        } },
        localStorage: { removeItem: key => removals.push(key) },
        async fetch() { return { status: options.status || 401, async json() { return { error: 'unauthorized' }; } }; }
    });
    vm.runInContext(source, context, { filename: 'api.js' });
    return { context, redirects, removals, url: target => context.appPageUrl(target) };
}

test('the page release marker overrides an old URL version while preserving the target query and fragment', () => {
    const page = createNavigation({ href: 'http://localhost:8080/login.html?v=' + previousRelease, meta: currentRelease });
    assert.equal(page.url('problem.html?id=7&view=cases#sample'), 'problem.html?id=7&view=cases&v=' + currentRelease + '#sample');
    assert.equal(page.url('register.html'), 'register.html?v=' + currentRelease);
    assert.equal(page.url('login.html'), 'login.html?v=' + currentRelease);
    assert.equal(page.url('problem_list.html'), 'problem_list.html?v=' + currentRelease);
});

test('a valid URL version is the fallback when the marker is absent or invalid', () => {
    for (const meta of [undefined, '', 'old-release', '<script>bad</script>', currentRelease + '0']) {
        const page = createNavigation({ href: 'http://localhost:8080/login.html?v=' + previousRelease, meta });
        assert.equal(page.url('./profile.html?tab=recent'), './profile.html?tab=recent&v=' + previousRelease);
    }
});

test('an unversioned source deployment preserves every original relative URL exactly', () => {
    for (const href of ['http://localhost:8080/login.html', 'http://localhost:8080/login.html?v=invalid']) {
        const page = createNavigation({ href, meta: 'invalid' });
        for (const target of ['login.html', './register.html?name=A%20B#form', '/problem.html?id=7&view=cases']) {
            assert.equal(page.url(target), target);
        }
    }
});

test('versioning is limited to same-origin HTML page references', () => {
    const page = createNavigation({ meta: currentRelease });
    for (const target of [
        'https://example.com/login.html', 'http://localhost:8081/login.html',
        '//example.com/login.html', '/api/login', 'css/style.css', 'js/auth.js',
        '#features', '?mode=preview', 'mailto:help@example.com', 'javascript:alert(1)'
    ]) assert.equal(page.url(target), target);
    assert.equal(page.url('/profile.html'), '/profile.html?v=' + currentRelease);
    assert.equal(page.url('http://localhost:8080/profile.html'), 'http://localhost:8080/profile.html?v=' + currentRelease);
    assert.equal(page.url('//localhost:8080/profile.html'), '//localhost:8080/profile.html?v=' + currentRelease);
});

test('a previous target version is replaced once without losing IDs, filters, or fragments', () => {
    const page = createNavigation({ meta: currentRelease });
    const result = page.url('problem.html?id=A%20%26%2FB&v=old&view=cases&v=duplicate#case-2');
    const parsed = new URL(result, 'http://localhost:8080/');
    assert.deepEqual(parsed.searchParams.getAll('v'), [currentRelease]);
    assert.equal(parsed.searchParams.get('id'), 'A &/B');
    assert.equal(parsed.searchParams.get('view'), 'cases');
    assert.equal(parsed.hash, '#case-2');
    assert.ok(result.startsWith('problem.html?'));
});

test('only complete 16-digit hexadecimal release values are accepted', () => {
    for (const meta of ['0123456789abcde', '0123456789abcdef0', '0123456789abcdeg', '0123456789abcdef&next=evil', ' 0123456789abcdef ']) {
        assert.equal(createNavigation({ meta }).url('register.html'), 'register.html');
    }
    assert.equal(createNavigation({ meta: currentRelease.toUpperCase() }).url('register.html'), 'register.html?v=' + currentRelease.toUpperCase());
});

test('an expired protected request uses the same real helper for its login replacement', async () => {
    const page = createNavigation({ href: 'http://localhost:8080/problem_list.html?v=' + previousRelease, meta: currentRelease });
    const result = await page.context.getProblems({ page: 1 });
    assert.equal(result.status, 401);
    assert.match(result.data.error, /登录状态已失效/);
    assert.deepEqual(page.removals, ['user']);
    assert.deepEqual(page.redirects, ['login.html?v=' + currentRelease]);
});

test('failed login and registration still avoid forced navigation even when a release is present', async () => {
    const page = createNavigation({ meta: currentRelease });
    await page.context.loginUser('student', 'wrong');
    await page.context.registerUser('student', 'wrong');
    assert.deepEqual(page.removals, []);
    assert.deepEqual(page.redirects, []);
});
