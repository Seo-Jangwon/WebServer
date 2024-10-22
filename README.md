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

## 프로젝트 구조

```
webserver/
├── src/
│   ├── main.c
│   ├── http_parser.c
│   ├── connection.c
│   └── ...
├── include/
│   ├── http_parser.h
│   ├── connection.h
│   └── ...
├── tests/
├── docs/
└── Makefile
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

- [x]기본 TCP 서버 구현
- [ ]HTTP 요청 파싱
- [ ]정적 파일 제공
- [ ]기본 에러 처리

### Phase 2: 기능 확장

- [ ]HTTP 메소드 확장
- [ ]MIME 타입 처리
- [ ]로깅 시스템
- [ ]설정 파일 처리

### Phase 3: 성능 최적화

- [ ]비동기 I/O
- [ ]스레드 풀
- [ ]캐싱 구현
- [ ]메모리 최적화

### Phase 4: 보안 강화

- [ ]접근 제어
- [ ]입력 검증
- [ ]SSL/TLS 지원
- [ ]보안 헤더 구현

## 참고자료

- [RFC 2616 - HTTP/1.1](https://tools.ietf.org/html/rfc2616)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)
- [The Linux Programming Interface](http://man7.org/tlpi/)
