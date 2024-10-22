// include/http_parser.h
#ifndef WEBSERVER_HTTP_PARSER_H
#define WEBSERVER_HTTP_PARSER_H

#include <string.h>

// HTTP 메소드 정의
typedef enum {
  HTTP_GET,
  HTTP_POST,
  HTTP_HEAD,
  HTTP_PUT,
  HTTP_DELETE,
  HTTP_UNKNOWN
} http_method;

// HTTP 요청 구조체
typedef struct {
  http_method method;    // HTTP 메소드
  char path[1024];       // 요청 경로
  char version[16];      // HTTP 버전
  char host[256];        // Host 헤더
  char user_agent[256];  // User-Agent 헤더
} http_request;

// 함수 선언
http_request parse_http_request(const char* raw_request);
const char* get_method_string(http_method method);
void print_http_request(const http_request* request);

#endif //WEBSERVER_HTTP_PARSER_H