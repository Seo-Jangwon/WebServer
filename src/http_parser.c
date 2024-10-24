/*
* HTTP 요청 파서 구현
 * 1. HTTP 요청 메시지 파싱
 * 2. 메소드, 헤더, 경로 추출
 * 3. URL 파라미터 파싱
 * 4. POST 데이터 처리
 */

#include "http_parser.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

// HTTP 메소드를 문자열로
const char *get_method_string(http_method method) {
  switch (method) {
    case HTTP_GET: return "GET";
    case HTTP_POST: return "POST";
    case HTTP_HEAD: return "HEAD";
    case HTTP_PUT: return "PUT";
    case HTTP_DELETE: return "DELETE";
    default: return "UNKNOWN";
  }
}

// HTTP 메소드 문자열을 enum으로
static http_method parse_method(const char *method_str) {
  if (strcmp(method_str, "GET") == 0) return HTTP_GET;
  if (strcmp(method_str, "POST") == 0) return HTTP_POST;
  if (strcmp(method_str, "HEAD") == 0) return HTTP_HEAD;
  if (strcmp(method_str, "PUT") == 0) return HTTP_PUT;
  if (strcmp(method_str, "DELETE") == 0) return HTTP_DELETE;
  return HTTP_UNKNOWN;
}

// URL 디코딩
static void url_decode(char *dst, const char *src) {
  char a, b;
  while (*src) {
    if (*src == '%' && ((a = src[1]) && (b = src[2])) &&
      isxdigit(a) && isxdigit(b)) {
      if (a >= 'a') a -= 'a' - 'A';
      if (a >= 'A') a -= ('A' - 10);
      else a -= '0';
      if (b >= 'a') b -= 'a' - 'A';
      if (b >= 'A') b -= ('A' - 10);
      else b -= '0';
      *dst++ = 16 * a + b;
      src += 3;
    } else if (*src == '+') {
      *dst++ = ' ';
      src++;
    } else {
      *dst++ = *src++;
    }
  }
  *dst = '\0';
}

// 쿼리 문자열 파싱
static void parse_query_string(http_request *req, const char *query) {
  char query_copy[1024];
  strncpy(query_copy, query, sizeof(query_copy) - 1);

  char *pair = strtok(query_copy, "&");
  while (pair && req->query_param_count < MAX_QUERY_PARAMS) {
    char *eq = strchr(pair, '=');
    if (eq) {
      *eq = '\0';
      strncpy(req->query_params[req->query_param_count].name,
              pair,
              sizeof(req->query_params[0].name) - 1);
      url_decode(req->query_params[req->query_param_count].value, eq + 1);
      req->query_param_count++;
    }
    pair = strtok(NULL, "&");
  }
}

// POST 데이터 파싱
static void parse_post_data(http_request *req, const char *body) {
  if (!body || !*body) return;

  // application/x-www-form-urlencoded 처리
  if (strstr(req->content_type, "application/x-www-form-urlencoded")) {
    char body_copy[4096];
    strncpy(body_copy, body, sizeof(body_copy) - 1);

    char *pair = strtok(body_copy, "&");
    while (pair && req->post_param_count < MAX_POST_PARAMS) {
      char *eq = strchr(pair, '=');
      if (eq) {
        *eq = '\0';
        strncpy(req->post_params[req->post_param_count].name,
                pair,
                sizeof(req->post_params[0].name) - 1);
        url_decode(req->post_params[req->post_param_count].value, eq + 1);
        req->post_param_count++;
      }
      pair = strtok(NULL, "&");
    }
  }
}

// HTTP 요청 파싱
http_request parse_http_request(const char *raw_request) {
  http_request req = {0};
  char method_str[32] = {0};
  char full_path[1024] = {0};

  // 요청 라인 파싱
  sscanf(raw_request, "%s %s %s", method_str, full_path, req.version);
  req.method = parse_method(method_str);

  // URL과 쿼리스트링 분리
  char *query = strchr(full_path, '?');
  if (query) {
    *query = '\0';
    strncpy(req.base_path, full_path, sizeof(req.base_path) - 1);
    strncpy(req.query_string, query + 1, sizeof(req.query_string) - 1);
    parse_query_string(&req, req.query_string);
  } else {
    strncpy(req.base_path, full_path, sizeof(req.base_path) - 1);
  }
  strncpy(req.path, full_path, sizeof(req.path) - 1);

  // 헤더 파싱
  const char *current = strchr(raw_request, '\n') + 1;
  char header_line[1024];

  // 헤더 끝(빈 줄)까지 반복
  while (*current && *current != '\r' && *current != '\n' &&
    req.header_count < MAX_HEADERS) {
    int i = 0;
    // 한 줄 읽기
    while (*current && *current != '\r' && *current != '\n') {
      header_line[i++] = *current++;
    }
    header_line[i] = '\0';

    // 빈 줄이면 헤더 끝
    if (i == 0) break;

    // 줄바꿈 문자 스킵
    while (*current == '\r' || *current == '\n') current++;

    // 헤더 파싱
    char *colon = strchr(header_line, ':');
    if (colon) {
      *colon = '\0';
      strncpy(req.headers[req.header_count].name,
              header_line,
              sizeof(req.headers[0].name) - 1);

      // 값 앞의 공백 제거
      const char *value = colon + 1;
      while (*value == ' ') value++;

      strncpy(req.headers[req.header_count].value,
              value,
              sizeof(req.headers[0].value) - 1);

      // 자주 사용하는 헤더 저장
      if (strcasecmp(header_line, "Host") == 0)
        strncpy(req.host, value, sizeof(req.host) - 1);
      else if (strcasecmp(header_line, "User-Agent") == 0)
        strncpy(req.user_agent, value, sizeof(req.user_agent) - 1);
      else if (strcasecmp(header_line, "Content-Type") == 0)
        strncpy(req.content_type, value, sizeof(req.content_type) - 1);
      else if (strcasecmp(header_line, "Content-Length") == 0)
        req.content_length = atol(value);
      else if (strcasecmp(header_line, "Accept") == 0)
        strncpy(req.accept, value, sizeof(req.accept) - 1);

      req.header_count++;
    }
  }

  // POST 데이터 파싱
  if (req.method == HTTP_POST && req.content_length > 0) {
    // 빈 줄 다음이 POST 데이터
    const char *body = strstr(current, "\r\n\r\n");
    if (body) {
      body += 4; // "\r\n\r\n" 건너뛰기
      parse_post_data(&req, body);
    }
  }

  return req;
}

// 헤더 값 검색
const char *get_header_value(const http_request *request, const char *header_name) {
  for (int i = 0; i < request->header_count; i++) {
    if (strcasecmp(request->headers[i].name, header_name) == 0) {
      return request->headers[i].value;
    }
  }
  return NULL;
}

// 쿼리 파라미터 값 검색
const char *get_query_param(const http_request *request, const char *param_name) {
  for (int i = 0; i < request->query_param_count; i++) {
    if (strcmp(request->query_params[i].name, param_name) == 0) {
      return request->query_params[i].value;
    }
  }
  return NULL;
}

// POST 파라미터 값 검색
const char *get_post_param(const http_request *request, const char *param_name) {
  for (int i = 0; i < request->post_param_count; i++) {
    if (strcmp(request->post_params[i].name, param_name) == 0) {
      return request->post_params[i].value;
    }
  }
  return NULL;
}

// HTTP 요청 정보 출력
void print_http_request(const http_request *req) {
  printf("=== HTTP Request ===\n");
  printf("Method: %s\n", get_method_string(req->method));
  printf("Path: %s\n", req->path);
  printf("Base Path: %s\n", req->base_path);
  printf("Query String: %s\n", req->query_string);
  printf("Version: %s\n", req->version);

  printf("\n=== Headers (%d) ===\n", req->header_count);
  for (int i = 0; i < req->header_count; i++) {
    printf("%s: %s\n", req->headers[i].name, req->headers[i].value);
  }

  if (req->query_param_count > 0) {
    printf("\n=== Query Parameters (%d) ===\n", req->query_param_count);
    for (int i = 0; i < req->query_param_count; i++) {
      printf("%s: %s\n", req->query_params[i].name, req->query_params[i].value);
    }
  }

  if (req->post_param_count > 0) {
    printf("\n=== POST Parameters (%d) ===\n", req->post_param_count);
    for (int i = 0; i < req->post_param_count; i++) {
      printf("%s: %s\n", req->post_params[i].name, req->post_params[i].value);
    }
  }

  printf("==================\n");
}
