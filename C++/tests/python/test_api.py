import requests
import pytest
import json

BASE_URL = "http://localhost:8080/api"

class TestAPI:
    @pytest.fixture
    def session(self):
        s = requests.Session()
        yield s
        s.close()

    def test_get_problems_unauthorized(self, session):
        """未登录应返回 401"""
        resp = session.get(f"{BASE_URL}/problems")
        assert resp.status_code in [401, 403]

    def test_register_validation(self, session):
        """注册参数校验"""
        # 空用户名
        resp = session.post(f"{BASE_URL}/register", json={"password": "123456"})
        assert resp.status_code == 400
        
        # 用户名太短
        resp = session.post(f"{BASE_URL}/register", json={"username": "ab", "password": "123456"})
        assert resp.status_code == 400
        
        # 密码太短
        resp = session.post(f"{BASE_URL}/register", json={"username": "testuser", "password": "123"})
        assert resp.status_code == 400

    def test_login_validation(self, session):
        """登录参数校验"""
        resp = session.post(f"{BASE_URL}/login", json={"username": "", "password": "123456"})
        assert resp.status_code == 400
        
        resp = session.post(f"{BASE_URL}/login", json={"username": "testuser", "password": ""})
        assert resp.status_code == 400

    def test_submit_no_code(self, session):
        """提交空代码应返回 400 (未登录时先返回 401)"""
        resp = session.post(f"{BASE_URL}/submit", json={"problem_id": 1})
        assert resp.status_code in [400, 401, 403]

    def test_admin_without_auth(self, session):
        """未登录访问管理接口应返回 401/403"""
        resp = session.post(f"{BASE_URL}/admin/problems", json={
            "title": "Test", "difficulty": "Easy", "content": "Test"
        })
        assert resp.status_code in [401, 403]
        
        resp = session.delete(f"{BASE_URL}/admin/problems/1")
        assert resp.status_code in [401, 403]
