/*
 * HTTP 서버
 * 1. 서버 소켓 초기화 및 바인딩
 * 2. 클라이언트 연결 수락
 * 3. 요청 처리를 위한 연결 관리
 * 4. 에러 처리 및 로깅
 */

#include "server.h"
#include "config.h"
#include "connection.h"
#include "file_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ws2tcpip.h>

#ifdef _WIN32
#define PATH_SEPARATOR '\\'
#define STATIC_FILE_PATH "static"
#else
#define PATH_SEPARATOR '/'
#define STATIC_FILE_PATH "static"
#endif

// 전역 서버 인스턴스
http_server *g_server = NULL;

// 서버 초기화
int server_init(http_server *server) {
  g_server = server;
  WSADATA wsaData;

  // Windows 소켓 초기화
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    fprintf(stderr, "WSAStartup failed: %d\n", WSAGetLastError());
    return -1;
  }

  // 서버 소켓 생성
  server->socket = socket(AF_INET, SOCK_STREAM, 0);
  if (server->socket == INVALID_SOCKET) {
    fprintf(stderr, "Socket creation failed: %d\n", WSAGetLastError());
    WSACleanup();
    return -1;
  }

  // 서버 주소 설정
  struct sockaddr_in server_addr = {0};
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(server->config.port);

  // 소켓 바인딩
  if (bind(server->socket,
           (struct sockaddr *) &server_addr,
           sizeof(server_addr)) == SOCKET_ERROR) {
    fprintf(stderr, "Bind failed: %d\n", WSAGetLastError());
    closesocket(server->socket);
    WSACleanup();
    return -1;
  }

  return 0;
}

// 서버 시작
int server_start(http_server *server) {
  // 연결 대기 시작
  if (listen(server->socket, server->config.backlog_size) == SOCKET_ERROR) {
    fprintf(stderr, "Listen failed: %d\n", WSAGetLastError());
    return -1;
  }

  printf("Server started on port %d\n", server->config.port);
  printf("Document root: %s\n", server->config.document_root);

  server->running = 1;

  // 메인 서버 루프
  while (server->running) {
    client_connection *client = server_accept_client(server);
    if (client) {
      handle_connection(client);
      close_connection(client);
    }
  }

  return 0;
}

// 서버 중지
void server_stop(http_server *server) {
  server->running = 0;
  if (server->socket != INVALID_SOCKET) {
    closesocket(server->socket);
    server->socket = INVALID_SOCKET;
  }
  WSACleanup();
}

// 클라이언트 연결 수락
client_connection *server_accept_client(http_server *server) {
  struct sockaddr_in client_addr;
  int addr_len = sizeof(client_addr);

  SOCKET client_socket = accept(server->socket,
                                (struct sockaddr *) &client_addr,
                                &addr_len);

  if (client_socket == INVALID_SOCKET) {
    fprintf(stderr, "Accept failed: %d\n", WSAGetLastError());
    return NULL;
  }

  printf("New client connected from %s:%d\n",
         inet_ntoa(client_addr.sin_addr),
         ntohs(client_addr.sin_port));

  return create_connection(client_socket,
                           client_addr,
                           server->config.buffer_size);
}


void handle_static_file(SOCKET client_socket, const char *request_path) {
  if (!g_server) {
    fprintf(stderr, "Server not initialized\n");
    return;
  }

  printf("\n=== Static File Request ===\n");
  printf("Request path: %s\n", request_path);
  printf("Document root: %s\n", g_server->config.document_root);

  // 기본 경로인 경우 index.html로 처리
  const char *file_path = request_path;
  if (strcmp(request_path, "/") == 0 || strlen(request_path) == 0) {
    file_path = "/index.html";
  }

  // 파일 처리
  file_result file = read_file(g_server->config.document_root, file_path);

  printf("File operation result - Status: %d, Size: %zu\n",
         file.status_code,
         file.size);

  // 응답 헤더 생성
  char header[1024];
  if (file.status_code == 200) {
    snprintf(header,
             sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n",
             file.content_type ? file.content_type : "application/octet-stream",
             file.size);
  } else {
    const char *error_message = file.status_code == 404
                                  ? "404 Not Found"
                                  : "500 Internal Server Error";

    snprintf(header,
             sizeof(header),
             "HTTP/1.1 %d %s\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n",
             file.status_code,
             error_message,
             strlen(error_message));

    file.data = (char *) error_message;
    file.size = strlen(error_message);
  }

  // 응답 전송
  send(client_socket, header, strlen(header), 0);
  if (file.data) {
    send(client_socket, file.data, file.size, 0);
  }

  // 정리
  if (file.status_code == 200) {
    free_file_result(&file);
  }
}
