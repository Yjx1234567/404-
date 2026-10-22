CREATE DATABASE IF NOT EXISTS oj_system DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE oj_system;

-- 题目表
CREATE TABLE IF NOT EXISTS problems (
    id          INT PRIMARY KEY AUTO_INCREMENT,
    title       VARCHAR(255) NOT NULL,
    difficulty  ENUM('Easy','Medium','Hard') NOT NULL,
    content     TEXT NOT NULL,
    template    TEXT,
    spj_code    MEDIUMTEXT NULL,   -- Special Judge 源码（NULL/空 表示普通比对）
    solution    MEDIUMTEXT NULL,   -- 可选参考答案（保留原题目数据）
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 测试用例表 (与题目 1:N 关联)
CREATE TABLE IF NOT EXISTS test_cases (
    id         INT PRIMARY KEY AUTO_INCREMENT,
    problem_id INT NOT NULL,
    input      TEXT NOT NULL,
    expected   TEXT NOT NULL,
    position   INT NOT NULL DEFAULT 0, -- 排序序号
    FOREIGN KEY (problem_id) REFERENCES problems(id) ON DELETE CASCADE
);

-- 用户表
CREATE TABLE IF NOT EXISTS users (
    id       INT PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(64) UNIQUE NOT NULL,
    password VARCHAR(128) NOT NULL,
    role     ENUM('user','admin') DEFAULT 'user',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 会话表
CREATE TABLE IF NOT EXISTS sessions (
    id         INT PRIMARY KEY AUTO_INCREMENT,
    user_id    INT NOT NULL,
    token      VARCHAR(128) UNIQUE NOT NULL,
    expires_at TIMESTAMP NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

-- 提交记录表（判题队列 + 历史查询 + 统计的数据源）
CREATE TABLE IF NOT EXISTS submissions (
    id         INT PRIMARY KEY AUTO_INCREMENT,
    user_id    INT NOT NULL,
    problem_id INT NOT NULL,
    code       MEDIUMTEXT NOT NULL,
    status     ENUM('PENDING','JUDGING','AC','WA','TLE','MLE','RE','CE','SE') NOT NULL DEFAULT 'PENDING',
    passed     INT NOT NULL DEFAULT 0,
    total      INT NOT NULL DEFAULT 0,
    time_ms    INT NOT NULL DEFAULT 0,      -- 各用例最大耗时
    error_info TEXT,                         -- 编译错误 / 运行错误摘要
    result     MEDIUMTEXT,                  -- 判题结果 JSON（details）
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_sub_user (user_id, id),
    INDEX idx_sub_problem (problem_id, id),
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
    FOREIGN KEY (problem_id) REFERENCES problems(id) ON DELETE CASCADE
);

-- 标签表
CREATE TABLE IF NOT EXISTS tags (
    id   INT PRIMARY KEY AUTO_INCREMENT,
    name VARCHAR(64) UNIQUE NOT NULL
);

-- 题目-标签关联表
CREATE TABLE IF NOT EXISTS problem_tags (
    problem_id INT NOT NULL,
    tag_id     INT NOT NULL,
    PRIMARY KEY (problem_id, tag_id),
    FOREIGN KEY (problem_id) REFERENCES problems(id) ON DELETE CASCADE,
    FOREIGN KEY (tag_id) REFERENCES tags(id) ON DELETE CASCADE
);

-- 班级表（teacher_id 为创建者，任何登录用户都可建班）
CREATE TABLE IF NOT EXISTS classes (
    id         INT PRIMARY KEY AUTO_INCREMENT,
    name       VARCHAR(128) NOT NULL,
    join_code  VARCHAR(16) UNIQUE NOT NULL,
    teacher_id INT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (teacher_id) REFERENCES users(id) ON DELETE CASCADE
);

-- 班级成员表
CREATE TABLE IF NOT EXISTS class_members (
    class_id INT NOT NULL,
    user_id  INT NOT NULL,
    joined_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (class_id, user_id),
    FOREIGN KEY (class_id) REFERENCES classes(id) ON DELETE CASCADE,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

-- 作业表
CREATE TABLE IF NOT EXISTS assignments (
    id          INT PRIMARY KEY AUTO_INCREMENT,
    class_id    INT NOT NULL,
    title       VARCHAR(255) NOT NULL,
    description TEXT,
    due_at      DATETIME NULL,
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (class_id) REFERENCES classes(id) ON DELETE CASCADE
);

-- 作业-题目关联表
CREATE TABLE IF NOT EXISTS assignment_problems (
    assignment_id INT NOT NULL,
    problem_id    INT NOT NULL,
    position      INT NOT NULL DEFAULT 0,
    PRIMARY KEY (assignment_id, problem_id),
    FOREIGN KEY (assignment_id) REFERENCES assignments(id) ON DELETE CASCADE,
    FOREIGN KEY (problem_id) REFERENCES problems(id) ON DELETE CASCADE
);
