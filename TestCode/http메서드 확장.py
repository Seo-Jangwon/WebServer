import requests
import json
import os
from pathlib import Path

class WebServerTest:
    def __init__(self, base_url="http://localhost:8080"):
        self.base_url = base_url
        self.test_files_dir = Path("test_files")
        self.test_files_dir.mkdir(exist_ok=True)

    def setup(self):
        """테스트 전 초기화"""
        print("\n=== Setting up test environment ===")
    
        # 테스트 파일 생성
        (self.test_files_dir / "test.txt").write_text("Hello, World!")
        (self.test_files_dir / "test.json").write_text('{"message": "Hello, JSON!"}')
    
        # static 폴더에 테스트 파일 복사
        static_dir = Path("static")
        static_dir.mkdir(exist_ok=True)
    
        with open(self.test_files_dir / "test.txt", "r") as src:
            with open(static_dir / "test.txt", "w") as dst:
                dst.write(src.read())
    
    def cleanup(self):
        """테스트 후 정리"""
        print("\n=== Cleaning up test environment ===")
        for file in self.test_files_dir.glob("*"):
            try:
                file.unlink()
            except:
                pass
        try:
            self.test_files_dir.rmdir()
        except:
            pass

    def test_get(self):
        """GET 메소드 테스트"""
        print("\n=== Testing GET ===")
        
        # 1. 존재하는 파일
        response = requests.get(f"{self.base_url}/test.txt")
        print(f"GET /test.txt: {response.status_code}")
        print(f"Response content: {response.text!r}")
        assert response.status_code == 200
        assert response.text.strip() == "테스트용 텍스트 파일"
    
        # 2. 존재하지 않는 파일
        response = requests.get(f"{self.base_url}/nonexistent.txt")
        print(f"GET /nonexistent.txt: {response.status_code}")
        assert response.status_code == 404
    
        # 3. index.html
        response = requests.get(f"{self.base_url}/")
        print(f"GET /: {response.status_code}")
        assert response.status_code == 200
    
    def test_head(self):
        """HEAD 메소드 테스트"""
        print("\n=== Testing HEAD ===")
        
        response = requests.head(f"{self.base_url}/test.txt")
        print(f"HEAD /test.txt: {response.status_code}")
        assert response.status_code == 200
        assert 'Content-Length' in response.headers
        assert 'Content-Type' in response.headers
        assert len(response.content) == 0  # HEAD는 본문이 없어야 함
    
    def test_post(self):
        """POST 메소드 테스트"""
        print("\n=== Testing POST ===")
        
        # 1. application/json
        json_data = {"name": "John", "age": 30}
        response = requests.post(
            f"{self.base_url}/api/test",
            json=json_data
        )
        print(f"POST (JSON): {response.status_code}")
        assert response.status_code == 200
        
        # 2. application/x-www-form-urlencoded
        form_data = {"username": "john", "password": "secret"}
        response = requests.post(
            f"{self.base_url}/api/login",
            data=form_data
        )
        print(f"POST (form): {response.status_code}")
        assert response.status_code == 200
        
        # 3. multipart/form-data
        files = {
            'file': ('test.txt', open(self.test_files_dir / 'test.txt', 'rb')),
        }
        response = requests.post(
            f"{self.base_url}/api/upload",
            files=files
        )
        print(f"POST (multipart): {response.status_code}")
        assert response.status_code == 200
    
    def test_put(self):
        """PUT 메소드 테스트"""
        print("\n=== Testing PUT ===")
        
        # 파일 업로드
        content = "This is a test file content"
        response = requests.put(
            f"{self.base_url}/uploaded.txt",
            data=content,
            headers={'Content-Type': 'text/plain'}
        )
        print(f"PUT /uploaded.txt: {response.status_code}")
        assert response.status_code == 201
        
        # 업로드된 파일 확인
        response = requests.get(f"{self.base_url}/uploaded.txt")
        assert response.status_code == 200
        assert response.text == content
    
    def test_delete(self):
        """DELETE 메소드 테스트"""
        print("\n=== Testing DELETE ===")
        
        # 1. 존재하는 파일 삭제
        response = requests.delete(f"{self.base_url}/uploaded.txt")
        print(f"DELETE /uploaded.txt: {response.status_code}")
        assert response.status_code == 200
        
        # 삭제 확인
        response = requests.get(f"{self.base_url}/uploaded.txt")
        assert response.status_code == 404
        
        # 2. 존재하지 않는 파일 삭제 시도
        response = requests.delete(f"{self.base_url}/nonexistent.txt")
        print(f"DELETE /nonexistent.txt: {response.status_code}")
        assert response.status_code == 404

def run_tests():
    tester = WebServerTest()
    try:
        tester.setup()
        
        # 모든 테스트 실행
        tests = [
            tester.test_get,
            tester.test_head,
            tester.test_post,
            tester.test_put,
            tester.test_delete
        ]
        
        success = 0
        failed = 0
        
        for test in tests:
            try:
                test()
                success += 1
                print(f"✅ {test.__name__} passed")
            except Exception as e:
                failed += 1
                print(f"❌ {test.__name__} failed: {str(e)}")
        
        print(f"\n=== Test Results ===")
        print(f"Total: {len(tests)}")
        print(f"Success: {success}")
        print(f"Failed: {failed}")
        
    finally:
        tester.cleanup()

if __name__ == "__main__":
    run_tests()