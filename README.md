# C Web Server Project

간단한 웹 서버를 C언어로 구현해보자.
</br>
기본적인 HTTP 프로토콜 처리부터 고성능 처리를 위한 다양한 최적화까지 단계적으로 구현해볼 예정임.

## 프로젝트 목표

- 기본적인 웹 서버의 동작 원리 이해
- C 언어를 활용한 시스템 프로그래밍 실습
- 네트워크 프로그래밍 및 동시성 처리 학습
- 효율적인 리소스 관리 방법 습득

## Core Components

### 1. Network Interface

#### Socket Programming

- Create TCP socket for listening (Port 80/443)
- Accept incoming connections
- Implement non-blocking I/O using select()/epoll
- Handle multiple client connections concurrently

#### Connection Management

- Connection pooling
- Keep-alive connections
- Connection timeout handling
- Maximum connection limit

### 2. HTTP Protocol Handler

#### Request Parsing

- Parse HTTP request line
- Parse HTTP headers
- Handle different HTTP methods (GET, POST, etc.)
- URL decoding
- Query string parsing

#### Response Generation

- Generate HTTP status line
- Set response headers
- Handle different status codes (200, 404, 500, etc.)
- Content-Length calculation
- Keep-Alive header handling

### 3. File System Interface

#### Static File Handling

- Root directory configuration
- MIME type detection
- File access permission checking
- Directory listing (optional)
- File reading and sending
- Range request handling

#### Resource Management

- File descriptor caching
- Memory mapped files
- Buffer management
- Temporary file handling

### 4. Server Configuration

#### Basic Settings

- Port number
- Document root
- Max connections
- Timeout values
- Buffer sizes

#### Security Settings

- Access control
- File permissions
- Directory traversal prevention
- Request size limits

### 5. Logging System

#### Error Logging

- Error message formatting
- Log level control
- File/console output
- Rotation policy

#### Access Logging

- Request logging
- Response status
- Timing information
- Client information

### 6. Performance Optimizations

#### Concurrency Model

- Process/Thread pool
- Event-driven architecture (epoll/kqueue)
- Worker process management

#### Caching

- File content caching
- Header caching
- Directory entry caching
- Cache invalidation

## 개발 환경

- 언어: C
- 컴파일러: GCC
- 운영체제: Linux/Unix 기반
- 빌드 도구: Make
- 개발 환경: CLion

## 프로젝트 구조

```
webserver/
├── include/
│   ├── config.h        (서버 설정)
│   ├── server.h        (서버 core)
│   ├── http_parser.h   (HTTP 파싱)
│   ├── file_handler.h  (파일 처리)
│   ├── error_handle.h  (에러 처리)
│   └── connection.h    (연결 관리)
├── src/
│   ├── main.c         (진입점)
│   ├── server.c       (서버 구현)
│   ├── config.c       (설정 관리)
│   ├── http_parser.c  (HTTP 파싱)
│   ├── file_handler.c (파일 처리)
│   ├── error_handle.c (에러 처리)
│   └── connection.c   (연결 관리)
├── static/            (정적 파일)
└── CMakeLists.txt
```

<!--
## 빌드 방법

```bash
make clean
make
```

## 실행 방법

```bash
./webserver
```

## 테스트

```bash
make test
```
-->

## 구현 계획

### Phase 1: 기본 기능 구현

- [x] 기본 TCP 서버 구현
- [x] HTTP 요청 파싱
- [x] 추가 파싱 (main.c todo 참고)
  - [x] 많은 HTTP 헤더 파싱
- [ ] 정적 파일 제공
  - [x] 파일 시스템 접근
  - [x] MIME 타입 감지
  - [x] 파일 캐싱
  - [x] 병목 개선 (청크단위 전송 구현)
  - [ ] 보안 (디렉토리 탐색 방지)
- [x] 기본 에러 처리

### Phase 2: 기능 확장

- [x] HTTP 메소드 확장
  <details>
  <summary>구현 내용 상세</summary>

  #### 1. 메소드별 구조체 및 enum 추가
  ```c
  // 지원하는 Content-Type
  typedef enum {
    CONTENT_TYPE_NONE,
    CONTENT_TYPE_FORM_URLENCODED,
    CONTENT_TYPE_JSON,
    CONTENT_TYPE_MULTIPART,
    CONTENT_TYPE_UNKNOWN
  } content_type_t;
  
  // JSON 값 타입
  typedef enum {
    JSON_STRING,
    JSON_NUMBER,
    JSON_BOOLEAN,
    JSON_NULL
  } json_value_type;
  
  // JSON 값
  typedef struct {
    json_value_type type;
    union {
      char *string_value;
      double number_value;
      int boolean_value;
    };
  } json_value;
  
  // JSON 키-값 쌍
  typedef struct {
    char key[256];
    json_value value;
  } json_field;
  
  // multipart 파일 정보
  typedef struct {
    char filename[256];
    char content_type[128];
    char *data;
    size_t size;
  } multipart_file;
  ```
  #### 2. POST 메소드 구현
  - form-urlencoded 데이터 파싱
  - JSON 파싱 및 처리
  - multipart/form-data (파일 업로드)

  ```c
  switch (req->content_type_enum) {
    case CONTENT_TYPE_FORM_URLENCODED:
      // form 데이터 파싱
      parse_post_data(&req, body);
      break;
    case CONTENT_TYPE_JSON:
      // JSON 파싱
      parse_json_body(&req, body);
      break;
    case CONTENT_TYPE_MULTIPART:
      // 파일 업로드 처리
      const char *boundary = strstr(content_type, "boundary=");
      if (boundary) {
        boundary += 9;
        parse_multipart_body(&req, body, boundary);
      }
      break;
    }
  ```
  #### 3. PUT 메소드 구현
  - 파일 업로드/덮어쓰기
  - 경로 검증
  - raw_body 처리
  ```c
  // PUT 요청 처리
  static void handle_put_request(SOCKET client_socket, http_request *req) {
    
    // ...
  
    // 상대 경로 정규화
    const char *relative_path = req->base_path;
    while (*relative_path == '/') relative_path++;

    // 경로 검증
    if (!is_path_safe(relative_path)) {
        send_json_response(client_socket, 400, "Bad Request", "Invalid path");
        return;
    }

    // ...
  
    // 파일 저장
    FILE *fp = fopen(full_path, "wb");
    
    // raw body를 파일에 쓰기
    size_t written = fwrite(req->raw_body, 1, req->raw_body_length, fp);
    fclose(fp);
  }

  ```
  #### 4. DELETE 메소드 구현
  - 파일 삭제 기능
  - 캐시 연동
  - 경로 보안 처리
  ```c
  static delete_result delete_file(const char *base_path, const char *request_path) {
    // 상대 경로 검증
    if (!is_path_safe(request_path)) {
      return DELETE_PATH_INVALID;
    }

    // 파일 삭제 시도
    if (remove(full_path) != 0) {
        return DELETE_ERROR;
    }
  
    return DELETE_SUCCESS;
  }
  
  // DELETE 요청 처리
  static void handle_delete_request(SOCKET client_socket, http_request *req) {
    delete_result result = delete_file(g_server->config.document_root, req->base_path);

    switch (result) {
      case DELETE_SUCCESS: {
        char detail[1024];
        snprintf(detail, sizeof(detail), "Successfully deleted file: %s", req->base_path);
        send_json_response(client_socket, 200, "OK", detail);

        // 캐시에서도 제거
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s%c%s", g_server->config.document_root, PATH_SEPARATOR, req->base_path[0] == '/' ? req->base_path + 1 : req->base_path);
        cache_remove(full_path);
        break;
      }             
      
      // ...  
  
    }
  }
  ```
  #### 5. 보안 개선
  ```c
  int is_path_safe(const char *path) {
      // 상위 디렉토리 접근 제한
      if (strstr(path, "..")) return 0;
      
      // 허용된 문자만 포함
      while (*cur) {
          if (!isalnum(*cur) && 
              *cur != '.' && 
              *cur != '-' && 
              *cur != '_' && 
              *cur != ' ') {
              return 0;
          }
          cur++;
      }
      return 1;
  }
  ```
  #### 6. 테스트 구현
  - 메소드별 테스트 케이스
  - 에러 케이스 검증
  </details>

- [ ] 로깅 시스템
- [ ] 설정 파일 처리

### Phase 3: 성능 최적화

- [ ] 비동기 I/O
- [ ] 스레드 풀
- [ ] 캐싱 구현
- [ ] 메모리 최적화

### Phase 4: 보안 강화

- [ ] 접근 제어
- [ ] 입력 검증
- [ ] SSL/TLS 지원
- [ ] 보안 헤더 구현

## 참고자료

- [RFC 2616 - HTTP/1.1](https://tools.ietf.org/html/rfc2616)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)
- [The Linux Programming Interface](http://man7.org/tlpi/)
