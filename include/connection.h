#ifndef CONNECTION_H
#define CONNECTION_H

#include <winsock2.h>

typedef struct {
  SOCKET socket; // 클라이언트 소켓
  struct sockaddr_in addr; // 클라이언트 주소
  char *buffer; // 요청 버퍼
  size_t buffer_size; // 버퍼 크기
} client_connection;

// 클라이언트 연결 초기화
client_connection *create_connection(SOCKET socket, struct sockaddr_in addr, size_t buffer_size);

// 요청 처리
void handle_connection(client_connection *conn);

// 연결 종료 및 정리
void close_connection(client_connection *conn);

#endif // CONNECTION_H
