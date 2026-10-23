#!/bin/bash
# 端到端 API 冒烟测试（在 WSL 内运行，服务需已启动在 8080）
set -e
BASE="http://localhost:8080/api"
JAR="/tmp/oj_cookies_user.txt"
JAR_ADMIN="/tmp/oj_cookies_admin.txt"
PASS=0; FAIL=0

check() {
    local name="$1" expected="$2" actual="$3"
    if [ "$actual" = "$expected" ]; then
        PASS=$((PASS+1)); echo "  [PASS] $name"
    else
        FAIL=$((FAIL+1)); echo "  [FAIL] $name (want=$expected got=$actual)"
    fi
}

echo "=== 1. 认证 ==="
code=$(curl -s -o /dev/null -w '%{http_code}' -X POST $BASE/register -H 'Content-Type: application/json' -d '{"username":"student1","password":"pass123"}')
if [ "$code" = "200" ] || [ "$code" = "201" ]; then
    PASS=$((PASS+1)); echo "  [PASS] register student1"
else
    FAIL=$((FAIL+1)); echo "  [FAIL] register student1 (got=$code)"
fi
code=$(curl -s -c $JAR -o /dev/null -w '%{http_code}' -X POST $BASE/login -H 'Content-Type: application/json' -d '{"username":"student1","password":"pass123"}')
check "login student1" "200" "$code"
code=$(curl -s -c $JAR_ADMIN -o /dev/null -w '%{http_code}' -X POST $BASE/login -H 'Content-Type: application/json' -d '{"username":"admin","password":"admin123"}')
check "login admin" "200" "$code"

echo "=== 2. 题目列表（分页/筛选/搜索/未登录拦截）==="
body=$(curl -s -b $JAR "$BASE/problems?page=1&page_size=2&keyword=斐波那契")
n=$(echo "$body" | python3 -c "import sys,json; d=json.load(sys.stdin); print(len(d['problems']), d['total'])")
check "keyword search 1 result" "1 1" "$n"
body=$(curl -s -b $JAR "$BASE/problems?difficulty=Easy")
n=$(echo "$body" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d['total'])")
check "difficulty=Easy filter" "2" "$n"
code=$(curl -s -o /dev/null -w '%{http_code}' $BASE/problems)
check "problems without login -> 401" "401" "$code"

echo "=== 3. 异步提交判题 ==="
submit_and_wait() {
    local pid="$1" code_src="$2" jar="$3"
    local body=$(curl -s -b "$jar" -X POST $BASE/submit -H 'Content-Type: application/json' \
        -d "{\"problem_id\":$pid,\"code\":$(python3 -c "import json,sys; print(json.dumps(sys.stdin.read()))" <<< "$code_src")}")
    local sid=$(echo "$body" | python3 -c "import sys,json; print(json.load(sys.stdin).get('submission_id',0))")
    for i in $(seq 1 120); do
        local st=$(curl -s -b "$jar" $BASE/submissions/$sid | python3 -c "import sys,json; print(json.load(sys.stdin).get('status','?'))")
        if [ "$st" != "PENDING" ] && [ "$st" != "JUDGING" ]; then echo "$sid $st"; return; fi
        sleep 0.25
    done
    echo "$sid TIMEOUT"
}

AC_CODE='#include <iostream>
using namespace std;
int main(){int a,b;cin>>a>>b;cout<<a+b<<endl;return 0;}'
res=$(submit_and_wait 1 "$AC_CODE" $JAR)
check "AC submit judged AC" "1 AC" "$res"

WA_CODE='#include <iostream>
using namespace std;
int main(){int a,b;cin>>a>>b;cout<<a-b<<endl;return 0;}'
res=$(submit_and_wait 1 "$WA_CODE" $JAR)
check "WA submit judged WA" "2 WA" "$res"

CE_CODE='int main( { return 0; }'
res=$(submit_and_wait 1 "$CE_CODE" $JAR)
check "CE submit judged CE" "3 CE" "$res"

TLE_CODE='#include <iostream>
using namespace std;
int main(){ while(true){} return 0; }'
res=$(submit_and_wait 1 "$TLE_CODE" $JAR)
check "TLE submit judged TLE" "4 TLE" "$res"

MLE_CODE='#include <vector>
using namespace std;
int main(){ vector<char> v; while(true){ v.push_back(1); v.push_back(2); v.push_back(3); v.push_back(4);} return 0; }'
res=$(submit_and_wait 1 "$MLE_CODE" $JAR)
check "MLE submit judged MLE" "5 MLE" "$res"

RE_CODE='#include <iostream>
using namespace std;
int main(){ int* p=nullptr; *p=1; return 0; }'
res=$(submit_and_wait 1 "$RE_CODE" $JAR)
check "RE submit judged RE" "6 RE" "$res"

echo "=== 4. 提交历史/统计/排行榜 ==="
body=$(curl -s -b $JAR "$BASE/submissions?page=1&page_size=100")
n=$(echo "$body" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d['total'])")
check "submission history count" "6" "$n"
body=$(curl -s -b $JAR "$BASE/users/me/stats")
s=$(echo "$body" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d['solved'], d['attempted'], d['submissions'])")
check "user stats solved/attempted/subs" "1 1 6" "$s"
body=$(curl -s -b $JAR "$BASE/leaderboard")
top=$(echo "$body" | python3 -c "import sys,json; d=json.load(sys.stdin); lb=d['leaderboard']; print(lb[0]['username'], lb[0]['solved'])")
check "leaderboard top1" "student1 1" "$top"

echo "=== 5. 标签 ==="
body=$(curl -s -b $JAR_ADMIN -X POST $BASE/admin/problems -H 'Content-Type: application/json' -d '{"title":"标签题","difficulty":"Easy","content":"test","tags":["模拟","入门"],"test_cases":[{"input":"1","expected":"1"}]}')
newpid=$(echo "$body" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])")
body=$(curl -s -b $JAR "$BASE/tags")
n=$(echo "$body" | python3 -c "import sys,json; d=json.load(sys.stdin); print(len(d['tags']))")
check "tags list has 2 tags" "2" "$n"
body=$(curl -s -b $JAR "$BASE/problems?tag_id=1")
n=$(echo "$body" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d['total'])")
check "filter by tag_id=1" "1" "$n"

echo "=== 6. 题目编辑（PUT）==="
code=$(curl -s -b $JAR_ADMIN -o /dev/null -w '%{http_code}' -X PUT $BASE/admin/problems/$newpid -H 'Content-Type: application/json' -d '{"title":"标签题改","difficulty":"Medium","content":"changed","template":"","tags":["DP"],"test_cases":[{"input":"2","expected":"2"}]}')
check "admin update problem" "200" "$code"
title=$(curl -s -b $JAR $BASE/problems/$newpid | python3 -c "import sys,json; d=json.load(sys.stdin); print(d['title'])")
check "problem updated title" "标签题改" "$title"
code=$(curl -s -b $JAR -o /dev/null -w '%{http_code}' -X PUT $BASE/admin/problems/$newpid -H 'Content-Type: application/json' -d '{"title":"x","difficulty":"Easy","content":"y"}')
check "non-admin update -> 403" "403" "$code"

echo "=== 7. 班级/作业 ==="
body=$(curl -s -b $JAR_ADMIN -X POST $BASE/classes -H 'Content-Type: application/json' -d '{"name":"C++ 入门班"}')
joincode=$(echo "$body" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d['join_code'])")
cid=$(echo "$body" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])")
check "class created with code" "6" "${#joincode}"
code=$(curl -s -b $JAR -o /dev/null -w '%{http_code}' -X POST $BASE/classes/join -H 'Content-Type: application/json' -d "{\"join_code\":\"$joincode\"}")
check "student joins class" "200" "$code"
code=$(curl -s -b $JAR_ADMIN -o /dev/null -w '%{http_code}' -X POST $BASE/classes/$cid/assignments -H 'Content-Type: application/json' -d "{\"title\":\"第一次作业\",\"description\":\"做完前三题\",\"problem_ids\":[1,2,3]}")
check "teacher creates assignment" "201" "$code"
body=$(curl -s -b $JAR $BASE/classes/$cid/assignments)
s=$(echo "$body" | python3 -c "import sys,json; a=json.load(sys.stdin)['assignments'][0]; print(a['solved_count'], a['total_count'])")
check "student assignment progress" "1 3" "$s"
body=$(curl -s -b $JAR_ADMIN $BASE/assignments/1)
n=$(echo "$body" | python3 -c "import sys,json; d=json.load(sys.stdin); print(len(d['member_status']))")
check "teacher sees member matrix" "1" "$n"

echo "=== 8. 频率限制 ==="
codes=""
for i in 1 2 3 4 5 6 7 8 9 10 11 12; do
    c=$(curl -s -o /dev/null -w '%{http_code}' -b $JAR -X POST $BASE/submit -H 'Content-Type: application/json' -d '{"problem_id":1,"code":"int main(){return 0;}"}')
    codes="$codes $c"
done
has429=$(echo $codes | grep -c 429 || true)
if [ "$has429" -ge 1 ]; then check "rate limit kicks in (429)" "yes" "yes"; else check "rate limit kicks in (429)" "yes" "no"; fi

echo ""
echo "=============================="
echo "PASS=$PASS FAIL=$FAIL"
