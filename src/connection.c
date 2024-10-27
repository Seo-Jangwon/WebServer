#include "connection.h"
#include "http_parser.h"
#include "file_handler.h"
#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

client_connection *create_connection(SOCKET socket, struct sockaddr_in addr, size_t buffer_size) {
  client_connection *conn = (client_connection *) malloc(sizeof(client_connection));
  if (!conn) return NULL;

  conn->socket = socket;
  conn->addr = addr;
  conn->buffer_size = buffer_size;

  // 소켓 버퍼 크기 설정
  int rcvbuf = 65536; // 64KB
  int sndbuf = 65536; // 64KB
  setsockopt(socket, SOL_SOCKET, SO_RCVBUF, (char *) &rcvbuf, sizeof(rcvbuf));
  setsockopt(socket, SOL_SOCKET, SO_SNDBUF, (char *) &sndbuf, sizeof(sndbuf));

  // TCP_NODELAY 활성화
  BOOL nodelay = TRUE;
  setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (char *) &nodelay, sizeof(nodelay));

  conn->buffer = (char *) malloc(buffer_size);
  if (!conn->buffer) {
    free(conn);
    return NULL;
  }

  return conn;
}

void handle_connection(client_connection *conn) {
  // 요청 수신 전에 버퍼 초기화
  memset(conn->buffer, 0, conn->buffer_size);

  // 헤더 끝을 찾기 위한 변수들
  size_t total_received = 0;
  const char* header_end;
  int found_header_end = 0;

  // 헤더를 완전히 받을 때까지 반복
  while (total_received < conn->buffer_size - 1) {
    int received = recv(conn->socket,
                      conn->buffer + total_received,
                      conn->buffer_size - total_received - 1,
                      0);

    if (received <= 0) {
      printf("Connection closed or error occurred\n");
      return;
    }

    total_received += received;
    conn->buffer[total_received] = '\0';

    // \r\n\r\n을 찾아 헤더의 끝 확인
    header_end = strstr(conn->buffer, "\r\n\r\n");
    if (header_end) {
      found_header_end = 1;
      break;
    }
  }

  if (!found_header_end) {
    printf("Could not find end of headers\n");
    return;
  }

  // 헤더 부분만 출력
  size_t header_length = header_end - conn->buffer + 4;
  char* headers = (char*)malloc(header_length + 1);
  if (headers) {
    memcpy(headers, conn->buffer, header_length);
    headers[header_length] = '\0';
    printf("\n=== Incoming Request Headers ===\n%s\n", headers);
    free(headers);
  }

  // HTTP 요청 파싱
  http_request req = parse_http_request(conn->buffer);
  print_http_request(&req);

  // GET 요청 처리
  if (req.method == HTTP_GET) {
    handle_static_file(conn->socket, &req, req.base_path);
  } else {
    // 기타 메소드에 대한 기본 응답
    const char *response = "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Connection: close\r\n"
        "\r\n"
        "Method not implemented";
    send(conn->socket, response, strlen(response), 0);
  }
}

void close_connection(client_connection *conn) {
  if (conn) {
    if (conn->socket != INVALID_SOCKET) {
      closesocket(conn->socket);
    }
    free(conn->buffer);
    free(conn);
  }
}
