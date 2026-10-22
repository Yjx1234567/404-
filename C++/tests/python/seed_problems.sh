#!/bin/bash
# 批量创建测试题目（用于验证题目列表分页）
set -e
BASE="http://localhost:8080/api"
curl -s -c /tmp/adm -X POST $BASE/login -H 'Content-Type: application/json' \
    -d '{"username":"admin","password":"admin123"}' -o /dev/null

for i in $(seq 6 21); do
    curl -s -b /tmp/adm -X POST $BASE/admin/problems -H 'Content-Type: application/json' \
        -d "{\"title\":\"练习题 $i\",\"difficulty\":\"Easy\",\"content\":\"测试题目 $i\",\"test_cases\":[{\"input\":\"1\",\"expected\":\"1\"}]}" \
        -o /dev/null
done

curl -s -b /tmp/adm "$BASE/problems?page=2&page_size=20" | python3 -c "import sys,json; d=json.load(sys.stdin); print('page2 total:', d['total'], 'items:', len(d['problems']))"
curl -s -b /tmp/adm "$BASE/problems?page=1&page_size=20" | python3 -c "import sys,json; d=json.load(sys.stdin); print('page1 items:', len(d['problems']))"
