#include "config.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>

#ifdef _WIN32
#define PATH_SEPARATOR '\\'
#define DEFAULT_DOC_ROOT ".\\static"
#else
#define PATH_SEPARATOR '/'
#define DEFAULT_DOC_ROOT "./static"
#endif

server_config load_default_config(void) {
  server_config config = {
    .port = 8080,
    .buffer_size = 1024,
    .max_connections = 1000,
    .backlog_size = 5
  };

  char exe_path[1024] = {0};
  GetModuleFileName(NULL, exe_path, sizeof(exe_path));

  // exe_path에서 마지막 '\' 이후 제거 (bin 폴더)
  char *last_sep = strrchr(exe_path, '\\');
  if (last_sep) *last_sep = '\0';

  // 상위 디렉토리로 이동 (bin의 부모 디렉토리)
  last_sep = strrchr(exe_path, '\\');
  if (last_sep) *last_sep = '\0';

  // static 경로 설정
  snprintf(config.document_root,
           sizeof(config.document_root),
           "%s\\static",
           exe_path);

  strncpy(config.server_name, "C Web Server", sizeof(config.server_name) - 1);

  return config;
}

int validate_config(const server_config *config) {
  if (!config) return 0;

  // 포트 번호 체크
  if (config->port <= 0 || config->port > 65535) return 0;

  // 버퍼 크기 체크
  if (config->buffer_size < 1024) return 0;

  // 최대 연결 수 체크
  if (config->max_connections <= 0) return 0;

  // 대기열 크기 체크
  if (config->backlog_size <= 0) return 0;

  return 1;
}

void print_config(const server_config *config) {
  printf("\n=== Server configuration ===\n");
  printf("Port: %d\n", config->port);
  printf("Document Root: %s\n", config->document_root);
  printf("Buffer Size: %zu bytes\n", config->buffer_size);
  printf("Max Connections: %d\n", config->max_connections);
  printf("Backlog Size: %d\n", config->backlog_size);
  printf("Server Name: %s\n", config->server_name);
  printf("==========================\n\n");
}
