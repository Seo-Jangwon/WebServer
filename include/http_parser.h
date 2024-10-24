#ifndef WEBSERVER_HTTP_PARSER_H
#define WEBSERVER_HTTP_PARSER_H

#define MAX_HEADERS 50
#define MAX_QUERY_PARAMS 20
#define MAX_POST_PARAMS 20

// HTTP 메소드 정의
typedef enum {
  HTTP_GET,
  HTTP_POST,
  HTTP_HEAD,
  HTTP_PUT,
  HTTP_DELETE,
  HTTP_UNKNOWN
} http_method;

// HTTP 헤더 구조체
typedef struct {
  char name[256];
  char value[1024];
} http_header;

// 쿼리/POST 파라미터 구조체
typedef struct {
  char name[256];
  char value[1024];
} http_parameter;

// HTTP 요청 구조체
typedef struct {
  // 기본 요청 정보
  http_method method;
  char path[1024];
  char version[16];

  // 순수 경로와 쿼리스트링 분리
  char base_path[1024];
  char query_string[1024];

  // 헤더 정보
  http_header headers[MAX_HEADERS];
  int header_count;

  // 쿼리 파라미터
  http_parameter query_params[MAX_QUERY_PARAMS];
  int query_param_count;

  // POST 데이터
  http_parameter post_params[MAX_POST_PARAMS];
  int post_param_count;
  char content_type[256];
  long content_length;

  // 자주 사용하는 헤더 값들 빠른 접근용
  char host[256];
  char user_agent[1024];
  char accept[1024];
} http_request;

// 함수 선언
http_request parse_http_request(const char *raw_request);
const char *get_method_string(http_method method);
void print_http_request(const http_request *request);
const char *get_header_value(const http_request *request, const char *header_name);
const char *get_query_param(const http_request *request, const char *param_name);
const char *get_post_param(const http_request *request, const char *param_name);

#endif //WEBSERVER_HTTP_PARSER_H
