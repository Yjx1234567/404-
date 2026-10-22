function readStoredUser() {
    try {
        const stored = localStorage.getItem('user');
        if (!stored) return null;
        const user = JSON.parse(stored);
        if (user && typeof user.username === 'string' && user.username.trim() &&
            (user.role === 'user' || user.role === 'admin')) return user;
        localStorage.removeItem('user');
    } catch (error) {
        try { localStorage.removeItem('user'); } catch (storageError) { /* 网站存储不可用。 */ }
    }
    return null;
}

function checkAuth() {
    const user = readStoredUser();
    if (!user) {
        const publicPages = ['/login.html', '/register.html', '/index.html', '/'];
        const current = window.location.pathname;
        if (!publicPages.some(p => current.endsWith(p))) {
            window.location.href = appPageUrl('login.html');
        }
        return null;
    }
    return user;
}

function updateUserInfo() {
    const userInfo = document.getElementById('userInfo');
    if (!userInfo) return;
    const user = checkAuth();
    if (user) {
        userInfo.textContent = user.username;
        if (user.role === 'admin') {
            const badge = document.createElement('span');
            badge.className = 'admin-badge';
            badge.textContent = '管理员';
            userInfo.appendChild(badge);
        }
        
        const adminLink = document.getElementById('adminLink');
        if (adminLink) {
            adminLink.classList.toggle('hidden', user.role !== 'admin');
        }
    }
}

async function logout() {
    try { localStorage.removeItem('user'); } catch (error) { /* 仍需退出服务端会话。 */ }
    try {
        await logoutUser();
    } catch (error) {
        // 网络异常时仍清除本地登录态并允许重新登录。
    } finally {
        window.location.href = appPageUrl('login.html');
    }
}

// Login form
const loginForm = document.getElementById('loginForm');
if (loginForm) {
    loginForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const errorDiv = document.getElementById('loginError');
        errorDiv.textContent = '';
        
        const username = document.getElementById('username').value.trim();
        const password = document.getElementById('password').value;
        
        try {
            const { status, data } = await loginUser(username, password);
            if (status === 200 && data.message === 'login successful') {
                try {
                    localStorage.setItem('user', JSON.stringify({ username: data.username, role: data.role }));
                } catch (error) {
                    errorDiv.textContent = '浏览器无法保存登录信息，请允许此网站使用本地存储后重试。';
                    return;
                }
                window.location.href = appPageUrl('problem_list.html');
            } else {
                errorDiv.textContent = status === 401 ? '用户名或密码错误，请重新输入。' : (data.error || '登录失败，请重试。');
            }
        } catch (error) {
            errorDiv.textContent = '无法连接服务器，请确认已启动 OJ 服务后重试。';
        }
    });
}

// Register form
const registerForm = document.getElementById('registerForm');
if (registerForm) {
    registerForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const errorDiv = document.getElementById('registerError');
        errorDiv.textContent = '';
        
        const username = document.getElementById('username').value.trim();
        const password = document.getElementById('password').value;
        const confirmPassword = document.getElementById('confirmPassword').value;
        
        if (password !== confirmPassword) {
            errorDiv.textContent = '两次输入的密码不一致';
            return;
        }
        
        try {
            const { status, data } = await registerUser(username, password);
            if (status === 201 || data.message === 'user registered successfully') {
                alert('注册成功，请登录');
                window.location.href = appPageUrl('login.html');
            } else {
                errorDiv.textContent = data.error || '注册失败，请重试。';
            }
        } catch (error) {
            errorDiv.textContent = '无法连接服务器，请确认已启动 OJ 服务后重试。';
        }
    });
}

// Logout button
const logoutBtn = document.getElementById('logoutBtn');
if (logoutBtn) {
    logoutBtn.addEventListener('click', logout);
}

// 本地用户缓存不能证明 HttpOnly Cookie 会话仍然有效。
// 登录/注册页保持可访问，避免失效缓存把用户反复送回题库。
checkAuth();

updateUserInfo();
