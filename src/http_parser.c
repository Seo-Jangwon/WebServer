#include "http_parser.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// HTTP 메소드를 문자열로
const char* get_method_string(http_method method) {
    switch (method) {
        case HTTP_GET:    return "GET";
        case HTTP_POST:   return "POST";
        case HTTP_HEAD:   return "HEAD";
        case HTTP_PUT:    return "PUT";
        case HTTP_DELETE: return "DELETE";
        default:         return "UNKNOWN";
    }
}

// HTTP 메소드 문자열 enum으로
static http_method parse_method(const char* method_str) {
    if (strcmp(method_str, "GET") == 0)    return HTTP_GET;
    if (strcmp(method_str, "POST") == 0)   return HTTP_POST;
    if (strcmp(method_str, "HEAD") == 0)   return HTTP_HEAD;
    if (strcmp(method_str, "PUT") == 0)    return HTTP_PUT;
    if (strcmp(method_str, "DELETE") == 0) return HTTP_DELETE;
    return HTTP_UNKNOWN;
}

// HTTP 요청 파싱
http_request parse_http_request(const char* raw_request) {
    http_request req = {0};
    char method_str[32] = {0};

    // 요청 라인 파싱
    sscanf(raw_request, "%s %s %s", method_str, req.path, req.version);
    req.method = parse_method(method_str);

    // 헤더 파싱
    const char* headers_start = strchr(raw_request, '\n') + 1;
    char header_line[1024];
    const char* current = headers_start;

    while (*current) {
        int i = 0;
        // 한 줄 읽기
        while (*current != '\r' && *current != '\n' && *current) {
            header_line[i++] = *current++;
        }
        header_line[i] = '\0';

        // 빈 줄이면 헤더 끝
        if (i == 0) break;

        // 헤더 스킵
        while (*current == '\r' || *current == '\n') current++;

        // 헤더 파싱
        char name[256], value[256];
        if (sscanf(header_line, "%[^:]: %[^\r\n]", name, value) == 2) {
            if (strcmp(name, "Host") == 0) {
                strncpy(req.host, value, sizeof(req.host) - 1);
            }
            else if (strcmp(name, "User-Agent") == 0) {
                strncpy(req.user_agent, value, sizeof(req.user_agent) - 1);
            }
        }
    }

    return req;
}

// HTTP 요청 정보 출력
void print_http_request(const http_request* req) {
    printf("=== HTTP Request ===\n");
    printf("Method: %s\n", get_method_string(req->method));
    printf("Path: %s\n", req->path);
    printf("Version: %s\n", req->version);
    printf("Host: %s\n", req->host);
    printf("User-Agent: %s\n", req->user_agent);
    printf("==================\n");
}