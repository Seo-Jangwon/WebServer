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
#include "http_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ws2tcpip.h>
#include <time.h>
#include <sys/stat.h>

#include "error_handle.h"

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

void optimize_socket(SOCKET socket) {
  // 송신 버퍼 크기 증가
  int send_buffer = SEND_BUFFER_SIZE;
  setsockopt(socket, SOL_SOCKET, SO_SNDBUF, (char *) &send_buffer, sizeof(send_buffer));

  // TCP_NODELAY 활성화
  BOOL nodelay = TRUE;
  setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (char *) &nodelay, sizeof(nodelay));

  // Keep-Alive 설정
  BOOL keepalive = TRUE;
  setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, (char *) &keepalive, sizeof(keepalive));

  // TCP Keep-Alive 파라미터 설정
  struct tcp_keepalive keepalive_vals = {
    .onoff = 1,
    .keepalivetime = TCP_KEEPALIVE_TIME * 1000,
    .keepaliveinterval = 1000
  };
  DWORD bytes_returned;
  WSAIoctl(socket,
           SIO_KEEPALIVE_VALS,
           &keepalive_vals,
           sizeof(keepalive_vals),
           NULL,
           0,
           &bytes_returned,
           NULL,
           NULL);
}

// 범위 요청 파싱
range_request *parse_range_header(const char *range_header, size_t file_size) {
  if (!range_header || strncmp(range_header, "bytes=", 6) != 0) {
    return NULL;
  }

  range_request *ranges = (range_request *) malloc(sizeof(range_request));
  if (!ranges) return NULL;

  ranges->count = 0;

  char *range_str = _strdup(range_header + 6); // Windows에서는 _strdup 사용
  char *token = strtok(range_str, ",");

  while (token && ranges->count < MAX_RANGE_PARTS) {
    // 앞뒤 공백 제거
    while (*token == ' ') token++;

    char *minus = strchr(token, '-');
    if (!minus) continue;

    range_part *part = &ranges->parts[ranges->count];

    if (token == minus) {
      // -500 형식 (마지막 500 바이트)
      part->start = file_size - atoll(minus + 1);
      part->end = file_size - 1;
    } else {
      *minus = '\0';
      part->start = atoll(token);
      if (*(minus + 1)) {
        part->end = atoll(minus + 1);
      } else {
        part->end = file_size - 1;
      }
    }

    // 범위 유효성 검사
    if (part->end >= file_size) {
      part->end = file_size - 1;
    }
    if (part->start > part->end || part->start >= file_size) {
      continue;
    }

    ranges->count++;
    token = strtok(NULL, ",");
  }

  free(range_str);

  if (ranges->count == 0) {
    free(ranges);
    return NULL;
  }

  return ranges;
}

void handle_static_file(SOCKET client_socket, const http_request *req, const char *request_path) {
  if (!g_server) {
    error_context err = MAKE_ERROR(ERR_INTERNAL_ERROR,
                                   "Server not initialized",
                                   "The server instance is not properly initialized");
    log_error(&err);
    send_error_response(client_socket, &err);
    return;
  }

  optimize_socket(client_socket);

  printf("\n=== Static File Request ===\n");
  printf("Request path: %s\n", request_path);

  const char *file_path = request_path;
  if (strcmp(request_path, "/") == 0 || strlen(request_path) == 0) {
    file_path = "/index.html";
  }

  file_result file = read_file(g_server->config.document_root, file_path);
  if (file.status_code != 200) {
    error_context err;
    if (file.status_code == 404) {
      err = MAKE_ERROR(ERR_NOT_FOUND,
                       "The requested file was not found",
                       file_path);
    } else {
      err = MAKE_ERROR(ERR_INTERNAL_ERROR,
                       "Failed to read file",
                       "Error occurred while reading the requested file");
    }
    log_error(&err);
    send_error_response(client_socket, &err);
    return;
  }

  // 전체 경로 생성
  char full_path[1024];
  snprintf(full_path,
           sizeof(full_path),
           "%s%c%s",
           g_server->config.document_root,
           PATH_SEPARATOR,
           file_path[0] == '/' ? file_path + 1 : file_path);

  // 마지막 수정 시간 확인
  struct stat file_stat;
  char last_modified[50] = {0};
  if (stat(full_path, &file_stat) == 0) {
    time_t gmt_time = file_stat.st_mtime;
    struct tm *gmt = gmtime(&gmt_time);
    if (gmt) {
      strftime(last_modified,
               sizeof(last_modified),
               "%a, %d %b %Y %H:%M:%S GMT",
               gmt);
    }
  }

  // If-Modified-Since 처리
  const char *if_modified = get_header_value(req, "If-Modified-Since");
  if (if_modified && last_modified[0] && strcmp(if_modified, last_modified) == 0) {
    const char *not_modified =
        "HTTP/1.1 304 Not Modified\r\n"
        "Cache-Control: public, max-age=86400\r\n"
        "\r\n";
    send(client_socket, not_modified, strlen(not_modified), 0);
    free_file_result(&file);
    return;
  }

  // Range 요청 처리
  const char *range_header = get_header_value(req, "Range");
  range_request *ranges = NULL;
  if (range_header) {
    ranges = parse_range_header(range_header, file.size);
  }

  // 성능 측정을 위한 타이머 초기화
  LARGE_INTEGER freq, start_time;
  QueryPerformanceFrequency(&freq);
  QueryPerformanceCounter(&start_time);

  int last_percent = -1;
  char header[1024];
  if (ranges && ranges->count > 0) {
    // 범위 요청 처리
    range_part *part = &ranges->parts[0];
    size_t content_length = part->end - part->start + 1;

    snprintf(header,
             sizeof(header),
             "HTTP/1.1 206 Partial Content\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %llu\r\n"
             "Content-Range: bytes %llu-%llu/%llu\r\n"
             "Cache-Control: public, max-age=86400\r\n"
             "Last-Modified: %s\r\n"
             "Accept-Ranges: bytes\r\n"
             "Connection: keep-alive\r\n"
             "X-Content-Type-Options: nosniff\r\n"
             "\r\n",
             file.content_type,
             (unsigned long long) content_length,
             (unsigned long long) part->start,
             (unsigned long long) part->end,
             (unsigned long long) file.size,
             last_modified);
  } else {
    // 전체 파일 요청
    snprintf(header,
             sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %llu\r\n"
             "Cache-Control: public, max-age=86400\r\n"
             "Last-Modified: %s\r\n"
             "Accept-Ranges: bytes\r\n"
             "Connection: keep-alive\r\n"
             "X-Content-Type-Options: nosniff\r\n"
             "\r\n",
             file.content_type,
             (unsigned long long) file.size,
             last_modified);
  }

  printf("\n=== Response Headers ===\n%s", header);
  send(client_socket, header, strlen(header), 0);

  // 파일 데이터 전송
  const char *current_pos = file.data;
  size_t remaining = file.size;
  size_t total_sent = 0;
  int retry_count = 0;
  const int MAX_RETRIES = 3;

  if (ranges && ranges->count > 0) {
    range_part *part = &ranges->parts[0];
    remaining = part->end - part->start + 1;
    current_pos += part->start;
  }

  while (remaining > 0) {
    size_t chunk_size = min(CHUNK_SIZE, remaining);
    size_t sent = 0;

    while (sent < chunk_size) {
      int result = send(client_socket,
                        current_pos + sent,
                        chunk_size - sent,
                        0);

      if (result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        char error_detail[256];
        snprintf(error_detail,
                 sizeof(error_detail),
                 "Socket error %d at position %zu",
                 error,
                 total_sent + sent);

        error_context err = MAKE_ERROR(ERR_SOCKET_ERROR,
                                       "Failed to send file data",
                                       error_detail);
        log_error(&err);

        if (error == WSAECONNRESET || error == WSAECONNABORTED) {
          if (retry_count < MAX_RETRIES) {
            fprintf(stderr,
                    "Connection reset, retrying (%d/%d)...\n",
                    retry_count + 1,
                    MAX_RETRIES);
            Sleep(1000);
            retry_count++;
            continue;
          } else {
            err.detail = "Maximum retry attempts reached";
            send_error_response(client_socket, &err);
            goto cleanup;
          }
        }

        if (error == WSAEWOULDBLOCK) {
          Sleep(1);
          continue;
        }
        goto cleanup;
      }

      sent += result;
      total_sent += result;
      retry_count = 0;

      // 진행률 및 속도 계산
      int current_percent = (int) ((total_sent * 100) /
        (ranges ? (ranges->parts[0].end - ranges->parts[0].start + 1) : file.size));

      if (current_percent != last_percent) {
        LARGE_INTEGER current_time;
        QueryPerformanceCounter(&current_time);
        double elapsed = (double) (current_time.QuadPart - start_time.QuadPart) / freq.QuadPart;
        double speed = elapsed > 0 ? (total_sent / (1024.0 * 1024.0)) / elapsed : 0;

        printf("\rProgress: %3d%% (%zu/%zu bytes), %.2f MB/s",
               current_percent,
               total_sent,
               ranges ? (ranges->parts[0].end - ranges->parts[0].start + 1) : file.size,
               speed);
        fflush(stdout);
        last_percent = current_percent;
      }
    }
    current_pos += chunk_size;
    remaining -= chunk_size;
  }

  printf("\nTransfer completed: %zu bytes sent\n", total_sent);

cleanup:
  if (ranges) free(ranges);
  free_file_result(&file);
}
