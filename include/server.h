/*
 * HTTP 서버 기능 정의
 * - 서버 초기화 및 시작/중지
 * - 연결 관리
 * - 요청 처리 라우팅
 */

#ifndef SERVER_H
#define SERVER_H

#define CHUNK_SIZE (64 * 1024)  // 64KB 청크 크기
#define SEND_BUFFER_SIZE (256 * 1024)  // 256KB 송신 버퍼
#define TCP_KEEPALIVE_TIME 60  // 60초 keepalive
#define MAX_RANGE_PARTS 10

#include <winsock2.h>
#include "config.h"
#include "connection.h"
#include "http_parser.h"

typedef struct {
  SOCKET socket; // 서버 소켓
  server_config config; // 서버 설정
  int running; // 서버 실행 상태
} http_server;

typedef struct {
  size_t start;
  size_t end;
} range_part; // 파일의 특정 부분

typedef struct {
  range_part parts[MAX_RANGE_PARTS];
  int count;
} range_request; // 파일의 특정 부분 요청

// 전역 서버 인스턴스
extern http_server *g_server;

// 서버 초기화
int server_init(http_server *server);

// 서버 시작
int server_start(http_server *server);

// 서버 중지
void server_stop(http_server *server);

// 새로운 클라이언트 연결 수락
client_connection *server_accept_client(http_server *server);

// 소켓 최적화
void optimize_socket(SOCKET socket);

// Range 헤더 파싱
range_request* parse_range_header(const char* range_header, size_t file_size);

// 정적 파일 처리 (Range 요청 지원)
void handle_static_file(SOCKET client_socket, const http_request* req, const char* request_path);

#endif // SERVER_H