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
} http_request;

// 요청 파싱
http_request parse_http_request(const char *raw_request);

// 유틸리티 함수들
const char *get_method_string(http_method method);
const char *get_header_value(const http_request *request, const char *header_name);
const char *get_query_param(const http_request *request, const char *param_name);
const char *get_post_param(const http_request *request, const char *param_name);

// 디버깅
void print_http_request(const http_request *request);

#endif // HTTP_PARSER_H
