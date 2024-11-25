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

#include "error_handle.h"

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

  // 요청 라인 추출
  const char *line_end = strstr(raw_request, "\r\n");
  if (!line_end) {
    printf("Invalid HTTP request: No CRLF found\n");
    req.method = HTTP_UNKNOWN;
    return req;
  }

  size_t request_line_length = line_end - raw_request;
  if (request_line_length >= sizeof(req.path)) {
    printf("Request line too long\n");
    req.method = HTTP_UNKNOWN;
    return req;
  }

  char request_line[1024];
  memcpy(request_line, raw_request, request_line_length);
  request_line[request_line_length] = '\0';

  // 요청 라인 파싱
  char *method_str = strtok(request_line, " ");
  char *path = strtok(NULL, " ");
  char *version = strtok(NULL, " ");

  if (!method_str || !path || !version) {
    printf("Failed to parse request line\n");
    req.method = HTTP_UNKNOWN;
    return req;
  }

  // 메서드 설정
  req.method = parse_method(method_str);

  // 경로 설정 (원본 그대로 유지)
  strncpy(req.base_path, path, sizeof(req.base_path) - 1);

  // 버전 설정
  strncpy(req.version, version, sizeof(req.version) - 1);

  // 경로와 쿼리스트링 분리
  char *query = strchr(req.base_path, '?');
  if (query) {
    *query = '\0';
    strncpy(req.query_string, query + 1, sizeof(req.query_string) - 1);
    parse_query_string(&req, req.query_string);
  }

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
  // Content-Type 파싱
  const char *content_type = get_header_value(&req, "Content-Type");
  req.content_type_enum = parse_content_type(content_type);

  // POST/PUT 데이터 파싱
  if ((req.method == HTTP_POST || req.method == HTTP_PUT) && req.content_length > 0) {
    const char *body = strstr(raw_request, "\r\n\r\n");
    if (body) {
      body += 4;

      // raw body 저장
      req.raw_body = malloc(req.content_length + 1);
      memcpy(req.raw_body, body, req.content_length);
      req.raw_body[req.content_length] = '\0';
      req.raw_body_length = req.content_length;

      switch (req.content_type_enum) {
        case CONTENT_TYPE_FORM_URLENCODED:
          parse_post_data(&req, body);
          break;

        case CONTENT_TYPE_JSON:
          parse_json_body(&req, body);
          break;

        case CONTENT_TYPE_MULTIPART: {
          const char *boundary = strstr(content_type, "boundary=");
          if (boundary) {
            boundary += 9;
            parse_multipart_body(&req, body, boundary);
          }
          break;
        }

        default:
          break;
      }
    }
  }

  return req;
}

// 메모리 해제
void free_request_body(http_request *req) {
  if (!req) return;

  // JSON 필드 해제
  if (req->json_fields) {
    for (int i = 0; i < req->json_field_count; i++) {
      if (req->json_fields[i].value.type == JSON_STRING) {
        free(req->json_fields[i].value.string_value);
      }
    }
    free(req->json_fields);
  }

  // Multipart 파일 해제
  if (req->files) {
    for (int i = 0; i < req->file_count; i++) {
      free(req->files[i].data);
    }
    free(req->files);
  }

  // Raw body 해제
  free(req->raw_body);

  req->json_fields = NULL;
  req->files = NULL;
  req->raw_body = NULL;
  req->json_field_count = 0;
  req->file_count = 0;
  req->raw_body_length = 0;
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

// Content-Type 파싱
content_type_t parse_content_type(const char *content_type) {
  if (!content_type) return CONTENT_TYPE_NONE;

  if (strstr(content_type, "application/x-www-form-urlencoded"))
    return CONTENT_TYPE_FORM_URLENCODED;
  else if (strstr(content_type, "application/json"))
    return CONTENT_TYPE_JSON;
  else if (strstr(content_type, "multipart/form-data"))
    return CONTENT_TYPE_MULTIPART;

  return CONTENT_TYPE_UNKNOWN;
}

// JSON 파싱 (간단한 구현)
static void skip_whitespace(const char **ptr) {
  while (**ptr && isspace(**ptr)) (*ptr)++;
}

static char *parse_json_string(const char **ptr) {
  if (**ptr != '"') return NULL;
  (*ptr)++;

  const char *start = *ptr;
  while (**ptr && **ptr != '"') (*ptr)++;

  if (**ptr != '"') return NULL;

  size_t len = *ptr - start;
  char *str = malloc(len + 1);
  strncpy(str, start, len);
  str[len] = '\0';
  (*ptr)++;

  return str;
}

static json_value parse_json_value(const char **ptr) {
  json_value value = {0};
  skip_whitespace(ptr);

  if (**ptr == '"') {
    value.type = JSON_STRING;
    value.string_value = parse_json_string(ptr);
  } else if (**ptr == 't' && strncmp(*ptr, "true", 4) == 0) {
    value.type = JSON_BOOLEAN;
    value.boolean_value = 1;
    *ptr += 4;
  } else if (**ptr == 'f' && strncmp(*ptr, "false", 5) == 0) {
    value.type = JSON_BOOLEAN;
    value.boolean_value = 0;
    *ptr += 5;
  } else if (**ptr == 'n' && strncmp(*ptr, "null", 4) == 0) {
    value.type = JSON_NULL;
    *ptr += 4;
  } else if (isdigit(**ptr) || **ptr == '-') {
    value.type = JSON_NUMBER;
    char *end;
    value.number_value = strtod(*ptr, &end);
    *ptr = end;
  }

  return value;
}

int parse_json_body(http_request *req, const char *body) {
  if (!body || !*body) return 0;

  req->json_fields = malloc(sizeof(json_field) * MAX_POST_PARAMS);
  req->json_field_count = 0;

  const char *ptr = body;
  skip_whitespace(&ptr);

  if (*ptr != '{') return 0;
  ptr++;

  while (*ptr && *ptr != '}' && req->json_field_count < MAX_POST_PARAMS) {
    skip_whitespace(&ptr);

    // 키 파싱
    char *key = parse_json_string(&ptr);
    if (!key) break;

    skip_whitespace(&ptr);
    if (*ptr != ':') {
      free(key);
      break;
    }
    ptr++;

    // 값 파싱
    json_value value = parse_json_value(&ptr);

    // 필드 저장
    strncpy(req->json_fields[req->json_field_count].key, key, 255);
    req->json_fields[req->json_field_count].value = value;
    req->json_field_count++;

    free(key);

    skip_whitespace(&ptr);
    if (*ptr == ',') ptr++;
  }

  return 1;
}

// multipart/form-data 파싱
int parse_multipart_body(http_request *req, const char *body, const char *boundary) {
  if (!body || !boundary) return 0;

  req->files = malloc(sizeof(multipart_file) * MAX_POST_PARAMS);
  req->file_count = 0;

  char boundary_start[256];
  snprintf(boundary_start, sizeof(boundary_start), "--%s", boundary);

  const char *current = strstr(body, boundary_start);
  while (current && req->file_count < MAX_POST_PARAMS) {
    current += strlen(boundary_start);

    // Content-Disposition 헤더 찾기
    const char *disposition = strstr(current, "Content-Disposition: form-data;");
    if (!disposition) break;

    // 파일 이름 추출
    const char *filename_start = strstr(disposition, "filename=\"");
    if (filename_start) {
      filename_start += 10;
      const char *filename_end = strchr(filename_start, '"');
      if (filename_end) {
        size_t filename_len = filename_end - filename_start;
        strncpy(req->files[req->file_count].filename,
                filename_start,
                min(filename_len, 255));
      }
    }

    // Content-Type 찾기
    const char *content_type = strstr(current, "Content-Type: ");
    if (content_type) {
      content_type += 14;
      const char *content_type_end = strstr(content_type, "\r\n");
      if (content_type_end) {
        size_t content_type_len = content_type_end - content_type;
        strncpy(req->files[req->file_count].content_type,
                content_type,
                min(content_type_len, 127));
      }
    }

    // 파일 데이터 찾기
    const char *data_start = strstr(current, "\r\n\r\n");
    if (data_start) {
      data_start += 4;
      const char *data_end = strstr(data_start, boundary_start);
      if (data_end) {
        size_t data_size = data_end - data_start - 2; // -2 for \r\n
        req->files[req->file_count].data = malloc(data_size);
        memcpy(req->files[req->file_count].data, data_start, data_size);
        req->files[req->file_count].size = data_size;
        req->file_count++;
      }
    }

    current = strstr(current, boundary_start);
  }

  return 1;
}
