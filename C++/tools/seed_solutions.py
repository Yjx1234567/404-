#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""为 100 道题批量生成参考答案 UPDATE SQL。"""

SOLUTIONS = {
1: r'''#include <iostream>
using namespace std;
int main() {
    int a, b; cin >> a >> b;
    cout << a + b << endl;
    return 0;
}''',

2: r'''#include <iostream>
using namespace std;
int main() {
    cout << "Hello, World!" << endl;
    return 0;
}''',

3: r'''#include <iostream>
using namespace std;
int main() {
    int a, b; cin >> a >> b;
    cout << b << " " << a << endl;
    return 0;
}''',

4: r'''#include <iostream>
#include <iomanip>
using namespace std;
int main() {
    int a, b, c; cin >> a >> b >> c;
    cout << fixed << setprecision(2) << (a + b + c) / 3.0 << endl;
    return 0;
}''',

5: r'''#include <iostream>
#include <iomanip>
using namespace std;
int main() {
    double c; cin >> c;
    cout << fixed << setprecision(2) << c * 9 / 5 + 32 << endl;
    return 0;
}''',

6: r'''#include <iostream>
#include <iomanip>
using namespace std;
int main() {
    double r; cin >> r;
    const double PI = 3.14159;
    cout << fixed << setprecision(2) << PI * r * r << " " << 2 * PI * r << endl;
    return 0;
}''',

7: r'''#include <iostream>
using namespace std;
int main() {
    int n; cin >> n;
    cout << (n % 2 != 0 ? "Yes" : "No") << endl;
    return 0;
}''',

8: r'''#include <iostream>
using namespace std;
int main() {
    int n; cin >> n;
    cout << (n % 2 == 0 ? "Yes" : "No") << endl;
    return 0;
}''',

9: r'''#include <iostream>
using namespace std;
int main() {
    int a, b; cin >> a >> b;
    cout << (a > b ? a : b) << endl;
    return 0;
}''',

10: r'''#include <iostream>
using namespace std;
int main() {
    int a, b, c; cin >> a >> b >> c;
    cout << max(a, max(b, c)) << endl;
    return 0;
}''',

11: r'''#include <iostream>
#include <cstdlib>
using namespace std;
int main() {
    int n; cin >> n;
    cout << abs(n) << endl;
    return 0;
}''',

12: r'''#include <iostream>
using namespace std;
int main() {
    int n; cin >> n;
    cout << n * n << " " << n * n * n << endl;
    return 0;
}''',

13: r'''#include <iostream>
using namespace std;
int main() {
    int a, b; cin >> a >> b;
    cout << a / b << " " << a % b << endl;
    return 0;
}''',

14: r'''#include <iostream>
using namespace std;
int main() {
    int n; cin >> n;
    int r = (n % 10) * 100 + (n / 10 % 10) * 10 + n / 100;
    cout << r << endl;
    return 0;
}''',

15: r'''#include <iostream>
using namespace std;
int main() {
    int n; cin >> n;
    cout << n / 100 + n / 10 % 10 + n % 10 << endl;
    return 0;
}''',

16: r'''#include <iostream>
using namespace std;
int main() {
    int n; cin >> n;
    cout << n * (n + 1) / 2 << endl;
    return 0;
}''',

17: r'''#include <iostream>
using namespace std;
int main() {
    int n; cin >> n;
    cout << n * n << endl;
    return 0;
}''',

18: r'''#include <iostream>
using namespace std;
int main() {
    int n; cin >> n;
    cout << n * (n + 1) << endl;
    return 0;
}''',

19: r'''#include <iostream>
using namespace std;
int main() {
    int n; cin >> n;
    long long f = 1;
    for (int i = 2; i <= n; i++) f *= i;
    cout << f << endl;
    return 0;
}''',

20: r'''#include <iostream>
using namespace std;
int main() {
    int n; cin >> n;
    long long a = 1, b = 1;
    for (int i = 3; i <= n; i++) { long long c = a + b; a = b; b = c; }
    cout << b << endl;
    return 0;
}''',

21: r'''#include <iostream>
using namespace std;
int main() {
    int n; cin >> n;
    bool ok = true;
    for (int i = 2; i * i <= n; i++) if (n % i == 0) { ok = false; break; }
    cout << (ok ? "Yes" : "No") << endl;
    return 0;
}''',

22: r'''#include <iostream>
using namespace std;
int main() {
    int n; cin >> n;
    int a = n / 100, b = n / 10 % 10, c = n % 10;
    cout << (a*a*a + b*b*b + c*c*c == n ? "Yes" : "No") << endl;
    return 0;
}''',

23: r'''#include <iostream>
using namespace std;
int gcd(int a, int b) { return b ? gcd(b, a % b) : a; }
int main() {
    int a, b; cin >> a >> b;
    cout << gcd(a, b) << endl;
    return 0;
}''',

24: r'''#include <iostream>
using namespace std;
int gcd(int a, int b) { return b ? gcd(b, a % b) : a; }
int main() {
    int a, b; cin >> a >> b;
    cout << a / gcd(a, b) * b << endl;
    return 0;
}''',

25: r'''#include <iostream>
using namespace std;
int main() {
    int n; cin >> n;
    long long sum = 0;
    for (int i = 0; i < n; i++) { int x; cin >> x; sum += x; }
    cout << sum << endl;
    return 0;
}''',

26: r'''#include <iostream>
#include <algorithm>
using namespace std;
int main() {
    int n; cin >> n;
    int mx; cin >> mx;
    for (int i = 1; i < n; i++) { int x; cin >> x; mx = max(mx, x); }
    cout << mx << endl;
    return 0;
}''',

27: r'''#include <iostream>
#include <algorithm>
using namespace std;
int main() {
    int n; cin >> n;
    int mn; cin >> mn;
    for (int i = 1; i < n; i++) { int x; cin >> x; mn = min(mn, x); }
    cout << mn << endl;
    return 0;
}''',

28: r'''#include <iostream>
#include <vector>
using namespace std;
int main() {
    int n; cin >> n;
    vector<int> a(n);
    for (int i = 0; i < n; i++) cin >> a[i];
    for (int i = n - 1; i >= 0; i--) cout << a[i] << (i ? " " : "\n");
    return 0;
}''',

29: r'''#include <iostream>
using namespace std;
int main() {
    int n; cin >> n;
    int cnt = 0;
    for (int i = 0; i < n; i++) { int x; cin >> x; if (x % 2 != 0) cnt++; }
    cout << cnt << endl;
    return 0;
}''',

30: r'''#include <iostream>
using namespace std;
int main() {
    int n; cin >> n;
    int cnt = 0;
    for (int i = 0; i < n; i++) { int x; cin >> x; if (x % 2 == 0) cnt++; }
    cout << cnt << endl;
    return 0;
}''',

31: r'''#include <iostream>
#include <string>
using namespace std;
int main() {
    string s; cin >> s;
    cout << s.size() << endl;
    return 0;
}''',

32: r'''#include <iostream>
#include <string>
#include <algorithm>
using namespace std;
int main() {
    string s; cin >> s;
    reverse(s.begin(), s.end());
    cout << s << endl;
    return 0;
}''',

33: r'''#include <iostream>
#include <string>
using namespace std;
int main() {
    string s; cin >> s;
    for (char &c : s) c = toupper(c);
    cout << s << endl;
    return 0;
}''',

34: r'''#include <iostream>
#include <string>
using namespace std;
int main() {
    string s; cin >> s;
    bool ok = true;
    for (int i = 0, j = s.size() - 1; i < j; i++, j--)
        if (s[i] != s[j]) ok = false;
    cout << (ok ? "Yes" : "No") << endl;
    return 0;
}''',

35: r'''#include <iostream>
#include <string>
using namespace std;
int main() {
    string s; cin >> s;
    int cnt = 0;
    for (char c : s) if (c=='a'||c=='e'||c=='i'||c=='o'||c=='u') cnt++;
    cout << cnt << endl;
    return 0;
}''',

36: r'''#include <iostream>
using namespace std;
int main() {
    int y; cin >> y;
    bool ok = (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
    cout << (ok ? "Yes" : "No") << endl;
    return 0;
}''',

37: r'''#include <iostream>
using namespace std;
int main() {
    int s; cin >> s;
    if (s >= 90) cout << "A" << endl;
    else if (s >= 80) cout << "B" << endl;
    else if (s >= 70) cout << "C" << endl;
    else if (s >= 60) cout << "D" << endl;
    else cout << "F" << endl;
    return 0;
}''',

38: r'''#include <iostream>
using namespace std;
int main() {
    int a, b, c; cin >> a >> b >> c;
    if (a + b <= c || a + c <= b || b + c <= a) { cout << "Invalid" << endl; return 0; }
    if (a == b && b == c) cout << "Equilateral" << endl;
    else if (a == b || b == c || a == c) cout << "Isosceles" << endl;
    else cout << "Scalene" << endl;
    return 0;
}''',

39: r'''#include <iostream>
using namespace std;
int main() {
    int n; cin >> n;
    for (int i = 1; i <= n; i++) {
        for (int j = 1; j <= i; j++) {
            cout << j << "x" << i << "=" << j * i;
            cout << (j == i ? "\n" : " ");
        }
    }
    return 0;
}''',

40: r'''#include <iostream>
#include <vector>
using namespace std;
int main() {
    int n; cin >> n;
    vector<vector<int>> t(n);
    for (int i = 0; i < n; i++) {
        t[i].resize(i + 1, 1);
        for (int j = 1; j < i; j++) t[i][j] = t[i-1][j-1] + t[i-1][j];
        for (int j = 0; j <= i; j++) cout << t[i][j] << (j == i ? "\n" : " ");
    }
    return 0;
}''',

41: r'''#include <iostream>
#include <vector>
using namespace std;
int main() {
    int n, t; cin >> n >> t;
    vector<int> a(n);
    for (int i = 0; i < n; i++) cin >> a[i];
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (a[i] + a[j] == t) { cout << i << " " << j << endl; return 0; }
    return 0;
}''',

42: r'''#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>
using namespace std;
int main() {
    string line; getline(cin, line);
    stringstream ss(line);
    vector<string> w; string x;
    while (ss >> x) w.push_back(x);
    reverse(w.begin(), w.end());
    for (int i = 0; i < (int)w.size(); i++) cout << w[i] << (i + 1 < (int)w.size() ? " " : "\n");
    return 0;
}''',

43: r'''#include <iostream>
#include <string>
#include <stack>
using namespace std;
int main() {
    string s; cin >> s;
    stack<char> st;
    bool ok = true;
    for (char c : s) {
        if (c=='('||c=='['||c=='{') st.push(c);
        else {
            if (st.empty()) { ok = false; break; }
            char t = st.top(); st.pop();
            if (c==')' && t!='(') { ok = false; break; }
            if (c==']' && t!='[') { ok = false; break; }
            if (c=='}' && t!='{') { ok = false; break; }
        }
    }
    if (!st.empty()) ok = false;
    cout << (ok ? "Yes" : "No") << endl;
    return 0;
}''',

44: r'''#include <iostream>
#include <vector>
using namespace std;
int main() {
    int n, m; cin >> n >> m;
    vector<int> a(n), b(m), r;
    for (auto &x : a) cin >> x;
    for (auto &x : b) cin >> x;
    int i = 0, j = 0;
    while (i < n && j < m) r.push_back(a[i] < b[j] ? a[i++] : b[j++]);
    while (i < n) r.push_back(a[i++]);
    while (j < m) r.push_back(b[j++]);
    for (int k = 0; k < (int)r.size(); k++) cout << r[k] << (k+1 < (int)r.size() ? " " : "\n");
    return 0;
}''',

45: r'''#include <iostream>
#include <vector>
using namespace std;
int main() {
    int n, t; cin >> n >> t;
    vector<int> a(n);
    for (auto &x : a) cin >> x;
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (a[mid] == t) { cout << mid << endl; return 0; }
        if (a[mid] < t) lo = mid + 1; else hi = mid - 1;
    }
    cout << -1 << endl;
    return 0;
}''',

46: r'''#include <iostream>
#include <vector>
using namespace std;
int main() {
    int n, t; cin >> n >> t;
    vector<int> a(n);
    for (auto &x : a) cin >> x;
    int lo = 0, hi = n;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (a[mid] < t) lo = mid + 1; else hi = mid;
    }
    cout << lo << endl;
    return 0;
}''',

47: r'''#include <iostream>
#include <vector>
using namespace std;
int main() {
    int n, t; cin >> n >> t;
    vector<int> a(n);
    for (auto &x : a) cin >> x;
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (a[mid] == t) { cout << mid << endl; return 0; }
        if (a[lo] <= a[mid]) {
            if (a[lo] <= t && t < a[mid]) hi = mid - 1; else lo = mid + 1;
        } else {
            if (a[mid] < t && t <= a[hi]) lo = mid + 1; else hi = mid - 1;
        }
    }
    cout << -1 << endl;
    return 0;
}''',

48: r'''#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;
int main() {
    int n; cin >> n;
    vector<int> a(n);
    for (auto &x : a) cin >> x;
    int best = a[0], cur = a[0];
    for (int i = 1; i < n; i++) {
        cur = max(a[i], cur + a[i]);
        best = max(best, cur);
    }
    cout << best << endl;
    return 0;
}''',

49: r'''#include <iostream>
using namespace std;
const int MOD = 1000000007;
int main() {
    int n; cin >> n;
    long long a = 1, b = 1;
    for (int i = 2; i <= n; i++) { long long c = (a + b) % MOD; a = b; b = c; }
    cout << b << endl;
    return 0;
}''',

50: r'''#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;
int main() {
    int n; cin >> n;
    vector<int> a(n);
    for (auto &x : a) cin >> x;
    if (n == 1) { cout << a[0] << endl; return 0; }
    int pp = a[0], p = max(a[0], a[1]);
    for (int i = 2; i < n; i++) {
        int cur = max(p, pp + a[i]);
        pp = p; p = cur;
    }
    cout << p << endl;
    return 0;
}''',

51: r'''#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;
int main() {
    int n; cin >> n;
    vector<vector<int>> a(n);
    for (int i = 0; i < n; i++) { a[i].resize(i + 1); for (auto &x : a[i]) cin >> x; }
    for (int i = n - 2; i >= 0; i--)
        for (int j = 0; j <= i; j++)
            a[i][j] += min(a[i+1][j], a[i+1][j+1]);
    cout << a[0][0] << endl;
    return 0;
}''',

52: r'''#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;
int main() {
    int n; cin >> n;
    vector<int> a(n);
    for (auto &x : a) cin >> x;
    vector<int> dp(n, 1);
    int best = 1;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < i; j++) if (a[j] < a[i]) dp[i] = max(dp[i], dp[j] + 1);
        best = max(best, dp[i]);
    }
    cout << best << endl;
    return 0;
}''',

53: r'''#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;
int main() {
    int n, amt; cin >> n >> amt;
    vector<int> c(n);
    for (auto &x : c) cin >> x;
    const int INF = 1e9;
    vector<int> dp(amt + 1, INF);
    dp[0] = 0;
    for (int i = 1; i <= amt; i++)
        for (int x : c) if (x <= i) dp[i] = min(dp[i], dp[i-x] + 1);
    cout << (dp[amt] >= INF ? -1 : dp[amt]) << endl;
    return 0;
}''',

54: r'''#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;
int main() {
    int n, W; cin >> n >> W;
    vector<int> w(n), v(n);
    for (auto &x : w) cin >> x;
    for (auto &x : v) cin >> x;
    vector<int> dp(W + 1, 0);
    for (int i = 0; i < n; i++)
        for (int j = W; j >= w[i]; j--)
            dp[j] = max(dp[j], dp[j-w[i]] + v[i]);
    cout << dp[W] << endl;
    return 0;
}''',

55: r'''#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
using namespace std;
int main() {
    string a, b; cin >> a >> b;
    int n = a.size(), m = b.size();
    vector<vector<int>> dp(n + 1, vector<int>(m + 1, 0));
    for (int i = 1; i <= n; i++)
        for (int j = 1; j <= m; j++)
            dp[i][j] = (a[i-1] == b[j-1]) ? dp[i-1][j-1] + 1 : max(dp[i-1][j], dp[i][j-1]);
    cout << dp[n][m] << endl;
    return 0;
}''',

56: r'''#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
using namespace std;
int main() {
    string a, b; cin >> a >> b;
    int n = a.size(), m = b.size();
    vector<vector<int>> dp(n + 1, vector<int>(m + 1, 0));
    for (int i = 0; i <= n; i++) dp[i][0] = i;
    for (int j = 0; j <= m; j++) dp[0][j] = j;
    for (int i = 1; i <= n; i++)
        for (int j = 1; j <= m; j++)
            dp[i][j] = a[i-1]==b[j-1] ? dp[i-1][j-1] : 1 + min({dp[i-1][j], dp[i][j-1], dp[i-1][j-1]});
    cout << dp[n][m] << endl;
    return 0;
}''',

57: r'''#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;
int main() {
    int n; cin >> n;
    vector<pair<int,int>> v(n);
    for (auto &p : v) cin >> p.first >> p.second;
    sort(v.begin(), v.end());
    vector<pair<int,int>> r;
    for (auto &p : v) {
        if (!r.empty() && p.first <= r.back().second) r.back().second = max(r.back().second, p.second);
        else r.push_back(p);
    }
    cout << r.size() << endl;
    for (auto &p : r) cout << p.first << " " << p.second << "\n";
    return 0;
}''',

58: r'''#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;
int main() {
    int n; cin >> n;
    vector<pair<int,int>> v(n);
    for (auto &p : v) cin >> p.first >> p.second;
    sort(v.begin(), v.end(), [](auto &a, auto &b){ return a.second < b.second; });
    int cnt = 0, last = -1e9;
    for (auto &p : v) if (p.first > last) { cnt++; last = p.second; }
    cout << cnt << endl;
    return 0;
}''',

59: r'''#include <iostream>
#include <vector>
using namespace std;
int main() {
    int n; cin >> n;
    vector<int> a(n);
    for (auto &x : a) cin >> x;
    vector<int> r;
    for (int x : a) if (r.empty() || r.back() != x) r.push_back(x);
    cout << r.size() << endl;
    for (int i = 0; i < (int)r.size(); i++) cout << r[i] << (i+1 < (int)r.size() ? " " : "\n");
    return 0;
}''',

60: r'''#include <iostream>
#include <vector>
using namespace std;
int main() {
    int n, v; cin >> n >> v;
    vector<int> a(n), r;
    for (auto &x : a) cin >> x;
    for (int x : a) if (x != v) r.push_back(x);
    cout << r.size() << endl;
    for (int i = 0; i < (int)r.size(); i++) cout << r[i] << (i+1 < (int)r.size() ? " " : "\n");
    return 0;
}''',

61: r'''#include <iostream>
#include <string>
using namespace std;
int main() {
    string a, b; cin >> a >> b;
    if (b.empty()) { cout << 0 << endl; return 0; }
    int pos = a.find(b);
    cout << (pos == string::npos ? -1 : (int)pos) << endl;
    return 0;
}''',

62: r'''#include <iostream>
#include <string>
#include <algorithm>
using namespace std;
int main() {
    string a, b; cin >> a >> b;
    int i = a.size() - 1, j = b.size() - 1, carry = 0;
    string r;
    while (i >= 0 || j >= 0 || carry) {
        int s = carry;
        if (i >= 0) s += a[i--] - '0';
        if (j >= 0) s += b[j--] - '0';
        r.push_back(char('0' + s % 2));
        carry = s / 2;
    }
    reverse(r.begin(), r.end());
    cout << r << endl;
    return 0;
}''',

63: r'''#include <iostream>
#include <vector>
using namespace std;
int main() {
    int n; cin >> n;
    vector<int> a(n);
    for (auto &x : a) cin >> x;
    int i = n - 1;
    while (i >= 0 && a[i] == 9) { a[i] = 0; i--; }
    if (i < 0) a.insert(a.begin(), 1);
    else a[i]++;
    for (int k = 0; k < (int)a.size(); k++) cout << a[k] << (k+1 < (int)a.size() ? " " : "\n");
    return 0;
}''',

64: r'''#include <iostream>
#include <vector>
using namespace std;
int main() {
    int m, n; cin >> m >> n;
    vector<vector<int>> a(m, vector<int>(n));
    for (auto &row : a) for (auto &x : row) cin >> x;
    vector<int> r;
    int top = 0, bot = m - 1, left = 0, right = n - 1;
    while (top <= bot && left <= right) {
        for (int j = left; j <= right; j++) r.push_back(a[top][j]);
        top++;
        for (int i = top; i <= bot; i++) r.push_back(a[i][right]);
        right--;
        if (top <= bot) { for (int j = right; j >= left; j--) r.push_back(a[bot][j]); bot--; }
        if (left <= right) { for (int i = bot; i >= top; i--) r.push_back(a[i][left]); left++; }
    }
    for (int i = 0; i < (int)r.size(); i++) cout << r[i] << (i+1 < (int)r.size() ? " " : "\n");
    return 0;
}''',

65: r'''#include <iostream>
#include <vector>
using namespace std;
int main() {
    int n; cin >> n;
    vector<int> row(n + 1, 0);
    row[0] = 1;
    for (int i = 1; i <= n; i++) {
        for (int j = i; j >= 1; j--) row[j] += row[j-1];
    }
    for (int j = 0; j <= n; j++) cout << row[j] << (j < n ? " " : "\n");
    return 0;
}''',

66: r'''#include <iostream>
#include <string>
#include <map>
using namespace std;
int main() {
    string a, b; cin >> a >> b;
    map<char,char> m1, m2;
    bool ok = a.size() == b.size();
    for (int i = 0; ok && i < (int)a.size(); i++) {
        char x = a[i], y = b[i];
        if (m1.count(x) && m1[x] != y) ok = false;
        if (m2.count(y) && m2[y] != x) ok = false;
        m1[x] = y; m2[y] = x;
    }
    cout << (ok ? "Yes" : "No") << endl;
    return 0;
}''',

67: r'''#include <iostream>
#include <string>
#include <algorithm>
using namespace std;
int main() {
    int n; cin >> n;
    if (n == 0) { cout << 0 << endl; return 0; }
    string r;
    while (n > 0) { r.push_back(char('0' + n % 2)); n /= 2; }
    reverse(r.begin(), r.end());
    cout << r << endl;
    return 0;
}''',

68: r'''#include <iostream>
#include <string>
#include <map>
using namespace std;
int main() {
    string s; cin >> s;
    map<char,int> v = {{'I',1},{'V',5},{'X',10},{'L',50},{'C',100},{'D',500},{'M',1000}};
    int total = 0;
    for (int i = 0; i < (int)s.size(); i++) {
        if (i+1 < (int)s.size() && v[s[i]] < v[s[i+1]]) total -= v[s[i]];
        else total += v[s[i]];
    }
    cout << total << endl;
    return 0;
}''',

69: r'''#include <iostream>
#include <vector>
using namespace std;
int main() {
    int n; cin >> n;
    vector<int> a(n);
    for (auto &x : a) cin >> x;
    for (int i = n - 1; i >= 0; i--) cout << a[i] << (i ? " " : "\n");
    return 0;
}''',

70: r'''#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;
int main() {
    int n; cin >> n;
    vector<pair<int,int>> v(n);
    for (auto &p : v) cin >> p.first >> p.second;
    sort(v.begin(), v.end());
    vector<pair<int,int>> r;
    for (auto &p : v) {
        if (!r.empty() && p.first <= r.back().second) r.back().second = max(r.back().second, p.second);
        else r.push_back(p);
    }
    cout << r.size() << endl;
    for (auto &p : r) cout << p.first << " " << p.second << "\n";
    return 0;
}''',

71: r'''#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;
int main() {
    int n, k; cin >> n >> k;
    vector<int> a(n);
    for (auto &x : a) cin >> x;
    k %= n;
    reverse(a.begin(), a.end());
    reverse(a.begin(), a.begin() + k);
    reverse(a.begin() + k, a.end());
    for (int i = 0; i < n; i++) cout << a[i] << (i+1 < n ? " " : "\n");
    return 0;
}''',

72: r'''#include <iostream>
#include <vector>
using namespace std;
int main() {
    int n; cin >> n;
    vector<int> a(n), r;
    for (auto &x : a) cin >> x;
    for (int x : a) if (x != 0) r.push_back(x);
    while ((int)r.size() < n) r.push_back(0);
    for (int i = 0; i < n; i++) cout << r[i] << (i+1 < n ? " " : "\n");
    return 0;
}''',

73: r'''#include <iostream>
#include <vector>
using namespace std;
int main() {
    int n; cin >> n;
    vector<int> a(n);
    for (auto &x : a) cin >> x;
    int cand = 0, cnt = 0;
    for (int x : a) {
        if (cnt == 0) { cand = x; cnt = 1; }
        else cnt += (x == cand ? 1 : -1);
    }
    cout << cand << endl;
    return 0;
}''',

74: r'''#include <iostream>
#include <vector>
#include <map>
using namespace std;
int main() {
    int n, k; cin >> n >> k;
    vector<int> a(n);
    for (auto &x : a) cin >> x;
    map<int,int> last;
    bool ok = false;
    for (int i = 0; i < n; i++) {
        if (last.count(a[i]) && i - last[a[i]] <= k) { ok = true; break; }
        last[a[i]] = i;
    }
    cout << (ok ? "Yes" : "No") << endl;
    return 0;
}''',

75: r'''#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
using namespace std;
int depth(vector<string> &a, int i) {
    if (i >= (int)a.size() || a[i] == "null") return 0;
    return 1 + max(depth(a, 2*i+1), depth(a, 2*i+2));
}
int main() {
    int n; cin >> n;
    vector<string> a(n);
    for (auto &x : a) cin >> x;
    cout << depth(a, 0) << endl;
    return 0;
}''',

76: r'''#include <iostream>
#include <vector>
#include <string>
#include <queue>
using namespace std;
int main() {
    int n; cin >> n;
    vector<string> a(n);
    for (auto &x : a) cin >> x;
    queue<int> q;
    if (n > 0 && a[0] != "null") q.push(0);
    while (!q.empty()) {
        int sz = q.size();
        for (int k = 0; k < sz; k++) {
            int i = q.front(); q.pop();
            cout << a[i] << (k+1 < sz ? " " : "\n");
            if (2*i+1 < n && a[2*i+1] != "null") q.push(2*i+1);
            if (2*i+2 < n && a[2*i+2] != "null") q.push(2*i+2);
        }
    }
    return 0;
}''',

77: r'''#include <iostream>
#include <vector>
using namespace std;
int main() {
    int n, m; cin >> n >> m;
    vector<int> a(n), b(m);
    for (auto &x : a) cin >> x;
    for (auto &x : b) cin >> x;
    int i = n - 1, j = m - 1;
    while (i >= 0 && j >= 0 && a[i] == b[j]) { i--; j--; }
    if (i < 0 || j < 0) { cout << (i < 0 ? b[j+1] : a[i+1]) << endl; return 0; }
    if (a[i] == b[j]) cout << a[i] << endl;
    else cout << "No" << endl;
    return 0;
}''',

78: r'''#include <iostream>
#include <vector>
using namespace std;
int main() {
    int n; cin >> n;
    vector<int> a(n);
    for (auto &x : a) cin >> x;
    int pos; cin >> pos;
    cout << (pos >= 0 ? "Yes" : "No") << endl;
    return 0;
}''',

79: r'''#include <iostream>
#include <vector>
#include <string>
using namespace std;
bool sym(vector<string> &a, int i, int j) {
    if (i >= (int)a.size() && j >= (int)a.size()) return true;
    if (i >= (int)a.size() || j >= (int)a.size()) return false;
    if (a[i] != a[j]) return false;
    return sym(a, 2*i+1, 2*j+2) && sym(a, 2*i+2, 2*j+1);
}
int main() {
    int n; cin >> n;
    vector<string> a(n);
    for (auto &x : a) cin >> x;
    cout << (sym(a, 0, 0) ? "Yes" : "No") << endl;
    return 0;
}''',

80: r'''#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
using namespace std;
int h; bool bal;
int check(vector<string> &a, int i) {
    if (i >= (int)a.size() || a[i] == "null") return 0;
    int l = check(a, 2*i+1), r = check(a, 2*i+2);
    if (abs(l - r) > 1) bal = false;
    return 1 + max(l, r);
}
int main() {
    int n; cin >> n;
    vector<string> a(n);
    for (auto &x : a) cin >> x;
    bal = true;
    check(a, 0);
    cout << (bal ? "Yes" : "No") << endl;
    return 0;
}''',

81: r'''#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;
int main() {
    int n; cin >> n;
    vector<int> a(n);
    for (auto &x : a) cin >> x;
    int l = 0, r = n - 1, lm = 0, rm = 0, water = 0;
    while (l < r) {
        if (a[l] < a[r]) {
            if (a[l] >= lm) lm = a[l]; else water += lm - a[l];
            l++;
        } else {
            if (a[r] >= rm) rm = a[r]; else water += rm - a[r];
            r--;
        }
    }
    cout << water << endl;
    return 0;
}''',

82: r'''#include <iostream>
#include <vector>
#include <stack>
using namespace std;
int main() {
    int n; cin >> n;
    vector<int> a(n);
    for (auto &x : a) cin >> x;
    stack<int> st;
    int best = 0;
    for (int i = 0; i <= n; i++) {
        int h = (i == n ? 0 : a[i]);
        while (!st.empty() && a[st.top()] > h) {
            int hi = a[st.top()]; st.pop();
            int w = st.empty() ? i : i - st.top() - 1;
            best = max(best, hi * w);
        }
        st.push(i);
    }
    cout << best << endl;
    return 0;
}''',

83: r'''#include <iostream>
#include <vector>
#include <string>
using namespace std;
int main() {
    int m, n; cin >> m >> n;
    vector<string> g(m);
    for (auto &row : g) cin >> row;
    vector<int> h(n + 1, 0);
    int best = 0;
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) h[j] = (g[i][j] == '1') ? h[j] + 1 : 0;
        vector<int> st;
        for (int j = 0; j <= n; j++) {
            while (!st.empty() && h[st.back()] > h[j]) {
                int hi = h[st.back()]; st.pop_back();
                int w = st.empty() ? j : j - st.back() - 1;
                best = max(best, hi * w);
            }
            st.push_back(j);
        }
    }
    cout << best << endl;
    return 0;
}''',

84: r'''#include <iostream>
#include <vector>
#include <string>
using namespace std;
string findLCA(vector<string> &a, int i, string p, string q) {
    if (i >= (int)a.size() || a[i] == "null") return "null";
    if (a[i] == p || a[i] == q) return a[i];
    string l = findLCA(a, 2*i+1, p, q);
    string r = findLCA(a, 2*i+2, p, q);
    if (l != "null" && r != "null") return a[i];
    return l != "null" ? l : r;
}
int main() {
    int n; cin >> n;
    vector<string> a(n);
    for (auto &x : a) cin >> x;
    string p, q; cin >> p >> q;
    cout << findLCA(a, 0, p, q) << endl;
    return 0;
}''',

85: r'''#include <iostream>
#include <vector>
using namespace std;
int main() {
    int n; cin >> n;
    vector<long long> dp(n + 1, 0);
    dp[0] = 1; dp[1] = 1;
    for (int i = 2; i <= n; i++)
        for (int j = 1; j <= i; j++) dp[i] += dp[j-1] * dp[i-j];
    cout << dp[n] << endl;
    return 0;
}''',

86: r'''#include <iostream>
#include <vector>
#include <string>
#include <queue>
using namespace std;
int main() {
    int n; cin >> n;
    vector<string> a(n);
    for (auto &x : a) cin >> x;
    vector<string> r;
    queue<int> q;
    if (n > 0 && a[0] != "null") q.push(0);
    while (!q.empty()) {
        int sz = q.size(), last = -1;
        for (int k = 0; k < sz; k++) {
            int i = q.front(); q.pop();
            last = i;
            if (2*i+1 < n && a[2*i+1] != "null") q.push(2*i+1);
            if (2*i+2 < n && a[2*i+2] != "null") q.push(2*i+2);
        }
        if (last >= 0) r.push_back(a[last]);
    }
    for (int i = 0; i < (int)r.size(); i++) cout << r[i] << (i+1 < (int)r.size() ? " " : "\n");
    return 0;
}''',

87: r'''#include <iostream>
#include <vector>
#include <string>
using namespace std;
int m, n;
vector<string> g;
int dx[4] = {0,0,1,-1}, dy[4] = {1,-1,0,0};
void dfs(int x, int y) {
    if (x < 0 || x >= m || y < 0 || y >= n || g[x][y] != '1') return;
    g[x][y] = '0';
    for (int d = 0; d < 4; d++) dfs(x+dx[d], y+dy[d]);
}
int main() {
    cin >> m >> n;
    g.resize(m);
    for (auto &row : g) cin >> row;
    int cnt = 0;
    for (int i = 0; i < m; i++)
        for (int j = 0; j < n; j++)
            if (g[i][j] == '1') { cnt++; dfs(i, j); }
    cout << cnt << endl;
    return 0;
}''',

88: r'''#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;
int main() {
    int n, m; cin >> n >> m;
    vector<vector<int>> g(n);
    for (int i = 0; i < m; i++) {
        int u, v; cin >> u >> v;
        g[u].push_back(v);
        g[v].push_back(u);
    }
    for (int i = 0; i < n; i++) {
        sort(g[i].begin(), g[i].end());
        for (int j = 0; j < (int)g[i].size(); j++) cout << g[i][j] << (j+1 < (int)g[i].size() ? " " : "\n");
        if (g[i].empty()) cout << "\n";
    }
    return 0;
}''',

89: r'''#include <iostream>
#include <vector>
using namespace std;
int main() {
    int n, m; cin >> n >> m;
    vector<vector<int>> g(n);
    vector<int> indeg(n, 0);
    for (int i = 0; i < m; i++) {
        int a, b; cin >> a >> b;
        g[b].push_back(a);
        indeg[a]++;
    }
    vector<int> q;
    for (int i = 0; i < n; i++) if (indeg[i] == 0) q.push_back(i);
    int seen = 0;
    for (int k = 0; k < (int)q.size(); k++) {
        int u = q[k]; seen++;
        for (int v : g[u]) if (--indeg[v] == 0) q.push_back(v);
    }
    cout << (seen == n ? "Yes" : "No") << endl;
    return 0;
}''',

90: r'''#include <iostream>
#include <string>
#include <map>
using namespace std;
struct Node { map<char,Node*> ch; bool end = false; };
int main() {
    int k; cin >> k;
    Node* root = new Node();
    while (k--) {
        string op, w; cin >> op >> w;
        if (op == "insert") {
            Node* cur = root;
            for (char c : w) { if (!cur->ch[c]) cur->ch[c] = new Node(); cur = cur->ch[c]; }
            cur->end = true;
        } else if (op == "search") {
            Node* cur = root; bool ok = true;
            for (char c : w) { if (!cur->ch.count(c)) { ok = false; break; } cur = cur->ch[c]; }
            cout << ((ok && cur->end) ? "Yes" : "No") << endl;
        } else { // prefix
            Node* cur = root; bool ok = true;
            for (char c : w) { if (!cur->ch.count(c)) { ok = false; break; } cur = cur->ch[c]; }
            cout << (ok ? "Yes" : "No") << endl;
        }
    }
    return 0;
}''',

91: r'''#include <iostream>
#include <vector>
#include <string>
using namespace std;
int m, n;
vector<string> g; string w;
int dx[4] = {0,0,1,-1}, dy[4] = {1,-1,0,0};
bool dfs(int x, int y, int i) {
    if (i == (int)w.size()) return true;
    if (x < 0 || x >= m || y < 0 || y >= n || g[x][y] != w[i]) return false;
    char save = g[x][y]; g[x][y] = '#';
    for (int d = 0; d < 4; d++) if (dfs(x+dx[d], y+dy[d], i+1)) { g[x][y] = save; return true; }
    g[x][y] = save;
    return false;
}
int main() {
    cin >> m >> n >> w;
    g.resize(m);
    for (auto &row : g) cin >> row;
    bool ok = false;
    for (int i = 0; i < m && !ok; i++)
        for (int j = 0; j < n && !ok; j++) if (dfs(i, j, 0)) ok = true;
    cout << (ok ? "Yes" : "No") << endl;
    return 0;
}''',

92: r'''#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;
int main() {
    int m, n; cin >> m >> n;
    vector<vector<int>> a(m, vector<int>(n));
    for (auto &row : a) for (auto &x : row) cin >> x;
    vector<vector<int>> dp(m, vector<int>(n));
    dp[0][0] = a[0][0];
    for (int i = 0; i < m; i++)
        for (int j = 0; j < n; j++) {
            if (i == 0 && j == 0) continue;
            int best = 1e9;
            if (i > 0) best = min(best, dp[i-1][j]);
            if (j > 0) best = min(best, dp[i][j-1]);
            dp[i][j] = best + a[i][j];
        }
    cout << dp[m-1][n-1] << endl;
    return 0;
}''',

93: r'''#include <iostream>
using namespace std;
int main() {
    int m, n; cin >> m >> n;
    long long dp[105][105] = {};
    dp[1][1] = 1;
    for (int i = 1; i <= m; i++)
        for (int j = 1; j <= n; j++) {
            if (i == 1 && j == 1) continue;
            dp[i][j] = dp[i-1][j] + dp[i][j-1];
        }
    cout << dp[m][n] << endl;
    return 0;
}''',

94: r'''#include <iostream>
#include <string>
#include <vector>
using namespace std;
int main() {
    string s, p; cin >> s >> p;
    int n = s.size(), m = p.size();
    vector<vector<bool>> dp(n+1, vector<bool>(m+1, false));
    dp[0][0] = true;
    for (int j = 2; j <= m; j += 2) if (p[j-1] == '*') dp[0][j] = dp[0][j-2];
    for (int i = 1; i <= n; i++)
        for (int j = 1; j <= m; j++) {
            if (p[j-1] == '.' || p[j-1] == s[i-1]) dp[i][j] = dp[i-1][j-1];
            else if (p[j-1] == '*') {
                dp[i][j] = dp[i][j-2];
                if (p[j-2] == '.' || p[j-2] == s[i-1]) dp[i][j] = dp[i][j] || dp[i-1][j];
            }
        }
    cout << (dp[n][m] ? "Yes" : "No") << endl;
    return 0;
}''',

95: r'''#include <iostream>
#include <string>
#include <vector>
using namespace std;
int main() {
    string s, p; cin >> s >> p;
    int n = s.size(), m = p.size();
    vector<vector<bool>> dp(n+1, vector<bool>(m+1, false));
    dp[0][0] = true;
    for (int j = 1; j <= m; j++) if (p[j-1] == '*') dp[0][j] = dp[0][j-1];
    for (int i = 1; i <= n; i++)
        for (int j = 1; j <= m; j++) {
            if (p[j-1] == '?' || p[j-1] == s[i-1]) dp[i][j] = dp[i-1][j-1];
            else if (p[j-1] == '*') dp[i][j] = dp[i-1][j] || dp[i][j-1];
        }
    cout << (dp[n][m] ? "Yes" : "No") << endl;
    return 0;
}''',

96: r'''#include <iostream>
#include <string>
#include <stack>
using namespace std;
int main() {
    string s; cin >> s;
    stack<int> st; st.push(-1);
    int best = 0;
    for (int i = 0; i < (int)s.size(); i++) {
        if (s[i] == '(') st.push(i);
        else {
            st.pop();
            if (st.empty()) st.push(i);
            else best = max(best, i - st.top());
        }
    }
    cout << best << endl;
    return 0;
}''',

97: r'''#include <iostream>
#include <vector>
#include <queue>
using namespace std;
int main() {
    int k; cin >> k;
    cin.ignore();
    priority_queue<int, vector<int>, greater<int>> pq;
    for (int i = 0; i < k; i++) {
        string line; getline(cin, line);
        size_t pos = 0;
        while (pos < line.size()) {
            while (pos < line.size() && line[pos] == ' ') pos++;
            if (pos >= line.size()) break;
            int x = 0;
            while (pos < line.size() && isdigit(line[pos])) { x = x*10 + (line[pos]-'0'); pos++; }
            pq.push(x);
        }
    }
    bool first = true;
    while (!pq.empty()) {
        if (!first) cout << " ";
        cout << pq.top(); pq.pop();
        first = false;
    }
    cout << endl;
    return 0;
}''',

98: r'''#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;
int main() {
    int n; cin >> n;
    vector<int> a(n);
    for (auto &x : a) cin >> x;
    sort(a.begin(), a.end());
    for (int i = 0; i < n; i++) cout << a[i] << (i+1 < n ? " " : "\n");
    return 0;
}''',

99: r'''#include <iostream>
#include <vector>
using namespace std;
int main() {
    int n, k; cin >> n >> k;
    vector<int> a(n);
    for (auto &x : a) cin >> x;
    vector<int> r;
    for (int i = 0; i < n; i++) if (i != n - k) r.push_back(a[i]);
    for (int i = 0; i < (int)r.size(); i++) cout << r[i] << (i+1 < (int)r.size() ? " " : "\n");
    return 0;
}''',

100: r'''#include <iostream>
#include <vector>
using namespace std;
int main() {
    int n; cin >> n;
    vector<int> v(n), r(n);
    for (auto &x : v) cin >> x;
    for (auto &x : r) cin >> x;
    for (int i = 0; i < n; i++) cout << v[i] << (i+1 < n ? " " : "\n");
    for (int i = 0; i < n; i++) cout << r[i] << (i+1 < n ? " " : "\n");
    return 0;
}''',
}


def esc(s: str) -> str:
    return s.replace("\\", "\\\\").replace("'", "\\'")


def main():
    print("USE oj_system;")
    print("SET NAMES utf8mb4;")
    for pid, code in SOLUTIONS.items():
        print(f"UPDATE problems SET solution = '{esc(code)}' WHERE id = {pid};")
    print(f"-- done: {len(SOLUTIONS)} solutions updated")


if __name__ == "__main__":
    main()
