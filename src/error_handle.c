#include "error_handle.h"

#include <stdio.h>
#include <time.h>

// HTTP 상태 코드에 따른 기본 메시지
static const char *get_status_text(error_code code) {
  switch (code) {
    case ERR_NOT_FOUND: return "Not Found";
    case ERR_METHOD_NOT_ALLOWED: return "Method Not Allowed";
    case ERR_REQUEST_TIMEOUT: return "Request Timeout";
    case ERR_PAYLOAD_TOO_LARGE: return "Payload Too Large";
    case ERR_URI_TOO_LONG: return "URI Too Long";
    case ERR_UNSUPPORTED_MEDIA: return "Unsupported Media Type";
    case ERR_INTERNAL_ERROR: return "Internal Server Error";
    case ERR_NOT_IMPLEMENTED: return "Not Implemented";
    case ERR_SERVICE_UNAVAILABLE: return "Service Unavailable";
    default: return "Unknown Error";
  }
}

// HTML 에러 페이지 생성
static void generate_error_page(char *buffer, size_t size, const error_context *err) {
  const char *detail_prefix = err->detail ? "<p>Details: " : "";
  const char *detail_text = err->detail ? err->detail : "";
  const char *detail_suffix = err->detail ? "</p>" : "";

  snprintf(buffer,
           size,
           "<!DOCTYPE html>\n"
           "<html>\n"
           "<head>\n"
           "    <title>Error %d - %s</title>\n"
           "    <style>\n"
           "        body { font-family: Arial, sans-serif; margin: 40px; }\n"
           "        .error-container { \n"
           "            border: 1px solid #ddd;\n"
           "            padding: 20px;\n"
           "            border-radius: 5px;\n"
           "            background-color: #f8f8f8;\n"
           "        }\n"
           "        .error-code { color: #d32f2f; }\n"
           "        .error-message { color: #666; }\n"
           "    </style>\n"
           "</head>\n"
           "<body>\n"
           "    <div class=\"error-container\">\n"
           "        <h1 class=\"error-code\">Error %d - %s</h1>\n"
           "        <p class=\"error-message\">%s</p>\n"
           "        %s%s%s\n"
           "    </div>\n"
           "</body>\n"
           "</html>",
           err->code,
           get_status_text(err->code),
           err->code,
           get_status_text(err->code),
           err->message,
           detail_prefix,
           detail_text,
           detail_suffix
  );
}

void send_error_response(SOCKET client_socket, const error_context *err) {
  char page_buffer[4096];
  generate_error_page(page_buffer, sizeof(page_buffer), err);

  char header[1024];
  snprintf(header,
           sizeof(header),
           "HTTP/1.1 %d %s\r\n"
           "Content-Type: text/html\r\n"
           "Content-Length: %zu\r\n"
           "Connection: close\r\n"
           "\r\n",
           err->code,
           get_status_text(err->code),
           strlen(page_buffer)
  );

  // 헤더와 에러 페이지 전송
  send(client_socket, header, strlen(header), 0);
  send(client_socket, page_buffer, strlen(page_buffer), 0);
}

void log_error(const error_context *err) {
  time_t now;
  time(&now);
  char timestamp[64];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

  FILE *log_file = fopen("server_error.log", "a");
  if (log_file) {
    fprintf(log_file,
            "[%s] Error %d: %s\n",
            timestamp,
            err->code,
            err->message);
    if (err->detail) {
      fprintf(log_file, "Detail: %s\n", err->detail);
    }
    fprintf(log_file, "Location: %s:%d\n\n", err->file, err->line);
    fclose(log_file);
  }

  // 콘솔에도 출력
  fprintf(stderr, "[%s] Error %d: %s\n", timestamp, err->code, err->message);
  if (err->detail) {
    fprintf(stderr, "Detail: %s\n", err->detail);
  }
  fprintf(stderr, "Location: %s:%d\n\n", err->file, err->line);
}
