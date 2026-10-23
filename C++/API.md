# API 接口文档

## 基础信息
- **Base URL**: `/api`
- **Content-Type**: `application/json`
- **认证方式**: Session/Cookie（通过 `session_id` Cookie）

---

## 公开接口（普通用户）

### 1. 题目列表
**GET** `/api/problems`

获取题目列表，支持分页与筛选。

**查询参数**:
| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| page | int | 否 | 页码，默认 1 |
| page_size | int | 否 | 每页数量，默认 20，最大 100 |
| difficulty | string | 否 | `Easy` / `Medium` / `Hard` |
| keyword | string | 否 | 标题模糊搜索 |
| tag_id | int | 否 | 按标签过滤 |

**响应示例** (200):
```json
{
  "problems": [
    {
      "id": 1,
      "title": "两数之和",
      "difficulty": "Easy",
      "tags": ["模拟"],
      "created_at": "2026-04-24 10:00:00"
    }
  ],
  "total": 21,
  "page": 1,
  "page_size": 20
}
```

---

### 2. 题目详情
**GET** `/api/problems/:id`

获取指定题目的详细信息及测试用例。

**路径参数**:
| 参数 | 类型 | 说明 |
|------|------|------|
| id | int | 题目 ID |

**响应示例** (200):
```json
{
  "id": 1,
  "title": "两数之和",
  "difficulty": "Easy",
  "content": "给定一个整数数组 nums...",
  "template": "#include <iostream>\nusing namespace std;",
  "created_at": "2026-04-24 10:00:00",
  "test_cases": [
    {
      "id": 1,
      "input": "2 7 11 15",
      "expected": "9 15",
      "position": 0
    }
  ]
}
```

**错误响应**:
- `400`: `{"error": "invalid problem id"}`
- `404`: `{"error": "problem not found"}`

---

### 3. 提交代码
**POST** `/api/submit`

提交代码并执行。

**请求体**:
| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| code | string | 是 | C++ 源代码 |
| problem_id | int | 是 | 题目 ID |

**响应示例** (200):
```json
{
  "status": "AC",
  "message": "All test cases passed",
  "passed": 3,
  "total": 3,
  "details": [
    {
      "case": 1,
      "exit_code": 0,
      "elapsed_ms": 15,
      "stdout": "9 15",
      "status": "AC"
    }
  ]
}
```

**status 枚举值**:
- `AC`: All Correct，所有测试用例通过
- `WA`: Wrong Answer，输出结果错误
- `TLE`: Time Limit Exceeded，超时
- `RE`: Runtime Error，运行时错误
- `CE`: Compilation Error，编译错误

**错误响应**:
- `400`: `{"error": "code is required"}` / `{"error": "problem_id is required"}`
- `404`: `{"error": "problem not found"}`

---

### 4. 用户注册
**POST** `/api/register`

注册新用户账号。

**请求体**:
| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| username | string | 是 | 用户名，3-64 字符 |
| password | string | 是 | 密码，至少 6 字符 |

**响应示例** (201):
```json
{
  "message": "user registered successfully",
  "id": 1
}
```

**错误响应**:
- `400`: `{"error": "username is required"}` / `{"error": "username must be between 3 and 64 characters"}` / `{"error": "password must be at least 6 characters"}`
- `409`: `{"error": "username already exists"}`
- `500`: `{"error": "failed to create user"}`

---

### 5. 用户登录
**POST** `/api/login`

用户登录，登录成功后在 Cookie 中设置 `session_id`。

**请求体**:
| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| username | string | 是 | 用户名 |
| password | string | 是 | 密码 |

**响应示例** (200):
```json
{
  "message": "login successful",
  "username": "testuser",
  "role": "user",
  "session_id": "a1b2c3d4..."
}
```

> 说明：登录成功后服务端会同时通过 `Set-Cookie: session_id=xxx; Path=/; HttpOnly` 下发会话 Cookie，前端请求自动携带；响应体中的 `session_id` 字段仅供调试参考。

**错误响应**:
- `400`: `{"error": "username is required"}` / `{"error": "password is required"}`
- `401`: `{"error": "invalid username or password"}`

---

### 6. 用户登出
**POST** `/api/logout`

用户登出，销毁当前会话。

**请求头**:
- `Cookie`: `session_id=xxx`

**响应示例** (200):
```json
{
  "message": "logout successful"
}
```

---

## 管理接口（管理员）
> **注意**: 管理接口需要管理员权限（role 为 `admin`）

### 7. 新增题目
**POST** `/api/admin/problems`

创建新题目。

**请求头**:
- `Cookie`: `session_id=xxx`

**请求体**:
| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| title | string | 是 | 题目标题 |
| difficulty | string | 是 | 难度等级：`Easy`、`Medium`、`Hard` |
| content | string | 是 | 题目描述（Markdown 格式） |
| template | string | 否 | 代码模板 |
| test_cases | array | 否 | 测试用例数组 |

**test_cases 数组元素**:
| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| input | string | 是 | 输入数据 |
| expected | string | 是 | 期望输出 |
| position | int | 否 | 排序序号 |

**响应示例** (201):
```json
{
  "id": 1,
  "message": "problem created successfully"
}
```

**错误响应**:
- `400`: `{"error": "title is required"}` / `{"error": "difficulty is required"}` / `{"error": "content is required"}` / `{"error": "invalid difficulty, must be Easy, Medium, or Hard"}`
- `500`: `{"error": "failed to save problem"}`

---

### 8. 删除题目
**DELETE** `/api/admin/problems/:id`

删除指定题目。

**路径参数**:
| 参数 | 类型 | 说明 |
|------|------|------|
| id | int | 题目 ID |

**请求头**:
- `Cookie`: `session_id=xxx`

**响应示例** (200):
```json
{
  "message": "problem deleted successfully"
}
```

**错误响应**:
- `400`: `{"error": "invalid problem id"}`
- `500`: `{"error": "failed to delete problem"}`

---

## 扩展接口（判题队列 / 统计 / 班级）

### 9. 提交代码（异步判题）
**POST** `/api/submit`

提交后立即返回，代码落库并进入判题队列；前端轮询接口 11 查询结果。

**请求体**: 同接口 3（`code` + `problem_id`）

**响应示例** (200):
```json
{
  "submission_id": 12,
  "status": "PENDING",
  "message": "Judging"
}
```

**错误响应**:
- `400`: 参数错误 / 代码超过 64KB
- `404`: `{"error": "problem not found"}`
- `429`: `{"error": "too many submissions, please wait a moment"}`（触发频率限制）

### 10. 提交记录列表
**GET** `/api/submissions?page=1&page_size=20&problem_id=`

查询**当前用户**的提交历史（管理员可用 `user_id=` 查看他人）。

**响应示例** (200):
```json
{
  "submissions": [
    {
      "id": 12,
      "user_id": 2,
      "problem_id": 1,
      "problem_title": "两数相加",
      "status": "AC",
      "passed": 3,
      "total": 3,
      "time_ms": 50,
      "created_at": "2026-09-03 01:33:38"
    }
  ],
  "total": 12,
  "page": 1,
  "page_size": 20
}
```

`status` 枚举：`PENDING`（排队）、`JUDGING`（判题中）、`AC`、`WA`、`TLE`、`MLE`（内存超限）、`RE`、`CE`、`SE`。

### 11. 提交详情
**GET** `/api/submissions/:id`

仅本人或管理员可查看（含代码与逐用例结果）。

**响应示例** (200):
```json
{
  "id": 12,
  "user_id": 2,
  "problem_id": 1,
  "status": "AC",
  "message": "All test cases passed",
  "passed": 3,
  "total": 3,
  "time_ms": 50,
  "error_info": "",
  "code": "#include <iostream>...",
  "details": [
    { "case": 1, "exit_code": 0, "elapsed_ms": 15, "stdout": "3", "status": "AC" }
  ],
  "created_at": "2026-09-03 01:33:38"
}
```

**错误响应**:
- `403`: `{"error": "forbidden"}`（查看他人提交）
- `404`: `{"error": "submission not found"}`

### 12. 个人做题统计
**GET** `/api/users/me/stats`

**响应示例** (200):
```json
{
  "solved": 1,
  "attempted": 2,
  "submissions": 12,
  "solved_by_difficulty": { "Easy": 1, "Medium": 0, "Hard": 0 }
}
```

### 13. 排行榜
**GET** `/api/leaderboard`

按 AC 题数排名（并列时提交次数少者靠前），返回前 50 名。

**响应示例** (200):
```json
{
  "leaderboard": [
    { "rank": 1, "user_id": 2, "username": "student1", "solved": 1, "submissions": 12 }
  ]
}
```

### 14. 标签列表
**GET** `/api/tags`

**响应示例** (200):
```json
{
  "tags": [ { "id": 1, "name": "模拟", "problem_count": 1 } ]
}
```

### 15. 管理员：题目详情 / 更新题目
**GET** `/api/admin/problems/:id` — 含 `spj_code`、`tags`、`test_cases`（编辑表单用）
**PUT** `/api/admin/problems/:id` — 更新题目；`test_cases` / `tags` 提供时整体替换

**PUT 请求体**: 同接口 7，另支持 `spj_code`（特判源码，空 = 普通比对）与 `tags`（字符串数组）。

### 16. 班级
| 方法 | 路径 | 说明 |
|------|------|------|
| POST | `/api/classes` | 创建班级 `{name}`，创建者为老师，返回 `{id, join_code}` |
| POST | `/api/classes/join` | 凭 `{join_code}` 加入班级 |
| GET | `/api/classes/mine` | 我创建/加入的班级 |
| GET | `/api/classes/:id` | 班级详情（老师/管理员含成员列表） |
| DELETE | `/api/classes/:id` | 删除班级（老师本人或管理员） |

### 17. 作业
| 方法 | 路径 | 说明 |
|------|------|------|
| POST | `/api/classes/:id/assignments` | 布置作业 `{title, description, problem_ids[], due_at}` |
| GET | `/api/classes/:id/assignments` | 作业列表（含我每题完成状态） |
| GET | `/api/assignments/:id` | 作业详情；老师/管理员额外返回全班完成矩阵 `member_status` |

---

## 数据模型

### 题目 (Problem)
| 字段 | 类型 | 说明 |
|------|------|------|
| id | int | 题目 ID |
| title | string | 题目标题 |
| difficulty | string | 难度：`Easy`、`Medium`、`Hard` |
| content | string | 题目描述 |
| template | string | 代码模板 |
| created_at | string | 创建时间 |

### 测试用例 (TestCase)
| 字段 | 类型 | 说明 |
|------|------|------|
| id | int | 用例 ID |
| problem_id | int | 所属题目 ID |
| input | string | 输入数据 |
| expected | string | 期望输出 |
| position | int | 排序序号 |

### 用户 (User)
| 字段 | 类型 | 说明 |
|------|------|------|
| id | int | 用户 ID |
| username | string | 用户名 |
| role | string | 角色：`user`、`admin` |
| created_at | string | 创建时间 |

---

## 错误码汇总

| HTTP 状态码 | 说明 |
|-------------|------|
| 200 | 请求成功 |
| 201 | 创建成功 |
| 400 | 请求参数错误 |
| 401 | 认证失败 |
| 403 | 权限不足 |
| 404 | 资源不存在 |
| 409 | 资源冲突（如用户名已存在） |
| 500 | 服务器内部错误 |
