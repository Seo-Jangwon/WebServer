// error_handler.h
#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include <winsock2.h>

// 에러 코드
typedef enum {
  ERR_NONE = 0,
  ERR_NOT_FOUND = 404,
  ERR_METHOD_NOT_ALLOWED = 405,
  ERR_REQUEST_TIMEOUT = 408,
  ERR_PAYLOAD_TOO_LARGE = 413,
  ERR_URI_TOO_LONG = 414,
  ERR_UNSUPPORTED_MEDIA = 415,
  ERR_INTERNAL_ERROR = 500,
  ERR_NOT_IMPLEMENTED = 501,
  ERR_SERVICE_UNAVAILABLE = 503,
  ERR_SOCKET_ERROR = 1001,
  ERR_MEMORY_ERROR = 1002,
  ERR_FILE_ERROR = 1003,
  ERR_ACCESS_DENIED = 403,
} error_code;

// 에러 컨텍스트
typedef struct {
  error_code code;
  const char *message;
  const char *detail;
  const char *file;
  int line;
} error_context;

// 에러 로깅
void log_error(const error_context *err);

// 에러 응답 생성
void send_error_response(SOCKET client_socket, const error_context *err);

// 에러 생성 매크로
#define MAKE_ERROR_DETAIL(code, msg, detail) \
((error_context){code, msg, detail, __FILE__, __LINE__})

#define LOG_ERROR(msg, detail) do { \
error_context err = MAKE_ERROR_DETAIL(ERR_FILE_ERROR, msg, detail); \
log_error(&err); \
} while(0)

#endif // ERROR_HANDLER_H
