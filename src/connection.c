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

  conn->buffer = (char *) malloc(buffer_size);
  if (!conn->buffer) {
    free(conn);
    return NULL;
  }

  return conn;
}

void handle_connection(client_connection *conn) {
  // 버퍼 초기화
  memset(conn->buffer, 0, conn->buffer_size);

  // 요청 수신
  int received = recv(conn->socket, conn->buffer, conn->buffer_size - 1, 0);
  if (received <= 0) {
    return;
  }

  // HTTP 요청 파싱
  http_request req = parse_http_request(conn->buffer);
  print_http_request(&req);

  // GET 요청 처리
  if (req.method == HTTP_GET) {
    handle_static_file(conn->socket, req.base_path);
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
