#!/bin/bash
# 验证题目列表难度过滤接口
BASE="http://localhost:8080/api"
curl -s -c /tmp/j3 -X POST $BASE/login -H 'Content-Type: application/json' \
    -d '{"username":"student1","password":"pass123"}' -o /dev/null

for d in Easy Medium Hard; do
    total=$(curl -s -b /tmp/j3 "$BASE/problems?difficulty=$d&page_size=1" | python3 -c "import sys,json; print(json.load(sys.stdin)['total'])")
    echo "$d -> $total"
done
echo "all -> $(curl -s -b /tmp/j3 "$BASE/problems?page_size=1" | python3 -c "import sys,json; print(json.load(sys.stdin)['total'])")"
