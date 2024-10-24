/*
 * HTTP 서버 기능 정의
 * - 서버 초기화 및 시작/중지
 * - 연결 관리
 * - 요청 처리 라우팅
 */

#ifndef SERVER_H
#define SERVER_H

#include <winsock2.h>
#include "config.h"
#include "connection.h"

typedef struct {
  SOCKET socket; // 서버 소켓
  server_config config; // 서버 설정
  int running; // 서버 실행 상태
} http_server;

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

// 정적 파일 처리
void handle_static_file(SOCKET client_socket, const char *request_path);

#endif // SERVER_H
