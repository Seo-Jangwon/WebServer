#include "connection.h"

#include <io.h>

#include "http_parser.h"
#include "file_handler.h"
#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "error_handle.h"

#ifdef _WIN32
#include <direct.h>  // for _mkdir
#define PATH_SEPARATOR '\\'
#else
#include <sys/stat.h>  // for mkdir
#define PATH_SEPARATOR '/'
#endif

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

// JSON 응답 생성 헬퍼
static void send_json_response(SOCKET client_socket,
                               int status_code,
                               const char *message,
                               const char *detail) {
  char response[4096];
  snprintf(response,
           sizeof(response),
           "HTTP/1.1 %d %s\r\n"
           "Content-Type: application/json\r\n"
           "Connection: close\r\n"
           "\r\n"
           "{"
           "\"status\":%d,"
           "\"message\":\"%s\"%s%s"
           "}",
           status_code,
           message,
           status_code,
           message,
           detail ? ",\"detail\":\"" : "",
           detail ? detail : "");

  send(client_socket, response, strlen(response), 0);
}

// POST 요청 처리
static void handle_post_request(SOCKET client_socket, http_request *req) {
  printf("\n=== Processing POST Request ===\n");
  printf("Content-Type: %s\n", get_header_value(req, "Content-Type"));

  char detail[1024] = {0};

  switch (req->content_type_enum) {
    case CONTENT_TYPE_FORM_URLENCODED: {
      // URL-encoded 폼 데이터 처리
      snprintf(detail,
               sizeof(detail),
               "Received %d form parameters",
               req->post_param_count);

      printf("Form parameters:\n");
      for (int i = 0; i < req->post_param_count; i++) {
        printf("  %s: %s\n",
               req->post_params[i].name,
               req->post_params[i].value);
      }
      break;
    }

    case CONTENT_TYPE_JSON: {
      // JSON 데이터 처리
      snprintf(detail,
               sizeof(detail),
               "Processed %d JSON fields",
               req->json_field_count);

      printf("JSON fields:\n");
      for (int i = 0; i < req->json_field_count; i++) {
        printf("  %s: ", req->json_fields[i].key);
        switch (req->json_fields[i].value.type) {
          case JSON_STRING:
            printf("%s (string)\n", req->json_fields[i].value.string_value);
            break;
          case JSON_NUMBER:
            printf("%f (number)\n", req->json_fields[i].value.number_value);
            break;
          case JSON_BOOLEAN:
            printf("%s (boolean)\n",
                   req->json_fields[i].value.boolean_value ? "true" : "false");
            break;
          case JSON_NULL:
            printf("null\n");
            break;
        }
      }
      break;
    }

    case CONTENT_TYPE_MULTIPART: {
      // 서버 인스턴스 체크
      if (!g_server) {
        send_json_response(client_socket, 500, "Internal Server Error", "Server not initialized");
        return;
      }

      // uploads 폴더 생성 (없는 경우)
      char uploads_dir[1024];
      snprintf(uploads_dir,
               sizeof(uploads_dir),
               "%s%cuploads",
               g_server->config.document_root,
               PATH_SEPARATOR);

#ifdef _WIN32
      _mkdir(uploads_dir);
#else
            mkdir(uploads_dir, 0777);
#endif

      // Multipart 폼 데이터 처리
      snprintf(detail,
               sizeof(detail),
               "Received %d files",
               req->file_count);

      printf("Files:\n");
      int success_count = 0;

      for (int i = 0; i < req->file_count; i++) {
        printf("  Filename: %s\n", req->files[i].filename);
        printf("  Content-Type: %s\n", req->files[i].content_type);
        printf("  Size: %zu bytes\n", req->files[i].size);

        // 파일 이름 검증
        if (!is_path_safe(req->files[i].filename)) {
          printf("  Invalid filename!\n");
          continue;
        }

        // 파일 저장 처리
        char filepath[2048];
        snprintf(filepath,
                 sizeof(filepath),
                 "%s%c%s",
                 uploads_dir,
                 PATH_SEPARATOR,
                 req->files[i].filename);

        FILE *fp = fopen(filepath, "wb");
        if (fp) {
          size_t written = fwrite(req->files[i].data, 1, req->files[i].size, fp);
          fclose(fp);

          if (written == req->files[i].size) {
            printf("  Saved to: %s\n", filepath);
            success_count++;
          } else {
            printf("  Failed to write file completely!\n");
            remove(filepath); // 실패한 파일 삭제
          }
        } else {
          printf("  Failed to create file!\n");
        }
      }

      // 세부 정보 업데이트
      snprintf(detail,
               sizeof(detail),
               "Successfully saved %d of %d files",
               success_count,
               req->file_count);
      break;
    }

    case CONTENT_TYPE_UNKNOWN:
      send_json_response(client_socket, 415, "Unsupported Media Type", NULL);
      return;

    case CONTENT_TYPE_NONE:
      send_json_response(client_socket, 400, "Bad Request", "Missing Content-Type header");
      return;
  }

  send_json_response(client_socket, 200, "OK", detail);
}

// PUT 요청 처리
static void handle_put_request(SOCKET client_socket, http_request *req) {
  printf("\n=== Processing PUT Request ===\n");
  printf("Path: %s\n", req->base_path);

  // 서버 인스턴스 체크
  if (!g_server) {
    send_json_response(client_socket, 500, "Internal Server Error", "Server not initialized");
    return;
  }

  // 상대 경로 정규화 (시작 슬래시 제거)
  const char *relative_path = req->base_path;
  while (*relative_path == '/') relative_path++;

  // 경로 검증
  if (!is_path_safe(relative_path)) {
    send_json_response(client_socket, 400, "Bad Request", "Invalid path");
    return;
  }

  // 전체 경로 생성
  char full_path[1024];
  snprintf(full_path,
           sizeof(full_path),
           "%s%c%s",
           g_server->config.document_root,
           PATH_SEPARATOR,
           relative_path);

  // 파일 저장
  FILE *fp = fopen(full_path, "wb");
  if (!fp) {
    send_json_response(client_socket, 500, "Internal Server Error", "Failed to create file");
    return;
  }

  // raw body를 파일에 쓰기
  size_t written = fwrite(req->raw_body, 1, req->raw_body_length, fp);
  fclose(fp);

  if (written != req->raw_body_length) {
    remove(full_path); // 실패시 파일 삭제
    send_json_response(client_socket, 500, "Internal Server Error", "Failed to write file");
    return;
  }

  char detail[256];
  snprintf(detail,
           sizeof(detail),
           "Successfully wrote %zu bytes to %s",
           written,
           relative_path);

  send_json_response(client_socket, 201, "Created", detail);
}

// 파일 삭제 헬퍼
static delete_result delete_file(const char *base_path, const char *request_path) {
  printf("\n=== Processing File Delete ===\n");
  printf("Base path: %s\n", base_path);
  printf("Request path: %s\n", request_path);

  // 상대 경로에서 시작 슬래시 제거
  while (*request_path == '/') request_path++;

  // 상대 경로 검증
  printf("\n=== Path Safety Check ===\n");
  printf("Checking relative path: %s\n", request_path);
  if (!is_path_safe(request_path)) {
    printf("Path security check failed\n");
    return DELETE_PATH_INVALID;
  }

  // 전체 경로 생성
  char full_path[1024];
  snprintf(full_path,
           sizeof(full_path),
           "%s%c%s",
           base_path,
           PATH_SEPARATOR,
           request_path);
  printf("Full path: %s\n", full_path);

  // 파일 존재 여부 확인
  struct stat file_stat;
  if (stat(full_path, &file_stat) != 0) {
    printf("File not found\n");
    return DELETE_FILE_NOT_FOUND;
  }

  // 디렉토리 삭제 방지
  if (S_ISDIR(file_stat.st_mode)) {
    printf("Cannot delete directory\n");
    return DELETE_ACCESS_DENIED;
  }

  // 파일 접근 권한 확인
  if (access(full_path, W_OK) != 0) {
    printf("Access denied\n");
    return DELETE_ACCESS_DENIED;
  }

  // 파일 삭제 시도
  if (remove(full_path) != 0) {
    printf("Delete failed: %s\n", strerror(errno));
    return DELETE_ERROR;
  }

  printf("File successfully deleted\n");
  return DELETE_SUCCESS;
}

// DELETE 요청 처리
static void handle_delete_request(SOCKET client_socket, http_request *req) {
  printf("\n=== Processing DELETE Request ===\n");
  printf("Target path: %s\n", req->base_path);

  // 서버 인스턴스 체크
  if (!g_server) {
    error_context err = MAKE_ERROR_DETAIL(ERR_INTERNAL_ERROR,
                                          "Server not initialized",
                                          "The server instance is not properly initialized");
    send_error_response(client_socket, &err);
    return;
  }

  delete_result result = delete_file(g_server->config.document_root, req->base_path);

  switch (result) {
    case DELETE_SUCCESS: {
      char detail[1024];
      snprintf(detail,
               sizeof(detail),
               "Successfully deleted file: %s",
               req->base_path);
      send_json_response(client_socket, 200, "OK", detail);

      // 캐시에서도 제거
      char full_path[1024];
      snprintf(full_path,
               sizeof(full_path),
               "%s%c%s",
               g_server->config.document_root,
               PATH_SEPARATOR,
               req->base_path[0] == '/' ? req->base_path + 1 : req->base_path);
      cache_remove(full_path);
      break;
    }

    case DELETE_FILE_NOT_FOUND:
      send_json_response(client_socket, 404, "Not Found", req->base_path);
      break;

    case DELETE_ACCESS_DENIED:
      send_json_response(client_socket, 403, "Forbidden", "Access denied");
      break;

    case DELETE_PATH_INVALID:
      send_json_response(client_socket, 400, "Bad Request", "Invalid path");
      break;

    case DELETE_ERROR:
    default:
      send_json_response(client_socket, 500, "Internal Server Error", strerror(errno));
      break;
  }
}

// HEAD 요청 처리
static void handle_head_request(SOCKET client_socket, http_request *req) {
  // GET과 동일한 헤더를 반환하지만 본문은 제외
  if (!g_server) {
    const char *response = "HTTP/1.1 500 Internal Server Error\r\n"
        "Connection: close\r\n"
        "\r\n";
    send(client_socket, response, strlen(response), 0);
    return;
  }

  file_result file = read_file(g_server->config.document_root, req->base_path);
  if (file.status_code != 200) {
    const char *response = "HTTP/1.1 404 Not Found\r\n"
        "Connection: close\r\n"
        "\r\n";
    send(client_socket, response, strlen(response), 0);
    free_file_result(&file);
    return;
  }

  char header[1024];
  snprintf(header,
           sizeof(header),
           "HTTP/1.1 200 OK\r\n"
           "Content-Type: %s\r\n"
           "Content-Length: %zu\r\n"
           "Connection: close\r\n"
           "\r\n",
           file.content_type,
           file.size);

  send(client_socket, header, strlen(header), 0);
  free_file_result(&file);
}

// handle_connection
void handle_connection(client_connection *conn) {
  printf("\n=== New Connection Started ===\n");
  printf("Buffer size: %zu\n", conn->buffer_size);
  printf("Client IP: %s\n", inet_ntoa(conn->addr.sin_addr));
  printf("Client Port: %d\n", ntohs(conn->addr.sin_port));

  // 요청 수신 전에 버퍼 초기화
  memset(conn->buffer, 0, conn->buffer_size);

  // 헤더 끝을 찾기 위한 변수들
  size_t total_received = 0;
  const char *header_end;
  int found_header_end = 0;

  printf("\n=== Receiving Request ===\n");
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
      printf("Found end of headers at position: %td\n", header_end - conn->buffer);
      break;
    }
  }

  if (!found_header_end) {
    printf("Could not find end of headers\n");
    return;
  }

  // Raw 요청 출력
  printf("\n=== Raw Request ===\n%s\n", conn->buffer);

  // 헤더 부분만 출력
  size_t header_length = header_end - conn->buffer + 4;
  char *headers = (char *) malloc(header_length + 1);
  if (headers) {
    memcpy(headers, conn->buffer, header_length);
    headers[header_length] = '\0';
    printf("\n=== Parsed Headers ===\n%s\n", headers);
    free(headers);
  }

  // HTTP 요청 파싱
  http_request req = parse_http_request(conn->buffer);
  print_http_request(&req);

  // 요청 메소드에 따른 처리
  switch (req.method) {
    case HTTP_GET:
      handle_static_file(conn->socket, &req, req.base_path);
      break;
    case HTTP_HEAD:
      handle_head_request(conn->socket, &req);
      break;
    case HTTP_POST:
      handle_post_request(conn->socket, &req);
      break;
    case HTTP_PUT:
      handle_put_request(conn->socket, &req);
      break;
    case HTTP_DELETE:
      handle_delete_request(conn->socket, &req);
      break;
    default:
      send_json_response(conn->socket,
                         405,
                         "Method Not Allowed",
                         "Supported methods: GET, HEAD, POST, PUT, DELETE");
      break;
  }
  free_request_body(&req);
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
