let adminProblems = [];

function checkAdmin() {
    const user = checkAuth();
    if (!user || user.role !== 'admin') {
        window.location.href = appPageUrl('problem_list.html');
    }
}

async function loadAdminProblems() {
    const list = document.getElementById('adminProblemList');
    list.innerHTML = '<div class="loading">加载中...</div>';
    
    const { status, data } = await getProblems();
    if (status !== 200 || !data.problems) {
        list.innerHTML = '<div class="empty-state">加载失败</div>';
        return;
    }
    
    adminProblems = data.problems;
    renderAdminProblems();
}

function renderAdminProblems() {
    const list = document.getElementById('adminProblemList');
    if (adminProblems.length === 0) {
        list.innerHTML = '<div class="empty-state">暂无题目</div>';
        return;
    }
    
    list.innerHTML = adminProblems.map(p => `
        <div class="admin-problem-item">
            <div class="admin-problem-item-info">
                <div class="admin-problem-item-title">#${p.id} ${p.title}</div>
                <div class="admin-problem-item-meta">
                    <span class="difficulty-tag ${p.difficulty.toLowerCase()}">${p.difficulty}</span>
                    ${p.created_at}
                </div>
            </div>
            <button class="btn-delete" onclick="handleDelete(${p.id})">删除</button>
        </div>
    `).join('');
}

async function handleDelete(id) {
    if (!confirm(`确定要删除题目 #${id} 吗？`)) return;
    
    const { status, data } = await deleteProblem(id);
    if (status === 200 && data.message === 'problem deleted successfully') {
        adminProblems = adminProblems.filter(p => p.id !== id);
        renderAdminProblems();
    } else {
        alert(data.error || '删除失败');
    }
}

// Test case management
const testCaseContainer = document.getElementById('testCaseContainer');
const addTestCaseBtn = document.getElementById('addTestCaseBtn');

if (addTestCaseBtn) {
    addTestCaseBtn.addEventListener('click', () => {
        const div = document.createElement('div');
        div.className = 'testcase-item';
        div.innerHTML = `
            <input type="text" class="tc-input" placeholder="输入" aria-label="测试用例输入">
            <input type="text" class="tc-expected" placeholder="期望输出" aria-label="测试用例期望输出">
            <button type="button" class="btn-remove-tc" aria-label="移除测试用例">&times;</button>
        `;
        div.querySelector('.btn-remove-tc').addEventListener('click', () => div.remove());
        testCaseContainer.appendChild(div);
    });
}

if (testCaseContainer) {
    testCaseContainer.addEventListener('click', (e) => {
        if (e.target.classList.contains('btn-remove-tc')) {
            e.target.closest('.testcase-item').remove();
        }
    });
}

// Create form
const createForm = document.getElementById('createProblemForm');
if (createForm) {
    createForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const errorDiv = document.getElementById('createError');
        const successDiv = document.getElementById('createSuccess');
        errorDiv.textContent = '';
        successDiv.textContent = '';
        
        const testCases = [];
        const items = testCaseContainer.querySelectorAll('.testcase-item');
        items.forEach(item => {
            const input = item.querySelector('.tc-input').value;
            const expected = item.querySelector('.tc-expected').value;
            if (input.trim() || expected.trim()) {
                testCases.push({ input, expected });
            }
        });
        
        const problem = {
            title: document.getElementById('title').value,
            difficulty: document.getElementById('difficulty').value,
            content: document.getElementById('content').value,
            template: document.getElementById('template').value,
            test_cases: testCases
        };
        
        const { status, data } = await createProblem(problem);
        if (status === 201 || data.message === 'problem created successfully') {
            successDiv.textContent = '题目创建成功！';
            createForm.reset();
            testCaseContainer.innerHTML = `
                <div class="testcase-item">
                    <input type="text" class="tc-input" placeholder="输入" aria-label="测试用例输入">
                    <input type="text" class="tc-expected" placeholder="期望输出" aria-label="测试用例期望输出">
                    <button type="button" class="btn-remove-tc" aria-label="移除测试用例">&times;</button>
                </div>
            `;
            loadAdminProblems();
        } else {
            errorDiv.textContent = data.error || '创建失败';
        }
    });
}

checkAdmin();
loadAdminProblems();
updateUserInfo();
