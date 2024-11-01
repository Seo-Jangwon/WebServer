/*
 * HTTP 요청 파서
 * 1. HTTP 요청 메시지 파싱
 * 2. 메소드, 헤더, 경로 추출
 * 3. URL 파라미터 파싱
 * 4. POST 데이터 처리
 */

#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#define MAX_HEADERS 50
#define MAX_QUERY_PARAMS 20
#define MAX_POST_PARAMS 20
#include <stddef.h>

// HTTP 메소드
typedef enum {
  HTTP_GET,
  HTTP_POST,
  HTTP_HEAD,
  HTTP_PUT,
  HTTP_DELETE,
  HTTP_UNKNOWN
} http_method;

// HTTP 헤더
typedef struct {
  char name[256];
  char value[1024];
} http_header;

// URL/POST 파라미터
typedef struct {
  char name[256];
  char value[1024];
} http_parameter;

// 지원하는 Content-Type
typedef enum {
  CONTENT_TYPE_NONE,
  CONTENT_TYPE_FORM_URLENCODED,
  CONTENT_TYPE_JSON,
  CONTENT_TYPE_MULTIPART,
  CONTENT_TYPE_UNKNOWN
} content_type_t;

// JSON 값 타입
typedef enum {
  JSON_STRING,
  JSON_NUMBER,
  JSON_BOOLEAN,
  JSON_NULL
} json_value_type;

// JSON 값
typedef struct {
  json_value_type type;
  union {
    char *string_value;
    double number_value;
    int boolean_value;
  };
} json_value;

// JSON 키-값 쌍
typedef struct {
  char key[256];
  json_value value;
} json_field;

// multipart 파일 정보
typedef struct {
  char filename[256];
  char content_type[128];
  char *data;
  size_t size;
} multipart_file;

// HTTP 요청
typedef struct {
  // 기본 요청 정보
  http_method method;
  char path[1024];
  char version[16];

  // 경로 및 쿼리스트링
  char base_path[1024];
  char query_string[1024];

  // 헤더 정보
  http_header headers[MAX_HEADERS];
  int header_count;

  // URL 파라미터
  http_parameter query_params[MAX_QUERY_PARAMS];
  int query_param_count;

  // POST 데이터
  http_parameter post_params[MAX_POST_PARAMS];
  int post_param_count;
  char content_type[256];
  long content_length;

  // 자주 쓰는 헤더
  char host[256];
  char user_agent[1024];
  char accept[1024];

  // Content-Type
  content_type_t content_type_enum;

  // JSON 데이터
  json_field *json_fields;
  int json_field_count;

  // Multipart 파일
  multipart_file *files;
  int file_count;

  // Raw body
  char *raw_body;
  size_t raw_body_length;
} http_request;

// 요청 파싱
http_request parse_http_request(const char *raw_request);

// 유틸리티 함수들
const char *get_method_string(http_method method);
const char *get_header_value(const http_request *request, const char *header_name);
const char *get_query_param(const http_request *request, const char *param_name);
const char *get_post_param(const http_request *request, const char *param_name);
content_type_t parse_content_type(const char *content_type);
int parse_json_body(http_request *req, const char *body);
int parse_multipart_body(http_request *req, const char *body, const char *boundary);
void free_request_body(http_request *req);

// 디버깅
void print_http_request(const http_request *request);

#endif // HTTP_PARSER_H
